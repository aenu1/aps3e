// SPDX-License-Identifier: GPL-2.0-only
#include <atomic>
#include <deque>
#include <mutex>
#include "aps3e_rp3_impl.h"
#include "emulator.h"
#include "emulator_aps3e.h"
#include "vkapi.h"
#include "vkutil.h"
LOG_CHANNEL(aps3e_log);
LOG_CHANNEL(sys_log, "SYS");

namespace ae{

    int boot_type;

    std::string boot_game_uri;//不使用
    std::string boot_game_path;
    int boot_game_fd;
    int boot_game_dec_key_fd=-1;

    ANativeWindow* window;
    int window_width;
    int window_height;

    std::string game_id;

    enum{
        STATUS_RUNNING=1,
        STATUS_STOPPED,
        STATUS_PAUSED,
        STATUS_REQUEST_PAUSE,
        STATUS_REQUEST_RESUME,
        STATUS_REQUEST_STOP,
    };

    pthread_mutex_t key_event_mutex;
    
    pthread_mutex_t emu_mutex;
    pthread_cond_t emu_cond;
    
    int emu_status=-1;
    
    //主线程任务队列(等价于Qt版的主线程事件循环分发)
    //rpcs3 内部大量使用 CallFromMainThread/BlockingCallFromMainThread 把任务投递到"主线程"执行,
    //若在当前调用线程同步执行(旧实现),会导致在持有 pad::g_pad_mutex 等锁的线程(如 Overlay Input Thread)
    //上同步执行 Emu.Restart/Stop,与其他线程互相等待造成死锁(Pad Thread too sleepy 卡死)。
    std::thread::id main_thr_id;
    std::mutex task_mutex;
    std::deque<std::function<void()>> task_queue;
    
    bool is_main_thread(){
        return std::this_thread::get_id()==main_thr_id;
    }
    
    void process_main_tasks(){
        while(true){
            std::function<void()> task;
            {
                std::lock_guard<std::mutex> lock(task_mutex);
                if(task_queue.empty())
                    break;
                task=std::move(task_queue.front());
                task_queue.pop_front();
            }
            task();
        }
    }
    
    void run_on_main(std::function<void()> task){
        if(is_main_thread()){
            //与Qt同线程直接连接(AutoConnection)行为一致,避免主线程自我投递造成死锁
            task();
            return;
        }
        std::lock_guard<std::mutex> lock(task_mutex);
        task_queue.push_back(std::move(task));
    }
    
    void init();
    bool boot_game();
    
    void main_thr(){
    
        main_thr_id=std::this_thread::get_id();
    
        std::string tid=[] {
            std::stringstream ss;
            ss<<std::this_thread::get_id();
            return ss.str();
        }();
        LOGW("new thr: %s",tid.c_str());
        pthread_mutex_init(&key_event_mutex, NULL);
    
        pthread_mutex_init(&emu_mutex, NULL);
        pthread_cond_init(&emu_cond, NULL);
    
        init();
    
        bool boot_ok=boot_game();
    
        emu_status=STATUS_RUNNING;
        while (true){
    
            //处理投递到主线程的任务(Restart/Stop/Resume等)
            process_main_tasks();
    
            if (emu_status == STATUS_REQUEST_RESUME) {
                pthread_mutex_lock(&emu_mutex);
                Emu.Resume();
                emu_status = STATUS_RUNNING;
                pthread_cond_signal(&emu_cond);
    
                pthread_mutex_unlock(&emu_mutex);
            }
            if (emu_status == STATUS_REQUEST_PAUSE) {
                pthread_mutex_lock(&emu_mutex);
                Emu.Pause(false,false);
                emu_status = STATUS_PAUSED;
                pthread_cond_signal(&emu_cond);
                pthread_mutex_unlock(&emu_mutex);
            }
            if (emu_status == STATUS_REQUEST_STOP) {
                pthread_mutex_lock(&emu_mutex);
                if(boot_ok) Emu.Kill();
                pthread_mutex_unlock(&emu_mutex);
    
                //持续消费主线程任务直到模拟完全停止(收尾任务在任务队列中执行)
                while(!Emu.IsStopped(true)){
                    process_main_tasks();
                    usleep(1000);
                }
                process_main_tasks();
    
                pthread_mutex_lock(&emu_mutex);
                emu_status = STATUS_STOPPED;
                pthread_cond_signal(&emu_cond);
                pthread_mutex_unlock(&emu_mutex);
                usleep(10);
                break;
            }
            usleep(10);
        }
    
        pthread_mutex_destroy(&key_event_mutex);
        pthread_mutex_destroy(&emu_mutex);
        pthread_cond_destroy(&emu_cond);
    }

    void key_event(int key_code,bool pressed,int value){
        pthread_mutex_lock(&key_event_mutex);
        {
            auto* pad_thr=g_fxo->try_get<named_thread<pad_thread>>();
            if(!pad_thr){
                LOGW("key_event: pad_thread not initialized");
                pthread_mutex_unlock(&key_event_mutex);
                return;
            }
            
            auto handlers = pad_thr->get_handlers();
            auto it = handlers.find(pad_handler::keyboard);
            if(it == handlers.end()){
                LOGW("key_event: keyboard handler not found");
                pthread_mutex_unlock(&key_event_mutex);
                return;
            }
            
            auto xx = it->second;
            if(!xx){
                LOGW("key_event: keyboard handler is null");
                pthread_mutex_unlock(&key_event_mutex);
                return;
            }
            
            std::shared_ptr<AndroidVirtualPadHandler> padh = std::dynamic_pointer_cast<AndroidVirtualPadHandler>(xx);
            if(!padh){
                LOGW("key_event: failed to cast to AndroidVirtualPadHandler");
                pthread_mutex_unlock(&key_event_mutex);
                return;
            }
            
            padh->Key(static_cast<u32>(key_code), static_cast<bool>(pressed), value);
        }
        pthread_mutex_unlock(&key_event_mutex);
    }
    bool is_running(){
        return emu_status==STATUS_RUNNING;
    }
    bool is_paused(){
        return emu_status==STATUS_PAUSED;
    }
    void pause(){
        pthread_mutex_lock(&emu_mutex);
        emu_status=STATUS_REQUEST_PAUSE;
        while(emu_status==STATUS_REQUEST_PAUSE)
            pthread_cond_wait(&emu_cond,&emu_mutex);
        pthread_mutex_unlock(&emu_mutex);
    }
    void resume(){
        pthread_mutex_lock(&emu_mutex);
        emu_status=STATUS_REQUEST_RESUME;
        while(emu_status==STATUS_REQUEST_RESUME)
            pthread_cond_wait(&emu_cond,&emu_mutex);
        pthread_mutex_unlock(&emu_mutex);
    }
    void quit(){
        pthread_mutex_lock(&emu_mutex);
        emu_status=STATUS_REQUEST_STOP;
        while(emu_status==STATUS_REQUEST_STOP)
            pthread_cond_wait(&emu_cond,&emu_mutex);
        pthread_mutex_unlock(&emu_mutex);
    }

    /** RPCS3 emulator has functions it desires to call from the GUI at times. Initialize them in here. */
    EmuCallbacks create_emu_cb()
    {
        EmuCallbacks callbacks{};

        callbacks.on_run    = [](bool /*start_playtime*/) {
            PRE("on_run");
        };

        callbacks.on_pause  = []() {
            PRE("on_pause");
        };
        callbacks.on_resume = []() {
            PRE("on_resume");
        };

        callbacks.update_emu_settings = [](){PRE("update_emu_settings");};
        callbacks.save_emu_settings = [](){PRE("save_emu_settings");};
        callbacks.init_kb_handler = [](){PRE("init_kb_handler");
            g_fxo->init<KeyboardHandlerBase, NullKeyboardHandler>(Emu.DeserialManager());
        };

        callbacks.init_mouse_handler = []()
        {
            PRE("init_mouse_handler");
#if 0
            mouse_handler handler = g_cfg.io.mouse;

		if (handler == mouse_handler::null)
		{
			switch (g_cfg.io.move)
			{
			case move_handler::mouse:
				LOGW("handler = mouse_handler::basic");
				break;
			case move_handler::raw_mouse:
				LOGW("handler = mouse_handler::raw");
				break;
			default:
				break;
			}
		}

		switch (handler)
		{
		case mouse_handler::null:
		{
			g_fxo->init<MouseHandlerBase, NullMouseHandler>(Emu.DeserialManager());
			break;
		}
		case mouse_handler::basic:
		{
			/*basic_mouse_handler* ret = g_fxo->init<MouseHandlerBase, basic_mouse_handler>(Emu.DeserialManager());
			ensure(ret);
			ret->moveToThread(get_thread());
			ret->SetTargetWindow(m_game_window);
			*/break;
		}
		case mouse_handler::raw:
		{
			g_fxo->init<MouseHandlerBase, raw_mouse_handler>(Emu.DeserialManager());
			break;
		}
		}
#else
            g_fxo->init<MouseHandlerBase, NullMouseHandler>(Emu.DeserialManager());
#endif
        };

        callbacks.init_pad_handler = [](std::string_view title_id)
        {
            PRE("init_pad_handler");
            aps3e_log.warning("init_pad_handler: title_id=%s", title_id);

            ensure(g_fxo->init<named_thread<pad_thread>>(nullptr, nullptr, title_id));
            qt_events_aware_op(0, [](){ return !!pad::g_started; });
        };

        callbacks.get_audio = []() -> std::shared_ptr<AudioBackend>
        {
            PRE("get_audio");
            std::shared_ptr<AudioBackend> result =std::make_shared<NullAudioBackend>();
            switch (g_cfg.audio.renderer.get())
            {
                case audio_renderer::null: result = std::make_shared<NullAudioBackend>(); break;
                case audio_renderer::cubeb: result = std::make_shared<CubebBackend>(); break;
#ifdef HAVE_FAUDIO
                    case audio_renderer::faudio: result = std::make_shared<FAudioBackend>(); break;
#endif
            }

            if (!result->Initialized())
            {
                PRE("!result->Initialized()");
                // Fall back to a null backend if something went wrong
                //sys_log.error("Audio renderer %s could not be initialized, using a Null renderer instead. Make sure that no other application is running that might block audio access (e.g. Netflix).", result->GetName());
                result = std::make_shared<NullAudioBackend>();
            }
            return result;
        };

        callbacks.get_audio_enumerator = [](u64 renderer) -> std::shared_ptr<audio_device_enumerator>
        {
            PRE("get_audio_enumerator");
            switch (static_cast<audio_renderer>(renderer))
            {
                case audio_renderer::null: return std::make_shared<null_enumerator>();
#ifdef _WIN32
                    case audio_renderer::xaudio: return std::make_shared<xaudio2_enumerator>();
#endif
                case audio_renderer::cubeb: return std::make_shared<cubeb_enumerator>();
#ifdef HAVE_FAUDIO
                    case audio_renderer::faudio: return std::make_shared<faudio_enumerator>();
#endif
                default: return std::make_shared<null_enumerator>();
            }
        };

        callbacks.get_image_info = [](const std::string& filename, std::string& sub_type, s32& width, s32& height, s32& orientation) -> bool
        {
            PRE("get_image_info");
            return false;
        };

        callbacks.get_scaled_image = [](const std::string& path, s32 target_width, s32 target_height, s32& width, s32& height, u8* dst, bool force_fit) -> bool
        {
            PRE("get_scaled_image");
            return false;
        };

        callbacks.resolve_path = [](std::string_view sv)
        {
            LOGW("resolve_path: %s",sv.data());
            if(sv.ends_with("/"sv)){
                sv.remove_suffix(1);
                return std::string{sv};
            }
            return std::string{sv};
            // May result in an empty string if path does not exist
            //return QFileInfo(QString::fromUtf8(sv.data(), static_cast<int>(sv.size()))).canonicalFilePath().toStdString();
        };

        callbacks.on_install_pkgs = [](const std::vector<std::string>& pkgs)
        {
            PRE("on_install_pkgs");
            bool result=true;
            /*if(pkgs[0][0]==':') {
                for(const auto& pkg : pkgs){
                    if(!ae::install_pkg(*Emu.GetIsoFs(),pkg)){
                        result=false;
                    }
                }
            }
            else{
                for (const auto& pkg : pkgs)
                {
                    if(!ae::install_pkg(pkg.c_str())){
                        result=false;
                    }
                }
            }*/
            return result;
        };

        callbacks.try_to_quit = [](bool force_quit, std::function<void()> on_exit) -> bool
        {
            PRE("try_to_quit");
            if (on_exit)
            {
                on_exit();
            }
            /*if (force_quit)
            {
                if (on_exit)
                {
                    on_exit();
                }

                quit();
                return true;
            }*/
            return true;
        };
        callbacks.call_from_main_thread = [](std::function<void()> func, atomic_t<u32>* wake_up)
        {
            run_on_main([func=std::move(func),wake_up]()
            {
                func();

                if (wake_up)
                {
                    *wake_up = true;
                    wake_up->notify_one();
                }
            });
        };

        callbacks.init_gs_render = [](utils::serial* ar)
        {
            PRE("init_gs_render");
            switch (g_cfg.video.renderer.get())
            {
                case video_renderer::null:
                {
                    LOGE("video_renderer::null");
                    break;
                }
                case video_renderer::vulkan:
                {
                    g_fxo->init<rsx::thread, named_thread<VKGSRender>>(ar);
                    break;
                }
            }
        };

        callbacks.close_gs_frame=[]() {
            PRE("close_gs_frame");
        };

        callbacks.get_camera_handler = []() -> std::shared_ptr<camera_handler_base>
        {
            PRE("get_camera_handler");
            // Use android camera handler for real camera support
            extern JavaVM* g_jvm;
            //return std::make_shared<android_camera_handler>(g_jvm);
            return std::make_shared<null_camera_handler>();
        };

        callbacks.get_music_handler = []() -> std::shared_ptr<music_handler_base>
        {
            PRE("get_music_handler");
            switch (g_cfg.audio.music.get())
            {
                case music_handler::null:
                {
                    return std::make_shared<null_music_handler>();
                }
                case music_handler::qt:
                {
                    return std::make_shared<null_music_handler>();
                }
            }
        };

        callbacks.get_msg_dialog                 = []() -> std::shared_ptr<MsgDialogBase> {
            PRE("get_msg_dialog");
            return std::make_shared<dummy_msg_dialog>(); };
        callbacks.get_osk_dialog                 = []() -> std::shared_ptr<OskDialogBase> {
            PRE("get_osk_dialog");
            return std::make_shared<dummy_osk_dialog>(); };
        callbacks.get_save_dialog                = []() -> std::unique_ptr<SaveDialogBase> {
            PRE("get_save_dialog");
            return std::make_unique<android_save_dialog>(); };
        callbacks.get_trophy_notification_dialog = []() -> std::unique_ptr<TrophyNotificationBase> {
            PRE("get_trophy_notification_dialog");
            return std::make_unique<android_trophy_notification>(); };
        callbacks.get_sendmessage_dialog=[]()->std::shared_ptr<SendMessageDialogBase>{
            PRE("get_sendmessage_dialog");
            return std::make_shared<dummy_send_message_dialog>();
        };
        callbacks.get_recvmessage_dialog=[]()->std::shared_ptr<RecvMessageDialogBase>{
            PRE("get_recvmessage_dialog");
            return std::make_shared<dummy_recv_message_dialog>();
        };
        callbacks.on_stop   = []() {
            PRE("on_stop");
        };
        callbacks.on_ready  = []() {
            PRE("on_ready");
        };
        callbacks.on_emulation_stop_no_response = [](std::shared_ptr<atomic_t<bool>> closed_successfully, int /*seconds_waiting_already*/)
        {
            PRE("on_emulation_stop_no_response");
        };

        callbacks.on_save_state_progress = [](std::shared_ptr<atomic_t<bool>>, stx::shared_ptr<utils::serial>, stx::atomic_ptr<std::string>*, std::shared_ptr<void>)
        {
            PRE("on_save_state_progress");
        };

        callbacks.enable_disc_eject  = [](bool) {
            PRE("enable_disc_eject");

        };
        callbacks.enable_disc_insert = [](bool) {
            PRE("enable_disc_insert");
        };

        callbacks.on_missing_fw = []() {
            PRE("on_missing_fw");
        };

        callbacks.handle_taskbar_progress = [](s32, s32) {
            PRE("handle_taskbar_progress");
        };

        callbacks.get_localized_string    = [](localized_string_id id, const char* args) -> std::string {
            PRE("get_localized_string");

            std::string str = ::localized_strings[static_cast<size_t>(id)];
            if(auto it=str.find("%0");it!=std::string::npos) {
                str.replace(it, 2, args);
            }
            return str;
        };
        /*callbacks.get_localized_u32string = [](localized_string_id, const char*) -> std::u32string {
            PRE("get_localized_u32string");
            return {}; };*/
        callbacks.get_localized_setting=[](const cfg::_base* setting, u32 index) -> std::string {
            // Return the raw value name of the enum entry. Qt translates these on
            // desktop; on Android there is no Qt translation system, so fall back to
            // the config value names (e.g. "Vulkan", "Auto"). Without this the
            // home-menu dropdown (spinner-like) widgets render empty text.
            if (setting)
            {
                const std::vector<std::string> list = setting->to_list();
                if (index < list.size())
                {
                    return list[index];
                }
            }
            return "";
        };

        callbacks.get_photo_path = [](std::string_view path) -> std::string {
            PRE("get_photo_path");
            return "";
        };

        callbacks.play_sound = [](const std::string&, std::optional<f32>){
            PRE("play_sound");

        };
        callbacks.add_breakpoint = [](u32 /*addr*/){
            PRE("add_breakpoint");

        };
        callbacks.display_sleep_control_supported = []() -> bool {
            PRE("display_sleep_control_supported");
            return false;
        };
        callbacks.enable_display_sleep = [](bool) {
            PRE("enable_display_sleep");
        };
        callbacks.check_microphone_permissions = []() {
            PRE("check_microphone_permissions");
        };
        callbacks.make_video_source = []() -> std::unique_ptr<video_source> {
            PRE("make_video_source");
            return std::make_unique<dummy_video_source>();
        };
        callbacks.enable_gamemode = [](bool) {
            PRE("enable_gamemode");
        };
        callbacks.get_database_config = [](const std::string&) -> std::string {
            PRE("get_database_config");
            return "";
        };
        return callbacks;
    }
    void init(){

        const char* enable_log=getenv("APS3E_ENABLE_LOG");
        if(enable_log&&strcmp(enable_log,"true")==0){
            static std::unique_ptr<logs::listener> log_file = logs::make_file_listener(std::string(getenv("APS3E_LOG_DIR"))+"/rp3_log.txt", 1024*1024*1024);
        }

        prctl(PR_SET_TIMERSLACK, 1, 0, 0, 0);

        // Initialize TSC freq (in case it isn't)
        static_cast<void>(utils::get_tsc_freq());

        // Initialize thread pool finalizer (on first use)
        static_cast<void>(named_thread("", [](int) {}));

        struct rlimit64 rlim;
        rlim.rlim_cur = 4096;
        rlim.rlim_max = 4096;

        if (setrlimit64(RLIMIT_NOFILE, &rlim) != 0)
        {
            LOGE("Failed to get max open file limit");
        }
        else{
            LOGW("RLIMIT_NOFILE: rlim_max 0x%x ,rlim_cur 0x%x",rlim.rlim_max,rlim.rlim_cur);
        }

        rlim.rlim_cur = RLIM_INFINITY;
        rlim.rlim_max = RLIM_INFINITY;
        if (setrlimit64(RLIMIT_MEMLOCK, &rlim) != 0)
        {
            LOGE("Failed to get RLIMIT_MEMLOCK size");
        }
        else{
            LOGW("RLIMIT_MEMLOCK: rlim_max 0x%x ,rlim_cur 0x%x",rlim.rlim_max,rlim.rlim_cur);
        }

        /*if(prlimit64(0,RLIMIT_MEMLOCK,NULL,&rlim)==0){
            LOGW("prlimit64(RLIMIT_MEMLOCK): rlim_max 0x%x ,rlim_cur 0x%x",rlim.rlim_max,rlim.rlim_cur);
        }*/

        EmuCallbacks callbacks=create_emu_cb();

        callbacks.get_gs_frame = [=]() -> std::unique_ptr<GSFrameBase>
        {
            PRE("get_gs_frame");
            return std::unique_ptr<android_gs_frame>(new android_gs_frame(window,window_width,window_height));
        };

        Emu.SetCallbacks(std::move(callbacks));
        Emu.SetHasGui(true);
        Emu.SetSupportedRenderers({video_renderer::vulkan, video_renderer::null});
        //Emu.SetDefaultRenderer(video_renderer::vulkan);
        Emu.Init();
    }

    bool boot_game(){

        //Emu.CallFromMainThread([=]()
        {
            Emu.SetForceBoot(true);

            const char* config_path=getenv("APS3E_CUSTOM_CONFIG_YAML_PATH");
            const cfg_mode config_mode = config_path?cfg_mode::custom:cfg_mode::database_config ;

            aps3e_log.warning("iso_fd: %d",boot_game_fd);
            const game_boot_result error =boot_type==BOOT_TYPE_WITH_PATH? Emu.BootGame(boot_game_path, game_id, true, config_mode, config_path?:"")
                                                             :Emu.BootISO(iso_device::virtual_device_name+"/PS3_GAME/USRDIR/EBOOT.BIN",game_id,boot_game_fd,boot_game_dec_key_fd,config_mode, config_path?:"");
            LOGW("game_boot_result %d",error);
            return error==game_boot_result::no_errors;
        }//);
    }

    //----------------------------------------------
    //util

    bool install_firmware(int fd){
        fs::file pup_f=fs::file::from_native_handle(fd);
        if (!pup_f)
        {
            //LOGE("Error opening PUP file %s (%s)", path);
            LOGE("Firmware installation failed: The selected firmware file couldn't be opened.");
            return false;
        }

        pup_object pup(std::move(pup_f));

        switch (pup.operator pup_error())
        {
            case pup_error::header_read:
            {
                LOGE("%s", pup.get_formatted_error().data());
                LOGE("Firmware installation failed: The provided file is empty.");
                return false;
            }
            case pup_error::header_magic:
            {
                LOGE("Error while installing firmware: provided file is not a PUP file.");
                LOGE("Firmware installation failed: The provided file is not a PUP file.");
                return false;
            }
            case pup_error::expected_size:
            {
                LOGE("%s", pup.get_formatted_error().data());
                LOGE("Firmware installation failed: The provided file is incomplete. Try redownloading it.");
                return false;
            }
            case pup_error::header_file_count:
            case pup_error::file_entries:
            case pup_error::stream:
            {
                std::string error = "Error while installing firmware: PUP file is invalid.";

                if (!pup.get_formatted_error().empty())
                {
                    fmt::append(error, "\n%s", pup.get_formatted_error().data());
                }

                LOGE("%s", error.data());
                LOGE("Firmware installation failed: The provided file is corrupted.");
                return false;
            }
            case pup_error::hash_mismatch:
            {
                LOGE("Error while installing firmware: Hash check failed.");
                LOGE("Firmware installation failed: The provided file's contents are corrupted.");
                return false;
            }
            case pup_error::ok: break;
        }

        fs::file update_files_f = pup.get_file(0x300);

        const usz update_files_size = update_files_f ? update_files_f.size() : 0;

        if (!update_files_size)
        {
            LOGE("Error while installing firmware: Couldn't find installation packages database.");
            LOGE("Firmware installation failed: The provided file's contents are corrupted.");
            return false;
        }

        fs::device_stat dev_stat{};
        if (!fs::statfs(g_cfg_vfs.get_dev_flash(), dev_stat))
        {
            LOGE("Error while installing firmware: Couldn't retrieve available disk space. ('%s')", g_cfg_vfs.get_dev_flash().data());
            LOGE("Firmware installation failed: Couldn't retrieve available disk space.");
            return false;
        }

        if (dev_stat.avail_free < update_files_size)
        {
            LOGE("Error while installing firmware: Out of disk space. ('%s', needed: %d bytes)", g_cfg_vfs.get_dev_flash().data(), update_files_size - dev_stat.avail_free);
            LOGE("Firmware installation failed: Out of disk space.");
            return false;
        }

        tar_object update_files(update_files_f);

        auto update_filenames = update_files.get_filenames();

        update_filenames.erase(std::remove_if(
                                       update_filenames.begin(), update_filenames.end(), [](const std::string& s) { return s.find("dev_flash_") == umax; }),
                               update_filenames.end());

        if (update_filenames.empty())
        {
            LOGE("Error while installing firmware: No dev_flash_* packages were found.");
            LOGE("Firmware installation failed: The provided file's contents are corrupted.");
            return false;
        }

        static constexpr std::string_view cur_version = "4.91";

        std::string version_string;

        if (fs::file version = pup.get_file(0x100))
        {
            version_string = version.to_string();
        }

        if (const usz version_pos = version_string.find('\n'); version_pos != umax)
        {
            version_string.erase(version_pos);
        }

        if (version_string.empty())
        {
            LOGE("Error while installing firmware: No version data was found.");
            LOGE("Firmware installation failed: The provided file's contents are corrupted.");
            return false;
        }

        LOGI("FIRMWARD VER %s",version_string.data());

        if (std::string installed = utils::get_firmware_version(); !installed.empty())
        {
            LOGW("Reinstalling firmware: old=%s, new=%s", installed.data(), version_string.data());
        }

        // Used by tar_object::extract() as destination directory
        g_fxo->reset();
        //g_cfg_vfs.from_default();

        vfs::mount("/dev_flash", g_cfg_vfs.get_dev_flash());

        for (const auto& update_filename : update_filenames)
        {
            auto update_file_stream = update_files.get_file(update_filename);

            if (update_file_stream->m_file_handler)
            {
                // Forcefully read all the data
                update_file_stream->m_file_handler->handle_file_op(*update_file_stream, 0, update_file_stream->get_size(umax), nullptr);
            }

            fs::file update_file = fs::make_stream(std::move(update_file_stream->data));

            SCEDecrypter self_dec(update_file);
            self_dec.LoadHeaders();
            self_dec.LoadMetadata(SCEPKG_ERK, SCEPKG_RIV);
            self_dec.DecryptData();

            auto dev_flash_tar_f = self_dec.MakeFile();
            if (dev_flash_tar_f.size() < 3)
            {
                LOGE("Error while installing firmware: PUP contents are invalid.");
                LOGE("Firmware installation failed: Firmware could not be decompressed");
                //progress = -1;
                return false;
            }

            tar_object dev_flash_tar(dev_flash_tar_f[2]);
            if (!dev_flash_tar.extract())
            {
                LOGE("Error while installing firmware: TAR contents are invalid. (package=%s)", update_filename.data());
                LOGE("The firmware contents could not be extracted."
                     "\nThis is very likely caused by external interference from a faulty anti-virus software."
                     "\nPlease add RPCS3 to your anti-virus\' whitelist or use better anti-virus software.");

                //progress = -1;
                return false;
            }
        }
        update_files_f.close();
        LOGW("install_firmware ok");
        return true;
    }
#if 0
    bool install_pkg(iso_fs& iso_fs, const std::string& path){

        std::deque<package_reader> readers;
        readers.emplace_back(iso_fs, path);

        std::deque<std::string> bootable_paths;

        package_install_result result = package_reader::extract_data(readers, bootable_paths);
        LOGW("install_pkg %d %s %s",result.error,result.version.expected.c_str(),result.version.found.c_str());
        return result.error == package_install_result::error_type::no_error;
    }
#endif
    bool install_pkg(const char* path){
        std::deque<package_reader> readers;
        readers.emplace_back(std::string(path));

        std::deque<std::string> bootable_paths;

        package_install_result result = package_reader::extract_data(readers, bootable_paths);
        LOGW("install_pkg %d %s %s",result.error,result.version.expected.c_str(),result.version.found.c_str());
        return result.error == package_install_result::error_type::no_error;

    }
    bool install_pkg(int pkg_fd){
        std::deque<package_reader> readers;
        readers.emplace_back("a.pkg",fs::file::from_native_handle(pkg_fd));

        std::deque<std::string> bootable_paths;

        package_install_result result = package_reader::extract_data(readers, bootable_paths);
        LOGW("install_pkg %d %s %s",result.error,result.version.expected.c_str(),result.version.found.c_str());
        return result.error == package_install_result::error_type::no_error;
        return true;

    }

    bool allow_eboot_decrypt(const fs::file& eboot_file){
        fs::file dec_eboot = decrypt_self(eboot_file);
        return dec_eboot?true:false;
    }

    bool install_edat(const fs::file& edat_f) {
        NPD_HEADER npd_header;
        EDAT_HEADER unused;

        if (edat_f.size() < 0x90) {
            LOGE("EDAT file is too small");
            return false;
        }
        read_npd_edat_header(&edat_f, npd_header, unused);
        if (memcmp(&npd_header.magic, "NPD\0", 4) != 0) {
            LOGE("Invalid NPD header");
            return false;
        }
        std::vector<uint8_t> edat_data(edat_f.size());
        edat_f.seek(0);
        edat_f.read(edat_data.data(), edat_data.size());

        const std::string user_id = "00000001";
        std::string edat_save_path = std::format("{}dev_hdd0/home/{}/exdata/{}.edat",
                                                 fs::get_config_dir(), user_id,
                                                 npd_header.content_id);
        //LOGI("Writing EDAT file to %s",edat_save_path.c_str());
        if (!fs::write_file(edat_save_path, fs::open_mode::create + fs::open_mode::trunc,
                            edat_data)) {
            LOGE("Failed to write EDAT file");
            return false;
        }
        return true;
    }

    class NotifyMsg:public MsgDialogBase{
        public:
        NotifyMsg()=default;
        ~NotifyMsg(){}

        void Create(const std::string& msg, const std::string& title) override
        {
        }
        void Close(bool success) override
        {
        }
        void SetMsg(const std::string& msg) override
        {
        }
        void ProgressBarSetMsg(u32 progressBarIndex, const std::string& msg) override
        {
        }
        void ProgressBarReset(u32 progressBarIndex) override
        {
        }
        void ProgressBarInc(u32 progressBarIndex, u32 delta) override
        {
        }
        void ProgressBarSetValue(u32 progressBarIndex, u32 value) override
        {
        }
        void ProgressBarSetLimit(u32 index, u32 limit) override
        {
        }
    };
    bool precompile_ppu_cache(const std::string& path,std::optional<int> fd) {

        //setenv("APS3E_ENABLE_LOG","true",1);
        /*ae::init();
        EmuCallbacks cbs=Emu.GetCallbacks();

        cbs.get_msg_dialog=[]()->std::shared_ptr<class MsgDialogBase>{
            return std::make_shared<NotifyMsg>();
        };
        Emu.SetCallbacks(std::move(cbs));
        return Emu.PrecompilePPUCache(path, fd);*/
        return true;
    }

    std::pair<std::string,bool> vk_lib_info(){
        const std::pair<std::string,bool> dedault_info={"libvulkan.so",false};
        const char* cfg_path=getenv("APS3E_GLOBAL_CONFIG_YAML_PATH");
        if(!cfg_path)
            return dedault_info;
        if(!std::filesystem::exists(cfg_path))
            return dedault_info;

        YAML::Node config_node = YAML::LoadFile(cfg_path);
        if(!config_node.IsDefined())
            return dedault_info;
        YAML::Node use_custom_driver=config_node["Video"]["Vulkan"]["Use Custom Driver"];
        if(!use_custom_driver.IsDefined())
            return dedault_info;
        if(!use_custom_driver.as<bool>())
            return dedault_info;

        YAML::Node custom_lib_path=config_node["Video"]["Vulkan"]["Custom Driver Library Path"];
        if(!custom_lib_path.IsDefined())
            return dedault_info;
        std::string custom_lib_path_str=custom_lib_path.as<std::string>();
        if(custom_lib_path_str.empty()||!std::filesystem::exists(custom_lib_path_str))
            return dedault_info;
        return {custom_lib_path_str,true};
    }

    std::string get_gpu_info(){
        std::pair<std::string,bool> lib_info=vk_lib_info();
        vk_load(lib_info.first.c_str(),lib_info.second);

        struct clean_t{
            std::vector<std::function<void()>> funcs;
            ~clean_t(){
                for(auto it=funcs.rbegin();it!=funcs.rend();it++){
                    (*it)();
                }
            }
        }clean;

        clean.funcs.push_back([](){
            vk_unload();
        });

        std::optional<VkInstance> inst=vk_create_instance("aps3e-gpu_info");
        if(!inst) {
            return "获取gpu信息失败";
        }

        clean.funcs.push_back([=](){
            vk_destroy_instance(*inst);
        });

        if(int count=vk_get_physical_device_count(*inst);count!=1) {

            if(count<1){
                return "获取gpu信息失败";
            }
            if(count>1){
                return "多个gpu!";
            }
        }
        if(auto pdev=vk_get_physical_device(*inst);pdev) {
            std::string gpu_name=vk_get_physical_device_properties(*pdev).deviceName;
            std::string gpu_vk_ver=[](uint32_t v) {
                std::ostringstream oss;
                oss << (v >> 22) << "." << ((v >> 12) & 0x3ff) << "." << (v & 0xfff);
                return oss.str();
            }(vk_get_physical_device_properties(*pdev).apiVersion);

            std::string gpu_ext=[&]() {
                std::ostringstream oss;
                for (auto ext : vk_get_physical_device_extension_properties(*pdev)) {
                    oss <<"    * " << ext.extensionName << "\n";
                }
                return oss.str();
            }();

            return "GPU [" + gpu_name +"(Vulkan: "+gpu_vk_ver+ ")]:\n" + gpu_ext;

        }
        return "获取gpu信息失败";

    }

    std::string get_cpu_info() {

        std::vector<core_info_t> core_info=cpu_get_core_info();
        std::string cpu_name=cpu_get_simple_info(core_info);
        std::string cpu_features=[&](){
            std::ostringstream oss;
            for(const auto& feature : core_info[0].features){
                oss <<"    * " << feature << "\n";
            }
            return oss.str();
        }();
        return "CPU [" + cpu_name + "]:\n" + cpu_features;
    }

}
