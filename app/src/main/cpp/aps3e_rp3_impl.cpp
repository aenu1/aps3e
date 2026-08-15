// SPDX-License-Identifier: GPL-2.0-only
#include "aps3e_rp3_impl.h"

LOG_CHANNEL(aps3e_log);
LOG_CHANNEL(sys_log, "SYS");

cfg_input_configurations g_cfg_input_configs;
std::string g_input_config_override;

extern void report_fatal_error(std::string_view _text, bool is_html = false, bool include_help_text = true)
{
    aps3e_log.fatal("%s",_text.data());
    LOGE("%s",_text.data());
}

bool is_input_allowed(){
    return true;
}
extern void check_microphone_permissions()
{
}
#ifndef __ANDROID__
void enable_display_sleep()
{
}

void disable_display_sleep()
{
}
#endif
namespace ae{
    bool is_main_thread();
    void process_main_tasks();
}
extern void qt_events_aware_op(int repeat_duration_ms, std::function<bool()> wrapped_op)
{
    ensure(wrapped_op);

    const bool on_main=ae::is_main_thread();

    {
        while (!wrapped_op())
        {
            //主线程没有独立事件循环,等待期间必须消费任务队列,
            //否则投递到主线程的任务(如 Restart 收尾/继续开机)永远无法执行
            if(on_main){
                ae::process_main_tasks();
            }

            if (repeat_duration_ms == 0)
            {
                std::this_thread::yield();
            }
            else if (thread_ctrl::get_current())
            {
                thread_ctrl::wait_for(repeat_duration_ms * 1000);
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(repeat_duration_ms));
            }
        }
    }
}
/*
extern std::shared_ptr<CPUDisAsm> make_disasm(const cpu_thread* cpu, shared_ptr<cpu_thread> handle)
{
    if (!handle)
    {
        switch (cpu->get_class())
        {
            case thread_class::ppu: handle = idm::get_unlocked<named_thread<ppu_thread>>(cpu->id); break;
            case thread_class::spu: handle = idm::get_unlocked<named_thread<spu_thread>>(cpu->id); break;
            default: break;
        }
    }

    std::shared_ptr<CPUDisAsm> result;

    switch (cpu->get_class())
    {
        case thread_class::ppu: result = std::make_shared<PPUDisAsm>(cpu_disasm_mode::interpreter, vm::g_sudo_addr); break;
        case thread_class::spu: result = std::make_shared<SPUDisAsm>(cpu_disasm_mode::interpreter, static_cast<const spu_thread*>(cpu)->ls); break;
        case thread_class::rsx: result = std::make_shared<RSXDisAsm>(cpu_disasm_mode::interpreter, vm::g_sudo_addr, 0, cpu); break;
        default: return result;
    }

    result->set_cpu_handle(std::move(handle));
    return result;
}*/
#if 1
template <>
void fmt_class_string<cheat_type>::format(std::string& out, u64 arg)
{
    format_enum(out, arg, [](cheat_type value)
    {
        switch (value)
        {
            case cheat_type::unsigned_8_cheat: return "Unsigned 8 bits";
            case cheat_type::unsigned_16_cheat: return "Unsigned 16 bits";
            case cheat_type::unsigned_32_cheat: return "Unsigned 32 bits";
            case cheat_type::unsigned_64_cheat: return "Unsigned 64 bits";
            case cheat_type::signed_8_cheat: return "Signed 8 bits";
            case cheat_type::signed_16_cheat: return "Signed 16 bits";
            case cheat_type::signed_32_cheat: return "Signed 32 bits";
            case cheat_type::signed_64_cheat: return "Signed 64 bits";
            case cheat_type::max: break;
        }

        return unknown;
    });
}
#endif
template <>
void fmt_class_string<std::chrono::sys_time<typename std::chrono::system_clock::duration>>::format(std::string& out, u64 arg)
{
PR;
//const std::time_t dateTime = std::chrono::system_clock::to_time_t(get_object(arg));
//out += date_time::fmt_time("%Y-%m-%dT%H:%M:%S", dateTime);
}

static const char* localized_string_keys[]={
#define WRAP(x) #x
#include "Emu/localized_string_id.inc"
#undef WRAP
};

std::string localized_strings[sizeof(localized_string_keys)/sizeof(localized_string_keys[0])];
__attribute__((constructor)) static void init_localized_strings()
{

    constexpr int n=sizeof(localized_string_keys)/sizeof(localized_string_keys[0]);
    for(int i=0;i<n;i++){
        const char* key_id = localized_string_keys[i];
        localized_strings[i]= getenv(key_id)?:"?";
    }
}

extern std::string rp3_get_config_dir(){
    return std::string(getenv("APS3E_DATA_DIR"))+"/config/";
}

extern std::string rp3_get_cache_dir(){
    return std::string (getenv("APS3E_DATA_DIR"))+"/cache/";
}

extern VkPhysicalDeviceLimits get_physical_device_limits(){
    static const VkPhysicalDeviceLimits r=[]{
        VkApplicationInfo appinfo = {};
        appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appinfo.pNext = nullptr;
        appinfo.pApplicationName = "aps3e-cfg-test";
        appinfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appinfo.pEngineName = "nul";
        appinfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appinfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo inst_create_info = {};
        inst_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        inst_create_info.pApplicationInfo = &appinfo;

        VkInstance inst;
        if (vkCreateInstance(&inst_create_info, nullptr, &inst)!= VK_SUCCESS) {
            __android_log_print(ANDROID_LOG_FATAL, LOG_TAG,"%s : %d",__func__,__LINE__);
            aps3e_log.fatal("%s : %d",__func__,__LINE__);
        }

        uint32_t physicalDeviceCount = 0;
        vkEnumeratePhysicalDevices(inst, &physicalDeviceCount, nullptr);
        std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
        vkEnumeratePhysicalDevices(inst, &physicalDeviceCount, physicalDevices.data());

        assert(physicalDeviceCount==1);

        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevices[0], &deviceProperties);

        vkDestroyInstance(inst, nullptr);

        return deviceProperties.limits;

    }();

    return r;
}

extern bool vk_limit_max_vertex_output_components_le_64(){
    return get_physical_device_limits().maxVertexOutputComponents<=64;
}
extern bool cfg_vertex_buffer_upload_mode_use_buffer_view(){
    static const bool r=[]{
        switch(g_cfg.video.vertex_buffer_upload_mode){
            case vertex_buffer_upload_mode::buffer_view:
                return true;
            case vertex_buffer_upload_mode::buffer:
                return false;
            case vertex_buffer_upload_mode::_auto:
                return get_physical_device_limits().maxTexelBufferElements>=64*1024*1024;//>=64M
        }
    }();
    return r;
}

extern const std::unordered_map<rsx::overlays::language_class,std::vector<std::string>>& cfg_font_files(){

    static const auto r=[]->std::unordered_map<rsx::overlays::language_class,std::vector<std::string>>{
        case_lab:
        switch(g_cfg.misc.font_file_selection) {
            case font_file_selection::from_firmware:
                return {
                        {rsx::overlays::language_class::default_, {"Arial.ttf","arial.ttf","/system/fonts/Roboto-Regular.ttf","SCE-PS3-VR-R-LATIN.TTF"}},
                        {rsx::overlays::language_class::cjk_base, {"SCE-PS3-SR-R-JPN.TTF","SCE-PS3-DH-R-CGB.TTF"}},
                        {rsx::overlays::language_class::hangul,   {"SCE-PS3-YG-R-KOR.TTF"}},
                };
            case font_file_selection::custom:
                std::string custom_font_file_path = g_cfg.misc.custom_font_file_path.to_string();
                if (!custom_font_file_path.empty() &&
                    std::filesystem::exists(custom_font_file_path))
                    return {
                            {rsx::overlays::language_class::default_, {custom_font_file_path}},
                            {rsx::overlays::language_class::cjk_base, {custom_font_file_path}},
                            {rsx::overlays::language_class::hangul,   {custom_font_file_path}}
                    };
                else {
                    g_cfg.misc.font_file_selection.set(font_file_selection::from_firmware);
                    goto case_lab;
                }
        };

        return {};
    }();

    return r;
}

#if 0
android_camera_handler::android_camera_handler(JavaVM *vm) : camera_handler_base()
{
    aps3e_log.notice("android_camera_handler created");

    jvm_ = vm;

    if (!jvm_)
    {
        aps3e_log.error("No Java VM available");
        m_state = camera_handler_state::closed;
        return;
    }

    if (!init_java_camera())
    {
        aps3e_log.error("Failed to initialize Java camera");
        m_state = camera_handler_state::closed;
    }
}

android_camera_handler::~android_camera_handler()
{
    aps3e_log.notice("android_camera_handler destroyed");
    Emu.BlockingCallFromMainThread([&]()
                                   {
                                       close_camera();
                                       reset();
                                   });
}
JNIEnv* android_camera_handler::get_env(JavaVM *vm){
    JNIEnv* env = nullptr;
    int status = vm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        vm->AttachCurrentThread(&env, nullptr);
        //pthread_cleanup_push([](void* arg) {
        //    jvm_->DetachCurrentThread();
        //}, nullptr);
    }
    return env;
}
bool android_camera_handler::init_java_camera()
{
    JNIEnv* env = get_env(jvm_);
    if (!env)
    {
        LOGE("Failed to get JNI environment");
        return false;
    }

    jclass camera_class = env->FindClass("aenu/aps3e/io/Camera");
    if (!camera_class)
    {
        LOGE("Failed to find AndroidCamera class");
        return false;
    }

    jmethodID constructor = env->GetMethodID(camera_class, "<init>", "(Landroid/content/Context;)V");
    if (!constructor)
    {
        LOGE("Failed to get AndroidCamera constructor");
        env->DeleteLocalRef(camera_class);
        return false;
    }

    jobject instance = env->NewObject(camera_class, constructor);//FIXME
    if (!instance)
    {
        LOGE("Failed to create AndroidCamera instance");
        env->DeleteLocalRef(camera_class);
        return false;
    }

    j_camera_instance_ = env->NewGlobalRef(instance);

    mth_open_ = env->GetMethodID(camera_class, "open_camera", "()V");
    mth_close_ = env->GetMethodID(camera_class, "close_camera", "()V");
    mth_start_ = env->GetMethodID(camera_class, "start_camera", "()V");
    mth_stop_ = env->GetMethodID(camera_class, "stop_camera", "()V");
    mth_set_resolution_ = env->GetMethodID(camera_class, "set_resolution", "(II)V");
    mth_set_format_ = env->GetMethodID(camera_class, "set_format", "(I)V");
    mth_set_mirrored_ = env->GetMethodID(camera_class, "set_mirrored", "(Z)V");

    env->DeleteLocalRef(instance);
    env->DeleteLocalRef(camera_class);

    if (!mth_open_ || !mth_close_ || !mth_start_ || !mth_stop_)
    {
        LOGE("Failed to get camera method IDs");
        cleanup_java_camera();
        return false;
    }

    LOGI("Android camera initialized successfully");
    return true;
}

void android_camera_handler::cleanup_java_camera()
{
    if (j_camera_instance_){
        JNIEnv* env = get_env(jvm_);
        env->DeleteGlobalRef(j_camera_instance_);
        j_camera_instance_ = nullptr;
    }
}

void android_camera_handler::reset()
{
    std::lock_guard<std::mutex> lock(frame_mutex_);
    frame_buffer_.clear();
    frame_width_ = 0;
    frame_height_ = 0;
    frame_number_ = 0;
    has_new_frame_ = false;
    cleanup_java_camera();
}

void android_camera_handler::open_camera()
{
    aps3e_log.notice("Opening android camera");

    if (!j_camera_instance_)
    {
        aps3e_log.error("Java camera not initialized");
        m_state = camera_handler_state::closed;
        return;
    }

    JNIEnv* env = get_env(jvm_);
    if (!env)
    {
        aps3e_log.error("Failed to get JNI environment");
        m_state = camera_handler_state::closed;
        return;
    }

    // Set resolution and format before opening
    if (mth_set_resolution_)
    {
        env->CallVoidMethod(j_camera_instance_, mth_set_resolution_, m_width, m_height);
    }

    if (mth_set_format_)
    {
        env->CallVoidMethod(j_camera_instance_, mth_set_format_, m_format);
    }

    if (mth_set_mirrored_)
    {
        env->CallVoidMethod(j_camera_instance_, mth_set_mirrored_, m_mirrored ? JNI_TRUE : JNI_FALSE);
    }

    if (mth_open_)
    {
        env->CallVoidMethod(j_camera_instance_, mth_open_);
    }

    m_state = camera_handler_state::open;
    aps3e_log.notice("Camera opened successfully");
}

void android_camera_handler::close_camera()
{
    aps3e_log.notice("Closing android camera");

    if (!j_camera_instance_)
    {
        m_state = camera_handler_state::closed;
        return;
    }

    JNIEnv* env = get_env(jvm_);
    if (!env)
    {
        m_state = camera_handler_state::closed;
        return;
    }

    if (mth_stop_ && m_state == camera_handler_state::running)
    {
        env->CallVoidMethod(j_camera_instance_, mth_stop_);
    }

    if (mth_close_)
    {
        env->CallVoidMethod(j_camera_instance_, mth_close_);
    }

    m_state = camera_handler_state::closed;
    aps3e_log.notice("Camera closed");
}

void android_camera_handler::start_camera()
{
    aps3e_log.notice("Starting android camera");

    if (!j_camera_instance_)
    {
        aps3e_log.error("Java camera not initialized");
        m_state = camera_handler_state::closed;
        return;
    }

    JNIEnv* env = get_env(jvm_);
    if (!env)
    {
        aps3e_log.error("Failed to get JNI environment");
        m_state = camera_handler_state::closed;
        return;
    }

    if (mth_start_)
    {
        env->CallVoidMethod(j_camera_instance_, mth_start_);
    }

    m_state = camera_handler_state::running;
    aps3e_log.notice("Camera started");
}

void android_camera_handler::stop_camera()
{
    aps3e_log.notice("Stopping android camera");

    if (!j_camera_instance_)
    {
        m_state = camera_handler_state::closed;
        return;
    }

    JNIEnv* env = get_env(jvm_);
    if (!env)
    {
        m_state = camera_handler_state::closed;
        return;
    }

    if (mth_stop_)
    {
        env->CallVoidMethod(j_camera_instance_, mth_stop_);
    }

    m_state = camera_handler_state::open;
    aps3e_log.notice("Camera stopped");
}

void android_camera_handler::set_format(s32 format, u32 bytesize)
{
    m_format = format;
    m_bytesize = bytesize;
    aps3e_log.trace("Set format: %d, bytesize: %d", format, bytesize);
}

void android_camera_handler::set_frame_rate(u32 frame_rate)
{
    m_frame_rate = frame_rate;
    aps3e_log.trace("Set frame rate: %d", frame_rate);
}

void android_camera_handler::set_resolution(u32 width, u32 height)
{
    m_width = width;
    m_height = height;
    aps3e_log.trace("Set resolution: %dx%d", width, height);
}

void android_camera_handler::set_mirrored(bool mirrored)
{
    m_mirrored = mirrored;
    aps3e_log.trace("Set mirrored: %d", mirrored);
}

u64 android_camera_handler::frame_number() const
{
    return frame_number_.load();
}

void android_camera_handler::on_frame_available(void* data, int width, int height, int format)
{
    if (!data || width <= 0 || height <= 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(frame_mutex_);

    size_t required_size = 0;
    /*
        * enum CellCameraFormat : s32
{
	CELL_CAMERA_FORMAT_UNKNOWN,
	CELL_CAMERA_JPG,
	CELL_CAMERA_RAW8,
	CELL_CAMERA_YUV422,
	CELL_CAMERA_RAW10,
	CELL_CAMERA_RGBA,
	CELL_CAMERA_YUV420,
	CELL_CAMERA_V_Y1_U_Y0,
	CELL_CAMERA_Y0_U_Y1_V = CELL_CAMERA_YUV422,
};
        * */
    switch (format)
    {
        case 1: // CELL_CAMERA_JPG
        case 5: // CELL_CAMERA_RGBA
            required_size = width * height * 4;
            break;
        case 2: // CELL_CAMERA_RAW8
            required_size = width * height;
            break;
        case 4: // CELL_CAMERA_YUV422
            required_size = width * height * 2;
            break;
        case 6: // CELL_CAMERA_YUV420
            required_size = width * height * 3 / 2;
            break;
        default:
            required_size = width * height * 4; // Default to RGBA
            break;
    }

    // Resize buffer if needed
    if (frame_buffer_.size() < required_size)
    {
        frame_buffer_.resize(required_size);
    }

    // Copy frame data
    std::memcpy(frame_buffer_.data(), data, required_size);

    frame_width_ = width;
    frame_height_ = height;
    frame_number_++;
    has_new_frame_ = true;
}

camera_handler_base::camera_handler_state android_camera_handler::get_image(
        u8* buf, u64 size, u32& width, u32& height, u64& frame_number, u64& bytes_read)
{
    width = 0;
    height = 0;
    frame_number = 0;
    bytes_read = 0;

    if (!j_camera_instance_)
    {
        aps3e_log.error("Camera not initialized");
        m_state = camera_handler_state::closed;
        return camera_handler_state::closed;
    }

    const camera_handler_state current_state = m_state;

    if (current_state == camera_handler_state::running)
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);

        if (has_new_frame_ && !frame_buffer_.empty())
        {
            size_t copy_size = std::min(static_cast<size_t>(size), frame_buffer_.size());
            std::memcpy(buf, frame_buffer_.data(), copy_size);

            width = frame_width_;
            height = frame_height_;
            frame_number = frame_number_.load();
            bytes_read = copy_size;

            has_new_frame_ = false;

            aps3e_log.trace("Frame delivered: %dx%d, frame=%llu, bytes=%llu",
                             width, height, frame_number, bytes_read);
        }
    }
    else
    {
        aps3e_log.warning("Camera not running (state=%d)", static_cast<int>(current_state));
    }

    return current_state;
}
#endif
enum class APS3E_VKC:u32{
    none=0,
    l,u,r,d,
    square,cross,circle,triangle,
    lsl,lsu,lsr,lsd,
    rsl,rsu,rsr,rsd,
    l1,l2,l3,
    r1,r2,r3,
    start,select,
    ps,
};

#define VKC(c) {static_cast<u32>(APS3E_VKC::c), #c}
#define VKC4(a,b,c,d) VKC(a),VKC(b),VKC(c),VKC(d),
#define VKC3(a,b,c) VKC(a),VKC(b),VKC(c),
#define VKC2(a,b) VKC(a),VKC(b),
#define VKC1(a) VKC(a),

const std::unordered_map<u32, std::string> mouse_list =
        {
                VKC1(none)
                VKC4(l,u,r,d)
                VKC4(square,cross,circle,triangle)
                VKC4(lsl,lsu,lsr,lsd)
                VKC4(rsl,rsu,rsr,rsd)
                VKC3(l1,l2,l3)
                VKC3(r1,r2,r3)
                VKC2(start,select)
                VKC1(ps)
        };


#undef VKC
#undef VKC1
#undef VKC2
#undef VKC3
#undef VKC4


#define VKC(c) {#c, static_cast<u32>(APS3E_VKC::c)}
#define VKC4(a,b,c,d) VKC(a),VKC(b),VKC(c),VKC(d),
#define VKC3(a,b,c) VKC(a),VKC(b),VKC(c),
#define VKC2(a,b) VKC(a),VKC(b),
#define VKC1(a) VKC(a),

const std::unordered_map<std::string, u32> mouse_list_r =
        {
                VKC1(none)
                VKC4(l,u,r,d)
                VKC4(square,cross,circle,triangle)
                VKC4(lsl,lsu,lsr,lsd)
                VKC4(rsl,rsu,rsr,rsd)
                VKC3(l1,l2,l3)
                VKC3(r1,r2,r3)
                VKC2(start,select)
                VKC1(ps)
        };

#undef VKCf
#undef VKC1
#undef VKC2
#undef VKC3
#undef VKC4


bool AndroidVirtualPadHandler::Init()
{
    const steady_clock::time_point now = steady_clock::now();
    m_last_mouse_move_left  = now;
    m_last_mouse_move_right = now;
    m_last_mouse_move_up    = now;
    m_last_mouse_move_down  = now;
    return true;
}

AndroidVirtualPadHandler::AndroidVirtualPadHandler()
        : PadHandlerBase(pad_handler::keyboard)
{
    init_configs();

    // set capabilities
    b_has_config = true;
}

void AndroidVirtualPadHandler::init_config(cfg_pad* cfg)
{
    if (!cfg) return;

    // Set default button mapping
#define VKC(c) ::at32(mouse_list, static_cast<u32>(APS3E_VKC::c))

    cfg->ls_left.def  = VKC(lsl);
    cfg->ls_down.def  = VKC(lsd);
    cfg->ls_right.def = VKC(lsr);
    cfg->ls_up.def    = VKC(lsu);
    cfg->rs_left.def  = VKC(rsl);
    cfg->rs_down.def  = VKC(rsd);
    cfg->rs_right.def = VKC(rsr);
    cfg->rs_up.def    = VKC(rsu);
    cfg->start.def    = VKC(start);
    cfg->select.def   = VKC(select);
    cfg->ps.def       = VKC(ps);
    cfg->square.def   = VKC(square);
    cfg->cross.def    = VKC(cross);
    cfg->circle.def   = VKC(circle);
    cfg->triangle.def = VKC(triangle);
    cfg->left.def     = VKC(l);
    cfg->down.def     = VKC(d);
    cfg->right.def    = VKC(r);
    cfg->up.def       = VKC(u);
    cfg->r1.def       = VKC(r1);
    cfg->r2.def       = VKC(r2);
    cfg->r3.def       = VKC(r3);
    cfg->l1.def       = VKC(l1);
    cfg->l2.def       = VKC(l2);
    cfg->l3.def       = VKC(l3);


    cfg->pressure_intensity_button.def = "";
    cfg->analog_limiter_button.def = "";

    cfg->lstick_anti_deadzone.def = 0;
    cfg->rstick_anti_deadzone.def = 0;
    cfg->lstickdeadzone.def       = 0;
    cfg->rstickdeadzone.def       = 0;
    cfg->ltriggerthreshold.def    = 0;
    cfg->rtriggerthreshold.def    = 0;
    cfg->lpadsquircling.def       = 8000;
    cfg->rpadsquircling.def       = 8000;

    // apply defaults
    cfg->from_default();
}

void AndroidVirtualPadHandler::Key(const u32 code, bool pressed, u16 value)
{
    if (!pad::g_enabled)
    {
        return;
    }

    value = Clamp0To255(value);

    for (auto& pad : m_pads_internal)
    {
        const auto register_new_button_value = [code, pressed, value](Button& btn) -> u16
        {
            // Make sure we keep this button pressed until all related keys are released.
            if (pressed)
            {
                btn.m_pressed_keys[code] = value;
            }
            else
            {
                btn.m_pressed_keys.erase(code);

                // Optimization: just skip the whole combo parsing if there are no keys pressed
                if (btn.m_pressed_keys.empty())
                {
                    return 0;
                }
            }

            u16 actual_value = 0;

            // Get the max value of all pressed keys for this DS3 button
            for (const std::set<u32>& key_codes : btn.m_key_combos)
            {
                if (key_codes.empty()) continue;

                // Every key in this combination has to be pressed for this button to be considered pressed
                u16 combo_value = 0;
                const bool combo_pressed = std::all_of(key_codes.cbegin(), key_codes.cend(), [&btn, &combo_value](u32 key_code)
                {
                    const auto it = btn.m_pressed_keys.find(key_code);
                    if (it == btn.m_pressed_keys.cend() || it->second == 0)
                    {
                        return false; // This combo button is not pressed, so the combo is not pressed either.
                    }

                    // Take minimum combo value. Otherwise we will always end up with the max value.
                    combo_value = (combo_value == 0) ? it->second : std::min(combo_value, it->second);
                    return true;
                });

                if (combo_pressed)
                {
                    actual_value = std::max(actual_value, combo_value);
                }
            }

            return actual_value;
        };

        // Find out if special buttons are pressed (introduced by RPCS3).
        // Activate the buttons here if possible since keys don't auto-repeat. This ensures that they are already pressed in the following loop.

        const auto update_special_button_press = [&pad, &register_new_button_value, code](s32 index)
        {
            if (index < 0) return;

            Button& pressure_intensity_button = pad.m_buttons[index];

            if (std::any_of(pressure_intensity_button.m_key_combos.cbegin(), pressure_intensity_button.m_key_combos.cend(), [code](const std::set<u32>& key_codes){ return key_codes.contains(code); }))
            {
                const u16 actual_value = register_new_button_value(pressure_intensity_button);

                pressure_intensity_button.m_pressed = actual_value > 0;
                pressure_intensity_button.m_value = actual_value;
            }
        };

        update_special_button_press(pad.m_pressure_intensity_button_index);

        const bool adjust_pressure = pad.get_pressure_intensity_button_active(m_pressure_intensity_toggle_mode, pad.m_player_id);
        const bool adjust_pressure_changed = pad.m_adjust_pressure_last != adjust_pressure;

        if (adjust_pressure_changed)
        {
            pad.m_adjust_pressure_last = adjust_pressure;
        }

        update_special_button_press(pad.m_analog_limiter_button_index);

        const bool analog_limiter_enabled = pad.get_analog_limiter_button_active(m_analog_limiter_toggle_mode, pad.m_player_id);
        const bool analog_limiter_changed = pad.m_analog_limiter_enabled_last != analog_limiter_enabled;
        const u32 l_stick_multiplier = analog_limiter_enabled ? m_l_stick_multiplier : 100;
        const u32 r_stick_multiplier = analog_limiter_enabled ? m_r_stick_multiplier : 100;

        if (analog_limiter_changed)
        {
            pad.m_analog_limiter_enabled_last = analog_limiter_enabled;
        }

        // Handle buttons
        for (usz i = 0; i < pad.m_buttons.size(); i++)
        {
            // Ignore special buttons
            if (static_cast<s32>(i) == pad.m_pressure_intensity_button_index ||
                static_cast<s32>(i) == pad.m_analog_limiter_button_index ||
                static_cast<s32>(i) == pad.m_orientation_reset_button_index)
                continue;

            Button& button = pad.m_buttons[i];

            bool update_button = true;

            if (std::none_of(button.m_key_combos.cbegin(), button.m_key_combos.cend(), [code](const std::set<u32>& key_codes){ return key_codes.contains(code); }))
            {
                // Handle pressure changes anyway
                update_button = adjust_pressure_changed;
            }
            else
            {
                button.m_actual_value = register_new_button_value(button);

                // to get the fastest response time possible we don't wanna use any lerp with factor 1
                if (button.m_analog)
                {
                    update_button = m_analog_lerp_factor >= 1.0f;
                }
                else if (button.m_trigger)
                {
                    update_button = m_trigger_lerp_factor >= 1.0f;
                }
            }

            if (!update_button)
                continue;

            if (button.m_actual_value > 0)
            {
                // Modify pressure if necessary if the button was pressed
                if (adjust_pressure)
                {
                    button.m_value = pad.m_pressure_intensity;
                }
                else if (m_pressure_intensity_deadzone > 0)
                {
                    button.m_value = NormalizeDirectedInput(button.m_actual_value, m_pressure_intensity_deadzone, 255);
                }
                else
                {
                    button.m_value = button.m_actual_value;
                }

                button.m_pressed = button.m_value > 0;
            }
            else
            {
                button.m_value = 0;
                button.m_pressed = false;
            }
        }

        // Handle sticks
        for (usz i = 0; i < pad.m_sticks.size(); i++)
        {
            AnalogStick& stick = pad.m_sticks[i];

            const bool is_left_stick = i < 2;

            const bool is_max = std::any_of(stick.m_key_combos_max.cbegin(), stick.m_key_combos_max.cend(), [code](const std::set<u32>& combo) { return combo.contains(code); });
            const bool is_min = std::any_of(stick.m_key_combos_min.cbegin(), stick.m_key_combos_min.cend(), [code](const std::set<u32>& combo) { return combo.contains(code); });

            if (!is_max && !is_min)
            {
                if (!analog_limiter_changed)
                    continue;

                // Update already pressed sticks
                const bool is_min_pressed = !stick.m_pressed_keys_min.empty();
                const bool is_max_pressed = !stick.m_pressed_keys_max.empty();

                const u32 stick_multiplier = is_left_stick ? l_stick_multiplier : r_stick_multiplier;

                const u16 actual_min_value = is_min_pressed ? MultipliedInput(255, stick_multiplier) : 255;
                const u16 normalized_min_value = std::ceil(actual_min_value / 2.0);

                const u16 actual_max_value = is_max_pressed ? MultipliedInput(255, stick_multiplier) : 255;
                const u16 normalized_max_value = std::ceil(actual_max_value / 2.0);

                m_stick_min[i] = is_min_pressed ? std::min<u8>(normalized_min_value, 128) : 0;
                m_stick_max[i] = is_max_pressed ? std::min<int>(128 + normalized_max_value, 255) : 128;
            }
            else
            {
                const u16 actual_value = pressed ? MultipliedInput(value, is_left_stick ? l_stick_multiplier : r_stick_multiplier) : value;
                u16 normalized_value = std::ceil(actual_value / 2.0);

                const auto register_new_stick_value = [&](std::map<u32, u16>& pressed_keys, bool is_max)
                {
                    // Make sure we keep this stick pressed until all related keys are released.
                    if (pressed)
                    {
                        pressed_keys[code] = normalized_value;
                    }
                    else
                    {
                        pressed_keys.erase(code);
                    }

                    // Get the min/max value of all pressed keys for this stick
                    for (const auto& [key, val] : pressed_keys)
                    {
                        normalized_value = is_max ? std::max(normalized_value, val) : std::min(normalized_value, val);
                    }
                };

                if (is_max)
                {
                    register_new_stick_value(stick.m_pressed_keys_max, true);

                    const bool is_max_pressed = !stick.m_pressed_keys_max.empty();

                    m_stick_max[i] = is_max_pressed ? std::min<int>(128 + normalized_value, 255) : 128;
                }

                if (is_min)
                {
                    register_new_stick_value(stick.m_pressed_keys_min, false);

                    const bool is_min_pressed = !stick.m_pressed_keys_min.empty();

                    m_stick_min[i] = is_min_pressed ? std::min<u8>(normalized_value, 128) : 0;
                }
            }

            m_stick_val[i] = m_stick_max[i] - m_stick_min[i];

            const f32 stick_lerp_factor = is_left_stick ? m_l_stick_lerp_factor : m_r_stick_lerp_factor;

            // to get the fastest response time possible we don't wanna use any lerp with factor 1
            if (stick_lerp_factor >= 1.0f)
            {
                stick.m_value = m_stick_val[i];
            }
        }
    }
}

void AndroidVirtualPadHandler::release_all_keys()
{
    for (auto& pad : m_pads_internal)
    {
        for (Button& button : pad.m_buttons)
        {
            button.m_pressed = false;
            button.m_value = 0;
            button.m_actual_value = 0;
        }

        for (usz i = 0; i < pad.m_sticks.size(); i++)
        {
            m_stick_min[i] = 0;
            m_stick_max[i] = 128;
            m_stick_val[i] = 128;
            pad.m_sticks[i].m_value = 128;
        }
    }

    m_keys_released = true;
}
#if 0
bool keyboard_pad_handler::eventFilter(QObject* target, QEvent* ev)
{
	if (!ev) [[unlikely]]
	{
		return false;
	}

	if (input::g_active_mouse_and_keyboard != input::active_mouse_and_keyboard::pad)
	{
		if (!m_keys_released)
		{
			release_all_keys();
		}
		return false;
	}

	m_keys_released = false;

	// !m_target is for future proofing when gsrender isn't automatically initialized on load.
	// !m_target->isVisible() is a hack since currently a guiless application will STILL inititialize a gsrender (providing a valid target)
	if (!m_target || !m_target->isVisible()|| target == m_target)
	{
		switch (ev->type())
		{
		case QEvent::KeyPress:
			keyPressEvent(static_cast<QKeyEvent*>(ev));
			break;
		case QEvent::KeyRelease:
			keyReleaseEvent(static_cast<QKeyEvent*>(ev));
			break;
		case QEvent::MouseButtonPress:
			mousePressEvent(static_cast<QMouseEvent*>(ev));
			break;
		case QEvent::MouseButtonRelease:
			mouseReleaseEvent(static_cast<QMouseEvent*>(ev));
			break;
		case QEvent::MouseMove:
			mouseMoveEvent(static_cast<QMouseEvent*>(ev));
			break;
		case QEvent::Wheel:
			mouseWheelEvent(static_cast<QWheelEvent*>(ev));
			break;
		case QEvent::FocusOut:
			release_all_keys();
			break;
		default:
			break;
		}
	}
	return false;
}

/* Sets the target window for the event handler, and also installs an event filter on the target. */
void keyboard_pad_handler::SetTargetWindow(QWindow* target)
{
	if (target != nullptr)
	{
		m_target = target;
		target->installEventFilter(this);
	}
	else
	{
		QApplication::instance()->installEventFilter(this);
		// If this is hit, it probably means that some refactoring occurs because currently a gsframe is created in Load.
		// We still want events so filter from application instead since target is null.
		input_log.error("Trying to set pad handler to a null target window.");
	}
}

void keyboard_pad_handler::processKeyEvent(QKeyEvent* event, bool pressed)
{
	if (!event) [[unlikely]]
	{
		return;
	}

	if (event->isAutoRepeat())
	{
		event->ignore();
		return;
	}

	const auto handle_key = [this, pressed, event]()
	{
		QStringList list = GetKeyNames(event);
		if (list.isEmpty())
			return;

		const bool is_num_key = list.removeAll("Num") > 0;
		const QString name = QString::fromStdString(GetKeyName(event, true));

		// TODO: Edge case: switching numlock keeps numpad keys pressed due to now different modifier

		// Handle every possible key combination, for example: ctrl+A -> {ctrl, A, ctrl+A}
		for (const QString& keyname : list)
		{
			// skip the 'original keys' when handling numpad keys
			if (is_num_key && !keyname.contains("Num"))
				continue;
			// skip held modifiers when handling another key
			if (keyname != name && list.count() > 1 && (keyname == "Alt" || keyname == "AltGr" || keyname == "Ctrl" || keyname == "Meta" || keyname == "Shift"))
				continue;
			Key(GetKeyCode(keyname), pressed);
		}
	};

	handle_key();
	event->ignore();
}

void keyboard_pad_handler::keyPressEvent(QKeyEvent* event)
{
	if (!event) [[unlikely]]
	{
		return;
	}

	if (event->modifiers() & Qt::AltModifier)
	{
		switch (event->key())
		{
		case Qt::Key_I:
			m_deadzone_y = std::min(m_deadzone_y + 1, 255);
			input_log.success("mouse move adjustment: deadzone y = %d", m_deadzone_y);
			event->ignore();
			return;
		case Qt::Key_U:
			m_deadzone_y = std::max(0, m_deadzone_y - 1);
			input_log.success("mouse move adjustment: deadzone y = %d", m_deadzone_y);
			event->ignore();
			return;
		case Qt::Key_Y:
			m_deadzone_x = std::min(m_deadzone_x + 1, 255);
			input_log.success("mouse move adjustment: deadzone x = %d", m_deadzone_x);
			event->ignore();
			return;
		case Qt::Key_T:
			m_deadzone_x = std::max(0, m_deadzone_x - 1);
			input_log.success("mouse move adjustment: deadzone x = %d", m_deadzone_x);
			event->ignore();
			return;
		case Qt::Key_K:
			m_multi_y = std::min(m_multi_y + 0.1, 5.0);
			input_log.success("mouse move adjustment: multiplier y = %d", static_cast<int>(m_multi_y * 100));
			event->ignore();
			return;
		case Qt::Key_J:
			m_multi_y = std::max(0.0, m_multi_y - 0.1);
			input_log.success("mouse move adjustment: multiplier y = %d", static_cast<int>(m_multi_y * 100));
			event->ignore();
			return;
		case Qt::Key_H:
			m_multi_x = std::min(m_multi_x + 0.1, 5.0);
			input_log.success("mouse move adjustment: multiplier x = %d", static_cast<int>(m_multi_x * 100));
			event->ignore();
			return;
		case Qt::Key_G:
			m_multi_x = std::max(0.0, m_multi_x - 0.1);
			input_log.success("mouse move adjustment: multiplier x = %d", static_cast<int>(m_multi_x * 100));
			event->ignore();
			return;
		default:
			break;
		}
	}
	processKeyEvent(event, true);
}

void keyboard_pad_handler::keyReleaseEvent(QKeyEvent* event)
{
	processKeyEvent(event, false);
}

void keyboard_pad_handler::mousePressEvent(QMouseEvent* event)
{
	if (!event) [[unlikely]]
	{
		return;
	}

	Key(event->button(), true);
	event->ignore();
}

void keyboard_pad_handler::mouseReleaseEvent(QMouseEvent* event)
{
	if (!event) [[unlikely]]
	{
		return;
	}

	Key(event->button(), false, 0);
	event->ignore();
}

bool keyboard_pad_handler::get_mouse_lock_state() const
{
	if (gs_frame* game_frame = dynamic_cast<gs_frame*>(m_target))
		return game_frame->get_mouse_lock_state();
	return false;
}

void keyboard_pad_handler::mouseMoveEvent(QMouseEvent* event)
{
	if (!m_mouse_move_used || !event)
	{
		event->ignore();
		return;
	}

	static int movement_x = 0;
	static int movement_y = 0;

	if (m_target && m_target->isActive() && get_mouse_lock_state())
	{
		// get the screen dimensions
		const QSize screen = m_target->size();

		// get the center of the screen in global coordinates
		QPoint p_center = m_target->geometry().topLeft() + QPoint(screen.width() / 2, screen.height() / 2);

		// reset the mouse to the center for consistent results since edge movement won't be registered
		QCursor::setPos(m_target->screen(), p_center);

		// convert the center into screen coordinates
		p_center = m_target->mapFromGlobal(p_center);

		// get the delta of the mouse position to the screen center
		const QPoint p_delta = event->pos() - p_center;

		if (m_mouse_movement_mode == mouse_movement_mode::relative)
		{
			movement_x = p_delta.x();
			movement_y = p_delta.y();
		}
		else
		{
			// current mouse position, starting at the center
			static QPoint p_real(p_center);

			// update the current position without leaving the screen borders
			p_real.setX(std::clamp(p_real.x() + p_delta.x(), 0, screen.width()));
			p_real.setY(std::clamp(p_real.y() + p_delta.y(), 0, screen.height()));

			// get the delta of the real mouse position to the screen center
			const QPoint p_real_delta = p_real - p_center;

			movement_x = p_real_delta.x();
			movement_y = p_real_delta.y();
		}
	}
	else if (m_mouse_movement_mode == mouse_movement_mode::relative)
	{
		static int last_pos_x = 0;
		static int last_pos_y = 0;
		const QPoint e_pos = event->pos();

		movement_x = e_pos.x() - last_pos_x;
		movement_y = e_pos.y() - last_pos_y;

		last_pos_x = e_pos.x();
		last_pos_y = e_pos.y();
	}
	else if (m_target && m_target->isActive())
	{
		// get the screen dimensions
		const QSize screen = m_target->size();

		// get the center of the screen in global coordinates
		QPoint p_center = m_target->geometry().topLeft() + QPoint(screen.width() / 2, screen.height() / 2);

		// convert the center into screen coordinates
		p_center = m_target->mapFromGlobal(p_center);

		// get the delta of the mouse position to the screen center
		const QPoint p_delta = event->pos() - p_center;

		movement_x = p_delta.x();
		movement_y = p_delta.y();
	}

	movement_x *= m_multi_x;
	movement_y *= m_multi_y;

	int deadzone_x = 0;
	int deadzone_y = 0;

	if (movement_x == 0 && movement_y != 0)
	{
		deadzone_y = m_deadzone_y;
	}
	else if (movement_y == 0 && movement_x != 0)
	{
		deadzone_x = m_deadzone_x;
	}
	else if (movement_x != 0 && movement_y != 0 && m_deadzone_x != 0 && m_deadzone_y != 0)
	{
		// Calculate the point on our deadzone ellipsis intersected with the line (0, 0)(movement_x, movement_y)
		// Ellipsis: 1 = (x²/a²) + (y²/b²)     ; where: a = m_deadzone_x and b = m_deadzone_y
		// Line:     y = mx + t                ; where: t = 0 and m = (movement_y / movement_x)
		// Combined: x = +-(a*b)/sqrt(a²m²+b²) ; where +- is always +, since we only want the magnitude

		const double a = m_deadzone_x;
		const double b = m_deadzone_y;
		const double m = static_cast<double>(movement_y) / static_cast<double>(movement_x);

		deadzone_x = a * b / std::sqrt(std::pow(a, 2) * std::pow(m, 2) + std::pow(b, 2));
		deadzone_y = std::abs(m * deadzone_x);
	}

	if (movement_x < 0)
	{
		Key(mouse::move_right, false);
		Key(mouse::move_left, true, std::min(deadzone_x + std::abs(movement_x), 255));
		m_last_mouse_move_left = steady_clock::now();
	}
	else if (movement_x > 0)
	{
		Key(mouse::move_left, false);
		Key(mouse::move_right, true, std::min(deadzone_x + movement_x, 255));
		m_last_mouse_move_right = steady_clock::now();
	}

	// in Qt mouse up is equivalent to movement_y < 0
	if (movement_y < 0)
	{
		Key(mouse::move_down, false);
		Key(mouse::move_up, true, std::min(deadzone_y + std::abs(movement_y), 255));
		m_last_mouse_move_up = steady_clock::now();
	}
	else if (movement_y > 0)
	{
		Key(mouse::move_up, false);
		Key(mouse::move_down, true, std::min(deadzone_y + movement_y, 255));
		m_last_mouse_move_down = steady_clock::now();
	}

	event->ignore();
}

void keyboard_pad_handler::mouseWheelEvent(QWheelEvent* event)
{
	if (!m_mouse_wheel_used || !event)
	{
		return;
	}

	const QPoint direction = event->angleDelta();

	if (direction.isNull())
	{
		// Scrolling started/ended event, no direction given
		return;
	}

	if (const int x = direction.x())
	{
		const bool to_left = event->inverted() ? x < 0 : x > 0;

		if (to_left)
		{
			Key(mouse::wheel_left, true);
			m_last_wheel_move_left = steady_clock::now();
		}
		else
		{
			Key(mouse::wheel_right, true);
			m_last_wheel_move_right = steady_clock::now();
		}
	}
	if (const int y = direction.y())
	{
		const bool to_up = event->inverted() ? y < 0 : y > 0;

		if (to_up)
		{
			Key(mouse::wheel_up, true);
			m_last_wheel_move_up = steady_clock::now();
		}
		else
		{
			Key(mouse::wheel_down, true);
			m_last_wheel_move_down = steady_clock::now();
		}
	}
}
#endif
std::vector<pad_list_entry> AndroidVirtualPadHandler::list_devices()
{
    std::vector<pad_list_entry> list_devices;
    list_devices.emplace_back(std::string(pad::keyboard_device_name), false);
    return list_devices;
}
#if 0
std::string keyboard_pad_handler::GetMouseName(const QMouseEvent* event)
{
	return GetMouseName(event->button());
}

std::string keyboard_pad_handler::GetMouseName(u32 button)
{
	if (const auto it = mouse_list.find(button); it != mouse_list.cend())
		return it->second;
	return "FAIL";
}

QStringList keyboard_pad_handler::GetKeyNames(const QKeyEvent* keyEvent)
{
	QStringList list;

	if (keyEvent->modifiers() & Qt::ShiftModifier)
	{
		list.append("Shift");
		list.append(QKeySequence(keyEvent->key() | Qt::ShiftModifier).toString(QKeySequence::NativeText));
	}
	if (keyEvent->modifiers() & Qt::AltModifier)
	{
		list.append("Alt");
		list.append(QKeySequence(keyEvent->key() | Qt::AltModifier).toString(QKeySequence::NativeText));
	}
	if (keyEvent->modifiers() & Qt::ControlModifier)
	{
		list.append("Ctrl");
		list.append(QKeySequence(keyEvent->key() | Qt::ControlModifier).toString(QKeySequence::NativeText));
	}
	if (keyEvent->modifiers() & Qt::MetaModifier)
	{
		list.append("Meta");
		list.append(QKeySequence(keyEvent->key() | Qt::MetaModifier).toString(QKeySequence::NativeText));
	}
	if (keyEvent->modifiers() & Qt::KeypadModifier)
	{
		list.append("Num"); // helper object, not used as actual key
		list.append(QKeySequence(keyEvent->key() | Qt::KeypadModifier).toString(QKeySequence::NativeText));
	}

	// Handle special cases
	if (const std::string name = native_scan_code_to_string(keyEvent->nativeScanCode()); !name.empty())
	{
		list.append(QString::fromStdString(name));
	}

	switch (keyEvent->key())
	{
	case Qt::Key_Alt:
		list.append("Alt");
		break;
	case Qt::Key_AltGr:
		list.append("AltGr");
		break;
	case Qt::Key_Shift:
		list.append("Shift");
		break;
	case Qt::Key_Control:
		list.append("Ctrl");
		break;
	case Qt::Key_Meta:
		list.append("Meta");
		break;
	default:
		list.append(QKeySequence(keyEvent->key()).toString(QKeySequence::NativeText));
		break;
	}

	list.removeDuplicates();
	return list;
}

std::string keyboard_pad_handler::GetKeyName(const QKeyEvent* keyEvent, bool with_modifiers)
{
	// Handle special cases first
	if (std::string name = native_scan_code_to_string(keyEvent->nativeScanCode()); !name.empty())
	{
		return name;
	}

	switch (keyEvent->key())
	{
	case Qt::Key_Alt: return "Alt";
	case Qt::Key_AltGr: return "AltGr";
	case Qt::Key_Shift: return "Shift";
	case Qt::Key_Control: return "Ctrl";
	case Qt::Key_Meta: return "Meta";
	case Qt::Key_NumLock: return QKeySequence(keyEvent->key()).toString(QKeySequence::NativeText).toStdString();
#ifdef __APPLE__
	// On macOS, the arrow keys are considered to be part of the keypad;
	// since most Mac keyboards lack a keypad to begin with,
	// we change them to regular arrows to avoid confusion
	case Qt::Key_Left: return "←";
	case Qt::Key_Up: return "↑";
	case Qt::Key_Right: return "→";
	case Qt::Key_Down: return "↓";
#endif
	default:
		break;
	}

	if (with_modifiers)
	{
		return QKeySequence(keyEvent->key() | keyEvent->modifiers()).toString(QKeySequence::NativeText).toStdString();
	}

	return QKeySequence(keyEvent->key()).toString(QKeySequence::NativeText).toStdString();
}

std::string keyboard_pad_handler::GetKeyName(const u32& keyCode)
{
	return QKeySequence(keyCode).toString(QKeySequence::NativeText).toStdString();
}
#endif

std::vector<std::set<u32>> AndroidVirtualPadHandler::GetKeyCombos(std::string_view cfg_string)
{
    std::vector<std::set<u32>> res;

    for (const pad::combo& combo : cfg_pad::get_combos(cfg_string))
    {
        std::set<u32> key_codes;

        for (const std::string& button : combo.buttons())
        {
            //if (u32 code = GetKeyCode(QString::fromStdString(button)); code != Qt::NoButton)
            if (u32 code = mouse_list_r.at(button); code)
            {
                key_codes.insert(code);
            }
        }

        if (!key_codes.empty())
        {
            res.push_back(std::move(key_codes));
        }
    }

    return res;
}
#if 0
u32 keyboard_pad_handler::GetKeyCode(const QString& keyName)
{
	if (keyName.isEmpty()) return Qt::NoButton;
	if (const int native_scan_code = native_scan_code_from_string(keyName.toStdString()); native_scan_code >= 0)
		return Qt::Key_unknown + native_scan_code; // Special cases that can't be expressed with Qt::Key
	if (keyName == "Alt") return Qt::Key_Alt;
	if (keyName == "AltGr") return Qt::Key_AltGr;
	if (keyName == "Shift") return Qt::Key_Shift;
	if (keyName == "Ctrl") return Qt::Key_Control;
	if (keyName == "Meta") return Qt::Key_Meta;
#ifdef __APPLE__
	// QKeySequence doesn't work properly for the arrow keys on macOS
	if (keyName == "Num←") return Qt::Key_Left;
	if (keyName == "Num↑") return Qt::Key_Up;
	if (keyName == "Num→") return Qt::Key_Right;
	if (keyName == "Num↓") return Qt::Key_Down;
#endif

	const QKeySequence seq(keyName);
	u32 key_code = Qt::NoButton;

	if (seq.count() == 1 && seq[0].key() != Qt::Key_unknown)
		key_code = seq[0].key();
	else
		input_log.notice("GetKeyCode(%s): seq.count() = %d", keyName, seq.count());

	return key_code;
}

int keyboard_pad_handler::native_scan_code_from_string([[maybe_unused]] const std::string& key)
{
	// NOTE: Qt throws a Ctrl key at us when using Alt Gr first, so right Alt does not work at the moment
	if (key == "Shift Left") return native_key::shift_l;
	if (key == "Shift Right") return native_key::shift_r;
	if (key == "Ctrl Left") return native_key::ctrl_l;
	if (key == "Ctrl Right") return native_key::ctrl_r;
	if (key == "Alt Left") return native_key::alt_l;
	if (key == "Alt Right") return native_key::alt_r;
	if (key == "Meta Left") return native_key::meta_l;
	if (key == "Meta Right") return native_key::meta_r;
#ifdef _WIN32
	if (key == "Num+0" || key == "Num+Ins") return 82;
	if (key == "Num+1" || key == "Num+End") return 79;
	if (key == "Num+2" || key == "Num+Down") return 80;
	if (key == "Num+3" || key == "Num+PgDown") return 81;
	if (key == "Num+4" || key == "Num+Left") return 75;
	if (key == "Num+5" || key == "Num+Clear") return 76;
	if (key == "Num+6" || key == "Num+Right") return 77;
	if (key == "Num+7" || key == "Num+Home") return 71;
	if (key == "Num+8" || key == "Num+Up") return 72;
	if (key == "Num+9" || key == "Num+PgUp") return 73;
	if (key == "Num+," || key == "Num+Del") return 83;
	if (key == "Num+/") return 309;
	if (key == "Num+*") return 55;
	if (key == "Num+-") return 74;
	if (key == "Num++") return 78;
	if (key == "Num+Enter") return 284;
#else
	// TODO
#endif
	return -1;
}

std::string keyboard_pad_handler::native_scan_code_to_string(int native_scan_code)
{
	// NOTE: the other Qt function "nativeVirtualKey" does not distinguish between VK_SHIFT and VK_RSHIFT key in Qt at the moment
	// NOTE: Qt throws a Ctrl key at us when using Alt Gr first, so right Alt does not work at the moment
	// NOTE: for MacOs: nativeScanCode may not work
	switch (native_scan_code)
	{
	case native_key::shift_l: return "Shift Left";
	case native_key::shift_r: return "Shift Right";
	case native_key::ctrl_l: return "Ctrl Left";
	case native_key::ctrl_r: return "Ctrl Right";
	case native_key::alt_l: return "Alt Left";
	case native_key::alt_r: return "Alt Right";
	case native_key::meta_l: return "Meta Left";
	case native_key::meta_r: return "Meta Right";
#ifdef _WIN32
	case 82: return "Num+0"; // Also "Num+Ins" depending on numlock
	case 79: return "Num+1"; // Also "Num+End" depending on numlock
	case 80: return "Num+2"; // Also "Num+Down" depending on numlock
	case 81: return "Num+3"; // Also "Num+PgDown" depending on numlock
	case 75: return "Num+4"; // Also "Num+Left" depending on numlock
	case 76: return "Num+5"; // Also "Num+Clear" depending on numlock
	case 77: return "Num+6"; // Also "Num+Right" depending on numlock
	case 71: return "Num+7"; // Also "Num+Home" depending on numlock
	case 72: return "Num+8"; // Also "Num+Up" depending on numlock
	case 73: return "Num+9"; // Also "Num+PgUp" depending on numlock
	case 83: return "Num+,"; // Also "Num+Del" depending on numlock
	case 309: return "Num+/";
	case 55: return "Num+*";
	case 74: return "Num+-";
	case 78: return "Num++";
	case 284: return "Num+Enter";
#else
	// TODO
#endif
	default: return "";
	}
}
#endif
bool AndroidVirtualPadHandler::bindPadToDevice(std::shared_ptr<Pad> pad)
{
    if (!pad || pad->m_player_id >= g_cfg_input.player.size())
        return false;

    const cfg_player* player_config = g_cfg_input.player[pad->m_player_id];
    if (!player_config || player_config->device.to_string() != pad::keyboard_device_name)
        return false;

    m_pad_configs[pad->m_player_id].from_string(player_config->config.to_string());
    const cfg_pad* cfg = &m_pad_configs[pad->m_player_id];
    if (cfg == nullptr)
        return false;

    m_mouse_movement_mode = cfg->mouse_move_mode;
    m_mouse_move_used = false;
    m_mouse_wheel_used = false;
    m_deadzone_x = cfg->mouse_deadzone_x;
    m_deadzone_y = cfg->mouse_deadzone_y;
    m_multi_x = cfg->mouse_acceleration_x / 100.0;
    m_multi_y = cfg->mouse_acceleration_y / 100.0;
    m_l_stick_lerp_factor = cfg->l_stick_lerp_factor / 100.0f;
    m_r_stick_lerp_factor = cfg->r_stick_lerp_factor / 100.0f;
    m_analog_lerp_factor  = cfg->analog_lerp_factor / 100.0f;
    m_trigger_lerp_factor = cfg->trigger_lerp_factor / 100.0f;
    m_l_stick_multiplier = cfg->lstickmultiplier;
    m_r_stick_multiplier = cfg->rstickmultiplier;
    m_analog_limiter_toggle_mode = cfg->analog_limiter_toggle_mode.get();
    m_pressure_intensity_toggle_mode = cfg->pressure_intensity_toggle_mode.get();
    m_pressure_intensity_deadzone = cfg->pressure_intensity_deadzone.get();

    const auto find_combos = [this](const cfg::string& name)
    {
        //std::vector<std::set<u32>> combos = find_key_combos(mouse_list, name);
        const std::vector<std::set<u32>> combos = GetKeyCombos(name.to_string());
        //for (const std::set<u32>& combo : GetKeyCombos(name)) combos.push_back(combo);

        if (!combos.empty())
        {
            /*if (!m_mouse_move_used)
            {
                if (std::any_of(combos.cbegin(), combos.cend(), [](const std::set<u32>& keys)
                {
                    return keys.contains(mouse::move_left) || keys.contains(mouse::move_right) || keys.contains(mouse::move_up) || keys.contains(mouse::move_down);
                }))
                {
                    m_mouse_move_used = true;
                }
            }

            if (!m_mouse_wheel_used)
            {
                if (std::any_of(combos.cbegin(), combos.cend(), [](const std::set<u32>& keys)
                {
                    return keys.contains(mouse::wheel_left) || keys.contains(mouse::wheel_right) || keys.contains(mouse::wheel_up) || keys.contains(mouse::wheel_down);
                }))
                {
                    m_mouse_wheel_used = true;
                }
            }*/
        }

        return combos;
    };

    u32 pclass_profile = 0x0;

    for (const input::product_info& product : input::get_products_by_class(cfg->device_class_type))
    {
        if (product.vendor_id == cfg->vendor_id && product.product_id == cfg->product_id)
        {
            pclass_profile = product.pclass_profile;
        }
    }

    // Fixed assign change, default is both sensor and press off
    pad->Init
            (
                    CELL_PAD_STATUS_DISCONNECTED,
                    CELL_PAD_CAPABILITY_PS3_CONFORMITY | CELL_PAD_CAPABILITY_PRESS_MODE | CELL_PAD_CAPABILITY_HP_ANALOG_STICK | CELL_PAD_CAPABILITY_ACTUATOR | CELL_PAD_CAPABILITY_SENSOR_MODE,
                    CELL_PAD_DEV_TYPE_STANDARD,
                    cfg->device_class_type,
                    pclass_profile,
                    cfg->vendor_id,
                    cfg->product_id,
                    cfg->pressure_intensity
            );

    if (b_has_pressure_intensity_button)
    {
        pad->m_buttons.emplace_back(special_button_offset, find_combos(cfg->pressure_intensity_button), special_button_value::pressure_intensity);
        pad->m_pressure_intensity_button_index = static_cast<s32>(pad->m_buttons.size()) - 1;
    }

    if (b_has_analog_limiter_button)
    {
        pad->m_buttons.emplace_back(special_button_offset, find_combos(cfg->analog_limiter_button), special_button_value::analog_limiter);
        pad->m_analog_limiter_button_index = static_cast<s32>(pad->m_buttons.size()) - 1;
    }

    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, find_combos(cfg->left),     CELL_PAD_CTRL_LEFT);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, find_combos(cfg->down),     CELL_PAD_CTRL_DOWN);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, find_combos(cfg->right),    CELL_PAD_CTRL_RIGHT);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, find_combos(cfg->up),       CELL_PAD_CTRL_UP);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, find_combos(cfg->start),    CELL_PAD_CTRL_START);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, find_combos(cfg->r3),       CELL_PAD_CTRL_R3);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, find_combos(cfg->l3),       CELL_PAD_CTRL_L3);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, find_combos(cfg->select),   CELL_PAD_CTRL_SELECT);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, find_combos(cfg->ps),       CELL_PAD_CTRL_PS);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, find_combos(cfg->square),   CELL_PAD_CTRL_SQUARE);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, find_combos(cfg->cross),    CELL_PAD_CTRL_CROSS);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, find_combos(cfg->circle),   CELL_PAD_CTRL_CIRCLE);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, find_combos(cfg->triangle), CELL_PAD_CTRL_TRIANGLE);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, find_combos(cfg->r1),       CELL_PAD_CTRL_R1);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, find_combos(cfg->l1),       CELL_PAD_CTRL_L1);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, find_combos(cfg->r2),       CELL_PAD_CTRL_R2);
    pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, find_combos(cfg->l2),       CELL_PAD_CTRL_L2);

    if (pad->m_class_type == CELL_PAD_PCLASS_TYPE_SKATEBOARD)
    {
        pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_PRESS_PIGGYBACK, find_combos(cfg->ir_nose), CELL_PAD_CTRL_PRESS_TRIANGLE);
        pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_PRESS_PIGGYBACK, find_combos(cfg->ir_tail), CELL_PAD_CTRL_PRESS_CIRCLE);
        pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_PRESS_PIGGYBACK, find_combos(cfg->ir_left), CELL_PAD_CTRL_PRESS_CROSS);
        pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_PRESS_PIGGYBACK, find_combos(cfg->ir_right), CELL_PAD_CTRL_PRESS_SQUARE);
        pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_PRESS_PIGGYBACK, find_combos(cfg->tilt_left), CELL_PAD_CTRL_PRESS_L1);
        pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_PRESS_PIGGYBACK, find_combos(cfg->tilt_right), CELL_PAD_CTRL_PRESS_R1);
    }

    pad->m_sticks[0] = AnalogStick(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X,  find_combos(cfg->ls_left), find_combos(cfg->ls_right));
    pad->m_sticks[1] = AnalogStick(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y,  find_combos(cfg->ls_up),   find_combos(cfg->ls_down));
    pad->m_sticks[2] = AnalogStick(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X, find_combos(cfg->rs_left), find_combos(cfg->rs_right));
    pad->m_sticks[3] = AnalogStick(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y, find_combos(cfg->rs_up),   find_combos(cfg->rs_down));

    pad->m_sensors[0] = AnalogSensor(CELL_PAD_BTN_OFFSET_SENSOR_X, 0, 0, 0, DEFAULT_MOTION_X);
    pad->m_sensors[1] = AnalogSensor(CELL_PAD_BTN_OFFSET_SENSOR_Y, 0, 0, 0, DEFAULT_MOTION_Y);
    pad->m_sensors[2] = AnalogSensor(CELL_PAD_BTN_OFFSET_SENSOR_Z, 0, 0, 0, DEFAULT_MOTION_Z);
    pad->m_sensors[3] = AnalogSensor(CELL_PAD_BTN_OFFSET_SENSOR_G, 0, 0, 0, DEFAULT_MOTION_G);

    pad->m_vibrate_motors[0] = VibrateMotor(true);
    pad->m_vibrate_motors[1] = VibrateMotor(false);

    m_bindings.emplace_back(pad, nullptr, nullptr);
    m_pads_internal.push_back(*pad);

    return true;
}

void AndroidVirtualPadHandler::process()
{
    constexpr double stick_interval = 10.0;
    constexpr double button_interval = 10.0;

    const auto now = steady_clock::now();

    const double elapsed_stick = std::chrono::duration_cast<std::chrono::microseconds>(now - m_stick_time).count() / 1000.0;
    const double elapsed_button = std::chrono::duration_cast<std::chrono::microseconds>(now - m_button_time).count() / 1000.0;

    const bool update_sticks = elapsed_stick > stick_interval;
    const bool update_buttons = elapsed_button > button_interval;

    if (update_sticks)
    {
        m_stick_time = now;
    }

    if (update_buttons)
    {
        m_button_time = now;
    }

    if (m_mouse_move_used && m_mouse_movement_mode == mouse_movement_mode::relative)
    {
        constexpr double mouse_interval = 30.0;

        const double elapsed_left  = std::chrono::duration_cast<std::chrono::microseconds>(now - m_last_mouse_move_left).count() / 1000.0;
        const double elapsed_right = std::chrono::duration_cast<std::chrono::microseconds>(now - m_last_mouse_move_right).count() / 1000.0;
        const double elapsed_up    = std::chrono::duration_cast<std::chrono::microseconds>(now - m_last_mouse_move_up).count() / 1000.0;
        const double elapsed_down  = std::chrono::duration_cast<std::chrono::microseconds>(now - m_last_mouse_move_down).count() / 1000.0;

        // roughly 1-2 frames to process the next mouse move
        /*if (elapsed_left > mouse_interval)
        {
            Key(mouse::move_left, false);
            m_last_mouse_move_left = now;
        }
        if (elapsed_right > mouse_interval)
        {
            Key(mouse::move_right, false);
            m_last_mouse_move_right = now;
        }
        if (elapsed_up > mouse_interval)
        {
            Key(mouse::move_up, false);
            m_last_mouse_move_up = now;
        }
        if (elapsed_down > mouse_interval)
        {
            Key(mouse::move_down, false);
            m_last_mouse_move_down = now;
        }*/
    }

    const auto get_lerped = [](f32 v0, f32 v1, f32 lerp_factor)
    {
        // linear interpolation from the current value v0 to the desired value v1
        const f32 res = std::lerp(v0, v1, lerp_factor);

        // round to the correct direction to prevent sticky values on small factors
        return (v0 <= v1) ? std::ceil(res) : std::floor(res);
    };

    for (uint i = 0; i < m_pads_internal.size(); i++)
    {
        auto& pad = m_pads_internal[i];

        if (last_connection_status[i] == false)
        {
            ensure(m_bindings[i].pad);
            m_bindings[i].pad->m_port_status |= CELL_PAD_STATUS_CONNECTED;
            m_bindings[i].pad->m_port_status |= CELL_PAD_STATUS_ASSIGN_CHANGES;
            last_connection_status[i] = true;
            connected_devices++;
        }
        else
        {
            if (update_sticks)
            {
                for (usz j = 0; j < pad.m_sticks.size(); j++)
                {
                    const f32 stick_lerp_factor = (j < 2) ? m_l_stick_lerp_factor : m_r_stick_lerp_factor;

                    // we already applied the following values on keypress if we used factor 1
                    if (stick_lerp_factor < 1.0f)
                    {
                        const f32 v0 = static_cast<f32>(pad.m_sticks[j].m_value);
                        const f32 v1 = static_cast<f32>(m_stick_val[j]);
                        const f32 res = get_lerped(v0, v1, stick_lerp_factor);

                        pad.m_sticks[j].m_value = static_cast<u16>(res);
                    }
                }
            }

            if (update_buttons)
            {
                for (Button& button : pad.m_buttons)
                {
                    if (button.m_analog)
                    {
                        // we already applied the following values on keypress if we used factor 1
                        if (m_analog_lerp_factor < 1.0f)
                        {
                            const f32 v0 = static_cast<f32>(button.m_value);
                            const f32 v1 = static_cast<f32>(button.m_actual_value);
                            const f32 res = get_lerped(v0, v1, m_analog_lerp_factor);

                            button.m_value = static_cast<u16>(res);
                            button.m_pressed = button.m_value > 0;
                        }
                    }
                    else if (button.m_trigger)
                    {
                        // we already applied the following values on keypress if we used factor 1
                        if (m_trigger_lerp_factor < 1.0f)
                        {
                            const f32 v0 = static_cast<f32>(button.m_value);
                            const f32 v1 = static_cast<f32>(button.m_actual_value);
                            const f32 res = get_lerped(v0, v1, m_trigger_lerp_factor);

                            button.m_value = static_cast<u16>(res);
                            button.m_pressed = button.m_value > 0;
                        }
                    }
                }
            }
        }
    }

    /*if (m_mouse_wheel_used)
    {
        // Releases the wheel buttons 0,1 sec after they've been triggered
        // Next activation is set to distant future to avoid activating this on every proc
        const auto update_threshold = now - std::chrono::milliseconds(100);
        const auto distant_future = now + std::chrono::hours(24);

        if (update_threshold >= m_last_wheel_move_up)
        {
            Key(mouse::wheel_up, false);
            m_last_wheel_move_up = distant_future;
        }
        if (update_threshold >= m_last_wheel_move_down)
        {
            Key(mouse::wheel_down, false);
            m_last_wheel_move_down = distant_future;
        }
        if (update_threshold >= m_last_wheel_move_left)
        {
            Key(mouse::wheel_left, false);
            m_last_wheel_move_left = distant_future;
        }
        if (update_threshold >= m_last_wheel_move_right)
        {
            Key(mouse::wheel_right, false);
            m_last_wheel_move_right = distant_future;
        }
    }*/

    for (uint i = 0; i < m_bindings.size(); i++)
    {
        auto& pad = m_bindings[i].pad;
        ensure(pad);

        const cfg_pad* cfg = &m_pad_configs[pad->m_player_id];
        ensure(cfg);

        const Pad& pad_internal = m_pads_internal[i];

        // Normalize and apply pad squircling
        // Copy sticks first. We don't want to modify the raw internal values
        std::array<AnalogStick, 4> squircled_sticks = pad_internal.m_sticks;

        // Apply squircling
        if (cfg->lpadsquircling != 0)
        {
            u16& lx = squircled_sticks[0].m_value;
            u16& ly = squircled_sticks[1].m_value;

            ConvertToSquirclePoint(lx, ly, cfg->lpadsquircling);
        }

        if (cfg->rpadsquircling != 0)
        {
            u16& rx = squircled_sticks[2].m_value;
            u16& ry = squircled_sticks[3].m_value;

            ConvertToSquirclePoint(rx, ry, cfg->rpadsquircling);
        }

        pad->m_buttons = pad_internal.m_buttons;
        pad->m_sticks = squircled_sticks; // Don't use std::move here. We assign values lockless, so std::move can lead to segfaults.
    }
}

extern void pad_state_notify_state_change(usz index, u32 state);
extern bool is_input_allowed();
extern std::string g_input_config_override;
/*
namespace pad
{
    atomic_t<pad_thread*> g_pad_thread = nullptr;
    shared_mutex g_pad_mutex;
    std::string g_title_id;
    atomic_t<bool> g_started{false};
    atomic_t<bool> g_reset{false};
    atomic_t<bool> g_enabled{true};
    atomic_t<bool> g_home_menu_requested{false};
}*/

namespace rsx
{
    void set_native_ui_flip();
}

struct pad_setting
{
    u32 port_status = 0;
    u32 device_capability = 0;
    u32 device_type = 0;
    bool is_ldd_pad = false;
};
#if 0
pad_thread::pad_thread(void* curthread, void* curwindow, std::string_view title_id) : m_curthread(curthread), m_curwindow(curwindow)
{
    pad::g_title_id = title_id;
    pad::g_pad_thread = this;
    pad::g_started = false;
}

pad_thread::~pad_thread()
{
    pad::g_pad_thread = nullptr;
}

void pad_thread::Init()
{
    std::lock_guard lock(pad::g_pad_mutex);

    // Cache old settings if possible
    std::array<pad_setting, CELL_PAD_MAX_PORT_NUM> pad_settings;
    for (u32 i = 0; i < CELL_PAD_MAX_PORT_NUM; i++) // max 7 pads
    {
        if (m_pads[i])
        {
            pad_settings[i] =
                    {
                            m_pads[i]->m_port_status,
                            m_pads[i]->m_device_capability,
                            m_pads[i]->m_device_type,
                            m_pads[i]->ldd
                    };
        }
        else
        {
            pad_settings[i] =
                    {
                            CELL_PAD_STATUS_DISCONNECTED,
                            CELL_PAD_CAPABILITY_PS3_CONFORMITY | CELL_PAD_CAPABILITY_PRESS_MODE | CELL_PAD_CAPABILITY_ACTUATOR,
                            CELL_PAD_DEV_TYPE_STANDARD,
                            false
                    };
        }
    }

    num_ldd_pad = 0;

    m_info.now_connect = 0;

    m_handlers.clear();

    g_cfg_input_configs.load();

    std::string active_config = g_cfg_input_configs.active_configs.get_value(pad::g_title_id);

    if (active_config.empty())
    {
        active_config = g_cfg_input_configs.active_configs.get_value(g_cfg_input_configs.global_key);
    }

    input_log.notice("Using input configuration: '%s' (override='%s')", active_config, g_input_config_override);

    // Load in order to get the pad handlers
    if (!g_cfg_input.load(pad::g_title_id, active_config))
    {
        input_log.notice("Loaded empty pad config");
    }

    // Adjust to the different pad handlers
    for (usz i = 0; i < g_cfg_input.player.size(); i++)
    {
        std::shared_ptr<PadHandlerBase> handler;
        pad_thread::InitPadConfig(g_cfg_input.player[i]->config, g_cfg_input.player[i]->handler, handler);
    }

    // Reload with proper defaults
    if (!g_cfg_input.load(pad::g_title_id, active_config))
    {
        input_log.notice("Reloaded empty pad config");
    }

    input_log.trace("Using pad config:\n%s", g_cfg_input);

    std::shared_ptr<AndroidVirtualPadHandler> keyptr;

    // Always have a Null Pad Handler
    std::shared_ptr<NullPadHandler> nullpad = std::make_shared<NullPadHandler>();
    m_handlers.emplace(pad_handler::null, nullpad);

    for (u32 i = 0; i < CELL_PAD_MAX_PORT_NUM; i++) // max 7 pads
    {
        cfg_player* cfg = g_cfg_input.player[i];
        std::shared_ptr<PadHandlerBase> cur_pad_handler;

        const pad_handler handler_type = pad_settings[i].is_ldd_pad ? pad_handler::null : cfg->handler.get();

        if (m_handlers.contains(handler_type))
        {
            cur_pad_handler = m_handlers[handler_type];
        }
        else
        {
            if (handler_type == pad_handler::keyboard)
            {
                keyptr = std::make_shared<AndroidVirtualPadHandler>();
                //keyptr->moveToThread(static_cast<QThread*>(m_curthread));
                //keyptr->SetTargetWindow(static_cast<QWindow*>(m_curwindow));
                cur_pad_handler = keyptr;
            }
            else
            {
                cur_pad_handler = GetHandler(handler_type);
            }

            m_handlers.emplace(handler_type, cur_pad_handler);
        }
        cur_pad_handler->Init();

        std::shared_ptr<Pad> pad = std::make_shared<Pad>(handler_type, i, CELL_PAD_STATUS_DISCONNECTED, pad_settings[i].device_capability, pad_settings[i].device_type);
        m_pads[i] = pad;

        if (pad_settings[i].is_ldd_pad)
        {
            InitLddPad(i, &pad_settings[i].port_status);
        }
        else
        {
            if (!cur_pad_handler->bindPadToDevice(pad))
            {
                // Failed to bind the device to cur_pad_handler so binds to NullPadHandler
                input_log.error("Failed to bind device '%s' to handler %s. Falling back to NullPadHandler.", cfg->device.to_string(), handler_type);
                nullpad->bindPadToDevice(pad);
            }

            input_log.notice("Pad %d: device='%s', handler=%s, VID=0x%x, PID=0x%x, class_type=0x%x, class_profile=0x%x",
                             i, cfg->device.to_string(), pad->m_pad_handler, pad->m_vendor_id, pad->m_product_id, pad->m_class_type, pad->m_class_profile);

            if (pad->m_pad_handler != pad_handler::null)
            {
                input_log.notice("Pad %d: config=\n%s", i, cfg->to_string());
            }
        }

        pad->is_fake_pad = (g_cfg.io.move == move_handler::fake && i >= (static_cast<u32>(CELL_PAD_MAX_PORT_NUM) - static_cast<u32>(CELL_GEM_MAX_NUM)))
                           || (pad->m_class_type >= CELL_PAD_FAKE_TYPE_FIRST && pad->m_class_type < CELL_PAD_FAKE_TYPE_LAST);
        connect_usb_controller(i, input::get_product_by_vid_pid(pad->m_vendor_id, pad->m_product_id));
    }

    // Initialize active mouse and keyboard. Activate pad handler if one exists.
    input::set_mouse_and_keyboard(m_handlers.contains(pad_handler::keyboard) ? input::active_mouse_and_keyboard::pad : input::active_mouse_and_keyboard::emulated);
}
void pad_thread::SetRumble(u32 pad, u8 large_motor, u8 small_motor)
{
    if (pad >= m_pads.size() || !m_pads[pad])
        return;

    const u64 now_us = get_system_time();

    m_pads[pad]->m_last_rumble_time_us = now_us;
    m_pads[pad]->m_vibrate_motors[0].value = large_motor;
    m_pads[pad]->m_vibrate_motors[1].value = small_motor;

    // Rumble copilots as well
    for (const auto& copilot : m_pads[pad]->copilots)
    {
        if (copilot && copilot->is_connected())
        {
            copilot->m_last_rumble_time_us = now_us;
            copilot->m_vibrate_motors[0].value = large_motor;
            copilot->m_vibrate_motors[1].value = small_motor;
        }
    }
}
void pad_thread::SetIntercepted(bool intercepted)
{
    if (intercepted)
    {
        m_info.system_info |= CELL_PAD_INFO_INTERCEPTED;
        m_info.ignore_input = true;
    }
    else
    {
        m_info.system_info &= ~CELL_PAD_INFO_INTERCEPTED;
    }
}

void pad_thread::update_pad_states()
{

    for (usz i = 0; i < m_pads.size(); i++)
    {
        const auto& pad = m_pads[i];
        const bool connected = pad && !pad->is_fake_pad && !!(pad->m_port_status & CELL_PAD_STATUS_CONNECTED);

        if (m_pads_connected[i] == connected)
            continue;

        pad_state_notify_state_change(i, connected ? CELL_PAD_STATUS_CONNECTED : CELL_PAD_STATUS_DISCONNECTED);

        m_pads_connected[i] = connected;
    }
}

namespace ae{
    extern pthread_mutex_t key_event_mutex;
}

void pad_thread::operator()()
{
    Init();

    const bool is_vsh = Emu.IsVsh();

    pad::g_reset = false;
    pad::g_started = true;

    bool mode_changed = true;

    atomic_t<pad_handler_mode> pad_mode{g_cfg.io.pad_mode.get()};
    std::vector<std::unique_ptr<named_thread<std::function<void()>>>> threads;

    const auto stop_threads = [&threads]()
    {
        input_log.notice("Stopping pad threads...");

        for (auto& thread : threads)
        {
            if (thread)
            {
                auto& enumeration_thread = *thread;
                enumeration_thread = thread_state::aborting;
                enumeration_thread();
            }
        }
        threads.clear();

        input_log.notice("Pad threads stopped");
    };

    const auto start_threads = [this, &threads, &pad_mode]()
    {
        if (pad_mode == pad_handler_mode::single_threaded)
        {
            return;
        }

        input_log.notice("Starting pad threads...");

        for (const auto& handler : m_handlers)
        {
            if (handler.first == pad_handler::null)
            {
                continue;
            }

            threads.push_back(std::make_unique<named_thread<std::function<void()>>>(fmt::format("%s Thread", handler.second->m_type), [&handler = handler.second, &pad_mode]()
            {
                while (thread_ctrl::state() != thread_state::aborting)
                {
                    if (!pad::g_enabled || !is_input_allowed())
                    {
                        thread_ctrl::wait_for(30'000);
                        continue;
                    }

                    handler->process();

                    u64 pad_sleep = g_cfg.io.pad_sleep;

                    if (Emu.IsPaused())
                    {
                        pad_sleep = std::max<u64>(pad_sleep, 30'000);
                    }

                    thread_ctrl::wait_for(pad_sleep);
                }
            }));
        }

        input_log.notice("Pad threads started");
    };

    while (thread_ctrl::state() != thread_state::aborting)
    {
        if (!pad::g_enabled || !is_input_allowed())
        {
            m_resume_emulation_flag = false;
            m_mask_start_press_to_resume = 0;
            thread_ctrl::wait_for(30'000);
            continue;
        }

        // Update variables
        const bool needs_reset = pad::g_reset && pad::g_reset.exchange(false);
        const pad_handler_mode new_pad_mode = g_cfg.io.pad_mode.get();
        mode_changed |= new_pad_mode != pad_mode.exchange(new_pad_mode);

        // Reset pad handlers if necessary
        if (needs_reset || mode_changed)
        {
            mode_changed = false;

            stop_threads();

            if (needs_reset)
            {
                Init();
            }
            else
            {
                input_log.success("The pad mode was changed to %s", pad_mode.load());
            }

            start_threads();
        }

        u32 connected_devices = 0;

        if (pad_mode == pad_handler_mode::single_threaded)
        {
            pthread_mutex_lock(&ae::key_event_mutex);
            for (auto& handler : m_handlers)
            {
                handler.second->process();
                connected_devices += handler.second->connected_devices;
            }
            pthread_mutex_unlock(&ae::key_event_mutex);
        }
        else
        {
            for (auto& handler : m_handlers)
            {
                connected_devices += handler.second->connected_devices;
            }
        }

        if (Emu.IsRunning())
        {
            update_pad_states();
        }

        m_info.now_connect = connected_devices + num_ldd_pad;

        // The ignore_input section is only reached when a dialog was closed and the pads are still intercepted.
        // As long as any of the listed buttons is pressed, cellPadGetData will ignore all input (needed for Hotline Miami).
        // ignore_input was added because if we keep the pads intercepted, then some games will enter the menu due to unexpected system interception (tested with Ninja Gaiden Sigma).
        if (m_info.ignore_input && !(m_info.system_info & CELL_PAD_INFO_INTERCEPTED))
        {
            bool any_button_pressed = false;

            for (usz i = 0; i < m_pads.size() && !any_button_pressed; i++)
            {
                const auto& pad = m_pads[i];

                if (!(pad->m_port_status & CELL_PAD_STATUS_CONNECTED))
                    continue;

                for (const auto& button : pad->m_buttons)
                {
                    if (button.m_pressed && (
                            button.m_outKeyCode == CELL_PAD_CTRL_CROSS ||
                            button.m_outKeyCode == CELL_PAD_CTRL_CIRCLE ||
                            button.m_outKeyCode == CELL_PAD_CTRL_TRIANGLE ||
                            button.m_outKeyCode == CELL_PAD_CTRL_SQUARE ||
                            button.m_outKeyCode == CELL_PAD_CTRL_START ||
                            button.m_outKeyCode == CELL_PAD_CTRL_SELECT))
                    {
                        any_button_pressed = true;
                        break;
                    }
                }
            }

            if (!any_button_pressed)
            {
                m_info.ignore_input = false;
            }
        }

        // Handle home menu if requested
        if (!is_vsh && !m_home_menu_open && Emu.IsRunning())
        {
            bool ps_button_pressed = false;

            for (usz i = 0; i < m_pads.size() && !ps_button_pressed; i++)
            {
                if (i > 0 && g_cfg.io.lock_overlay_input_to_player_one)
                    break;

                const auto& pad = m_pads[i];

                if (!(pad->m_port_status & CELL_PAD_STATUS_CONNECTED))
                    continue;

                // Check if an LDD pad pressed the PS button (bit 0 of the first button)
                // NOTE: Rock Band 3 doesn't seem to care about the len. It's always 0.
                if (pad->ldd /*&& pad->ldd_data.len >= 1 */&& !!(pad->ldd_data.button[0] & CELL_PAD_CTRL_LDD_PS))
                {
                    ps_button_pressed = true;
                    break;
                }

                for (const auto& button : pad->m_buttons)
                {
                    if (button.m_offset == CELL_PAD_BTN_OFFSET_DIGITAL1 && button.m_outKeyCode == CELL_PAD_CTRL_PS && button.m_pressed)
                    {
                        ps_button_pressed = true;
                        break;
                    }
                }
            }

            // Make sure we call this function only once per button press
            if ((ps_button_pressed && !m_ps_button_pressed) || pad::g_home_menu_requested.exchange(false))
            {
                open_home_menu();
            }

            m_ps_button_pressed = ps_button_pressed;
        }

        // Handle paused emulation (if triggered by home menu).
        if (m_home_menu_open && g_cfg.misc.pause_during_home_menu)
        {
            // Reset resume control if the home menu is open
            m_resume_emulation_flag = false;
            m_mask_start_press_to_resume = 0;
            m_track_start_press_begin_timestamp = 0;

            // Update UI
            rsx::set_native_ui_flip();
            thread_ctrl::wait_for(33'000);
            continue;
        }

        if (m_resume_emulation_flag)
        {
            m_resume_emulation_flag = false;

            Emu.BlockingCallFromMainThread([]()
                                           {
                                               Emu.Resume();
                                           });
        }

        u64 pad_sleep = g_cfg.io.pad_sleep;

        if (Emu.GetStatus(false) == system_state::paused)
        {
            pad_sleep = std::max<u64>(pad_sleep, 30'000);

            u64 timestamp = get_system_time();
            u32 pressed_mask = 0;

            for (usz i = 0; i < m_pads.size(); i++)
            {
                const auto& pad = m_pads[i];

                if (!(pad->m_port_status & CELL_PAD_STATUS_CONNECTED))
                    continue;
                for (const auto& button : pad->m_buttons)
                {
                    if (button.m_offset == CELL_PAD_BTN_OFFSET_DIGITAL1 && button.m_outKeyCode == CELL_PAD_CTRL_START && button.m_pressed)
                    {
                        pressed_mask |= 1u << i;
                        break;
                    }
                }
            }

            m_mask_start_press_to_resume &= pressed_mask;

            if (!pressed_mask || timestamp - m_track_start_press_begin_timestamp >= 700'000)
            {
                m_track_start_press_begin_timestamp = timestamp;

                if (std::exchange(m_mask_start_press_to_resume, u32{umax}))
                {
                    m_mask_start_press_to_resume = 0;
                    m_track_start_press_begin_timestamp = 0;

                    //sys_log.success("Resuming emulation using the START button in a few seconds...");

                    auto msg_ref = std::make_shared<atomic_t<u32>>(1);
                    rsx::overlays::queue_message(localized_string_id::EMULATION_RESUMING, 2'000'000, msg_ref);

                    m_resume_emulation_flag = true;

                    for (u32 i = 0; i < 40; i++)
                    {
                        if (Emu.GetStatus(false) != system_state::paused)
                        {
                            // Abort if emulation has been resumed by other means
                            m_resume_emulation_flag = false;
                            msg_ref->release(0);
                            break;
                        }

                        thread_ctrl::wait_for(50'000);
                        rsx::set_native_ui_flip();
                    }
                }
            }
        }
        else
        {
            // Reset resume control if caught a state of unpaused emulation
            m_mask_start_press_to_resume = 0;
            m_track_start_press_begin_timestamp = 0;
        }

        thread_ctrl::wait_for(pad_sleep);
    }

    stop_threads();
}

void pad_thread::InitLddPad(u32 handle, const u32* port_status)
{
    if (handle >= m_pads.size())
    {
        return;
    }

    static const input::product_info product = input::get_product_info(input::product_type::playstation_3_controller);

    m_pads[handle]->ldd = true;
    m_pads[handle]->Init
            (
                    port_status ? *port_status : CELL_PAD_STATUS_CONNECTED | CELL_PAD_STATUS_ASSIGN_CHANGES | CELL_PAD_STATUS_CUSTOM_CONTROLLER,
                    CELL_PAD_CAPABILITY_PS3_CONFORMITY,
                    CELL_PAD_DEV_TYPE_LDD,
                    CELL_PAD_PCLASS_TYPE_STANDARD,
                    product.pclass_profile,
                    product.vendor_id,
                    product.product_id,
                    50
            );

    input_log.notice("Pad %d: LDD, VID=0x%x, PID=0x%x, class_type=0x%x, class_profile=0x%x",
                     handle, m_pads[handle]->m_vendor_id, m_pads[handle]->m_product_id, m_pads[handle]->m_class_type, m_pads[handle]->m_class_profile);

    num_ldd_pad++;
}

s32 pad_thread::AddLddPad()
{
    // Look for first null pad
    for (u32 i = 0; i < CELL_PAD_MAX_PORT_NUM; i++)
    {
        if (g_cfg_input.player[i]->handler == pad_handler::null && !m_pads[i]->ldd)
        {
            InitLddPad(i, nullptr);
            return i;
        }
    }

    return -1;
}

void pad_thread::UnregisterLddPad(u32 handle)
{
    ensure(handle < m_pads.size());

    m_pads[handle]->ldd = false;

    num_ldd_pad--;
}

std::shared_ptr<PadHandlerBase> pad_thread::GetHandler(pad_handler type)
{
    switch (type)
    {
        case pad_handler::null:
            return std::make_shared<NullPadHandler>();
        case pad_handler::keyboard:
            return std::make_shared<AndroidVirtualPadHandler>();
            /*case pad_handler::ds3:
                return std::make_shared<ds3_pad_handler>();
            case pad_handler::ds4:
                return std::make_shared<ds4_pad_handler>();
            case pad_handler::dualsense:
                return std::make_shared<dualsense_pad_handler>();
            case pad_handler::skateboard:
                return std::make_shared<skateboard_pad_handler>();
        #ifdef _WIN32
            case pad_handler::xinput:
                return std::make_shared<xinput_pad_handler>();
            case pad_handler::mm:
                return std::make_shared<mm_joystick_handler>();
        #endif
        #ifdef HAVE_SDL2
            case pad_handler::sdl:
                return std::make_shared<sdl_pad_handler>();
        /*#endif
        #ifdef HAVE_LIBEVDEV
            case pad_handler::evdev:
                return std::make_shared<evdev_joystick_handler>();
        #endif*/
    }

    return nullptr;
}

void pad_thread::InitPadConfig(cfg_pad& cfg, pad_handler type, std::shared_ptr<PadHandlerBase>& handler)
{
    if (!handler)
    {
        handler = GetHandler(type);
    }

    ensure(!!handler);
    handler->init_config(&cfg);
}

extern bool send_open_home_menu_cmds();
extern void send_close_home_menu_cmds();
extern bool close_osk_from_ps_button();

void pad_thread::open_home_menu()
{
    // Check if the OSK is open and can be closed
    if (!close_osk_from_ps_button())
    {
        rsx::overlays::queue_message(get_localized_string(localized_string_id::CELL_OSK_DIALOG_BUSY));
        return;
    }

    if (auto manager = g_fxo->try_get<rsx::overlays::display_manager>())
    {
        if (m_home_menu_open.exchange(true))
        {
            return;
        }

        if (!send_open_home_menu_cmds())
        {
            m_home_menu_open = false;
            return;
        }

        input_log.notice("opening home menu...");

        const error_code result = manager->create<rsx::overlays::home_menu_dialog>()->show([this](s32 status)
                                                                                           {
                                                                                               input_log.notice("closing home menu with status %d", status);

                                                                                               m_home_menu_open = false;

                                                                                               send_close_home_menu_cmds();
                                                                                           });

        (result ? input_log.error : input_log.notice)("opened home menu with result %d", s32{result});
    }
}
#endif

void mouse_gyro_handler::clear()
{}

void mouse_gyro_handler::apply_gyro(const std::shared_ptr<Pad>& pad){}
void mouse_gyro_handler::set_enabled(bool enabled)
{}