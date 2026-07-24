// SPDX-License-Identifier: WTFPL

#ifndef APS3E_APS3E_RP3_IMPL_H
#define APS3E_APS3E_RP3_IMPL_H

#include <jni.h>
#include <assert.h>
#include <string.h>
#include <android/log.h>
#include <vector>
#include <android/native_window_jni.h>

#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>

#include <linux/prctl.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <thread>
#include <string_view>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include <stb_truetype.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma clang diagnostic ignored "-Wsign-compare"

#include "util/logs.hpp"
#include "util/video_provider.h"

#include "Input/product_info.h"
#include "Input/pad_thread.h"

#include "Utilities/cheat_info.h"
#include "Emu/System.h"
#include "Emu/Io/interception.h"
#include "Emu/RSX/RSXThread.h"
#include "Emu/Cell/lv2/sys_usbd.h"

#include "Emu/IdManager.h"

#include "Emu/RSX/RSXDisAsm.h"
#include "Emu/Cell/PPUAnalyser.h"
#include "Emu/Cell/PPUDisAsm.h"
#include "Emu/Cell/PPUThread.h"
#include "Emu/Cell/SPUDisAsm.h"
#include "Emu/Cell/SPUThread.h"
#include "Emu/CPU/CPUThread.h"
#include "Emu/CPU/CPUDisAsm.h"

#include "Emu/Io/Null/NullPadHandler.h"

#include "rpcs3_version.h"
#include "Emu/IdManager.h"
#include "Emu/VFS.h"
#include "Emu/vfs_config.h"
#include "Emu/system_utils.hpp"
#include "Emu/system_config.h"

#include "Emu/system_progress.hpp"
#include "Emu/Cell/Modules/cellGem.h"
#include "Emu/Cell/Modules/cellMsgDialog.h"
#include "Emu/Cell/Modules/cellOskDialog.h"
#include "Emu/Cell/Modules/sceNp.h"
#include "Emu/Cell/lv2/sys_sync.h"
#include "Emu/Cell/PPUFunction.h"

#include "Crypto/unpkg.h"
#include "Crypto/unself.h"
#include "Crypto/unzip.h"
#include "Crypto/decrypt_binaries.h"
#include "Crypto/sha1.h"


#include "Loader/iso.h"
#include "Loader/PUP.h"
#include "Loader/TAR.h"
#include "Loader/PSF.h"
#include "Loader/mself.hpp"

#include "Utilities/File.h"
#include "Utilities/Thread.h"
#include "util/sysinfo.hpp"
#include "util/serialization_ext.hpp"

#include "Emu/Io/PadHandler.h"

#include "Emu/Io/Null/null_music_handler.h"
#include "Emu/Io/Null/null_camera_handler.h"
#include "Emu/Io/Null/NullKeyboardHandler.h"
#include "Emu/Io/Null/NullMouseHandler.h"
#include "Emu/Io/KeyboardHandler.h"
#include "Emu/Io/MouseHandler.h"

#include "Emu/Audio/Null/NullAudioBackend.h"
#include "Emu/Audio/Cubeb/CubebBackend.h"
#include "Emu/Audio/Null/null_enumerator.h"
#include "Emu/Audio/Cubeb/cubeb_enumerator.h"

#include "Emu/RSX/GSFrameBase.h"

#include "Emu/Io/music_handler_base.h"

#include "Input/raw_mouse_handler.h"

#include "Emu/RSX/VK/VKGSRender.h"

#include "Input/sdl_pad_handler.h"

#include "Emu/Cell/Modules/cellSaveData.h"
#include "Emu/Cell/Modules/sceNpTrophy.h"

#include "Emu/Cell/Modules/cellMusic.h"

#include "Emu/RSX/Overlays/HomeMenu/overlay_home_menu.h"
#include "Emu/RSX/Overlays/overlay_message.h"

#include "yaml-cpp/yaml.h"

#include "Emu/RSX/Overlays/overlay_save_dialog.h"
#include "Emu/Cell/Modules/cellSysutil.h"
#include "Emu/RSX/Overlays/overlay_trophy_notification.h"
#include "Emu/RSX/Overlays/overlay_fonts.h"

#include "cpuinfo.h"
#include "meminfo.h"

#define LOG_TAG "aps3e_native"
#if 0

#define LOGI(...) {}
#define LOGW(...) {}
#define LOGE(...) {}
#define PR {}
#define PRE(f) {}

#else

#define LOGI(...) {      \
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,"%s : %d",__func__,__LINE__);\
	__android_log_print(ANDROID_LOG_INFO, LOG_TAG,__VA_ARGS__);\
}

#define LOGW(...) {      \
    __android_log_print(ANDROID_LOG_WARN, LOG_TAG,"%s : %d",__func__,__LINE__);\
	__android_log_print(ANDROID_LOG_WARN, LOG_TAG,__VA_ARGS__);\
}

#define LOGE(...) {      \
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,"%s : %d",__func__,__LINE__);\
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__);\
}

//#define PR {__android_log_print(ANDROID_LOG_WARN, LOG_TAG,"%s : %d",__func__,__LINE__);aps3e_log.notice("%s : %d",__func__,__LINE__);}
//#define PRE(f) {__android_log_print(ANDROID_LOG_WARN, LOG_TAG,"EmuCallbacks::%s : %d",f,__LINE__);aps3e_log.notice("EmuCallbacks::%s : %d",f,__LINE__);}

#define PR {}
#define PRE(f) {}

#endif

extern std::string localized_strings[];
extern void qt_events_aware_op(int repeat_duration_ms, std::function<bool()> wrapped_op);


class android_camera_handler final : public camera_handler_base
{
public:
    android_camera_handler(JavaVM *vm);
    virtual ~android_camera_handler();

    void open_camera() override;
    void close_camera() override;
    void start_camera() override;
    void stop_camera() override;
    void set_format(s32 format, u32 bytesize) override;
    void set_frame_rate(u32 frame_rate) override;
    void set_resolution(u32 width, u32 height) override;
    void set_mirrored(bool mirrored) override;
    u64 frame_number() const override;
    camera_handler_state get_image(u8* buf, u64 size, u32& width, u32& height, u64& frame_number, u64& bytes_read) override;

    void on_frame_available(void* data, int width, int height, int format);

private:
    void reset();
    bool init_java_camera();
    void cleanup_java_camera();
    JNIEnv* get_env(JavaVM *vm);

    mutable std::mutex frame_mutex_;
    std::vector<u8> frame_buffer_;
    int frame_width_ = 0;
    int frame_height_ = 0;
    atomic_t<u64> frame_number_{0};
    bool has_new_frame_ = false;

    jobject j_camera_instance_ = nullptr;
    jmethodID mth_open_ = nullptr;
    jmethodID mth_close_ = nullptr;
    jmethodID mth_start_ = nullptr;
    jmethodID mth_stop_ = nullptr;
    jmethodID mth_set_resolution_ = nullptr;
    jmethodID mth_set_format_ = nullptr;
    jmethodID mth_set_mirrored_ = nullptr;

    JavaVM *jvm_;
};

class AndroidVirtualPadHandler final : public PadHandlerBase
{
public:
    bool Init() override;

    AndroidVirtualPadHandler();

    /*void SetTargetWindow(QWindow* target);
    void processKeyEvent(QKeyEvent* event, bool pressed);
    void keyPressEvent(QKeyEvent* event);
    void keyReleaseEvent(QKeyEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseWheelEvent(QWheelEvent* event);

    bool eventFilter(QObject* target, QEvent* ev) override;*/

    void init_config(cfg_pad* cfg) override;
    std::vector<pad_list_entry> list_devices() override;
    connection get_next_button_press(const std::string& /*padId*/, const pad_callback& /*callback*/, const pad_fail_callback& /*fail_callback*/, gui_call_type /*call_type*/, const std::vector<std::string>& /*buttons*/) override { return connection::connected; }
    bool bindPadToDevice(std::shared_ptr<Pad> pad) override;
    void process() override;

    /*static std::string GetMouseName(const QMouseEvent* event);
    static std::string GetMouseName(u32 button);
    static QStringList GetKeyNames(const QKeyEvent* keyEvent);
    static std::string GetKeyName(const QKeyEvent* keyEvent, bool with_modifiers);
    static std::string GetKeyName(const u32& keyCode);*/
    static std::vector<std::set<u32>> GetKeyCombos(std::string_view cfg_string);
    //static u32 GetKeyCode(const QString& keyName);

    static int native_scan_code_from_string(const std::string& key);
    static std::string native_scan_code_to_string(int native_scan_code);

public:
    void Key(const u32 code, bool pressed, u16 value = 255);

private:
    //QWindow* m_target = nullptr;
    mouse_movement_mode m_mouse_movement_mode = mouse_movement_mode::relative;
    bool m_keys_released = false;
    bool m_mouse_move_used = false;
    bool m_mouse_wheel_used = false;
    bool get_mouse_lock_state() const;
    void release_all_keys();

    std::vector<Pad> m_pads_internal; // Accumulates input until the next poll. Only used for user input!

    // Button Movements
    steady_clock::time_point m_button_time;
    f32 m_analog_lerp_factor  = 1.0f;
    f32 m_trigger_lerp_factor = 1.0f;
    bool m_analog_limiter_toggle_mode = false;
    bool m_pressure_intensity_toggle_mode = false;
    u32 m_pressure_intensity_deadzone = 0;

    // Stick Movements
    steady_clock::time_point m_stick_time;
    f32 m_l_stick_lerp_factor = 1.0f;
    f32 m_r_stick_lerp_factor = 1.0f;
    u32 m_l_stick_multiplier = 100;
    u32 m_r_stick_multiplier = 100;

    static constexpr usz max_sticks = 4;
    std::array<u8, max_sticks> m_stick_min{ 0, 0, 0, 0 };
    std::array<u8, max_sticks> m_stick_max{ 128, 128, 128, 128 };
    std::array<u8, max_sticks> m_stick_val{ 128, 128, 128, 128 };

    // Mouse Movements
    steady_clock::time_point m_last_mouse_move_left;
    steady_clock::time_point m_last_mouse_move_right;
    steady_clock::time_point m_last_mouse_move_up;
    steady_clock::time_point m_last_mouse_move_down;
    int m_deadzone_x = 60;
    int m_deadzone_y = 60;
    double m_multi_x = 2;
    double m_multi_y = 2.5;

    // Mousewheel
    steady_clock::time_point m_last_wheel_move_up;
    steady_clock::time_point m_last_wheel_move_down;
    steady_clock::time_point m_last_wheel_move_left;
    steady_clock::time_point m_last_wheel_move_right;
};

class android_gs_frame:public GSFrameBase
{
public:
    ANativeWindow*& wnd;
    int& width;
    int& height;

    android_gs_frame(ANativeWindow*& wnd,int& w,int& h):wnd(wnd),width(w),height(h){PR;
        g_fxo->need<utils::video_provider>();
    }
    ~android_gs_frame(){PR;}

    void close(){PR;}
    void reset(){PR;}
    bool shown(){PR;return false;}
    void hide(){PR;}
    void show(){PR;}
    void toggle_fullscreen(){PR;}

    void delete_context(draw_context_t ctx){PR;}
    draw_context_t make_context(){PR;return nullptr;}
    void set_current(draw_context_t ctx){PR;}
    void flip(draw_context_t ctx, bool skip_frame = false){/*PR;*/}
    int client_width(){/*PR;*/return this->width;}
    int client_height(){/*PR;*/return this->height;}
    f64 client_display_rate(){PR;return 60.0;}
    bool has_alpha(){PR;return true;}

    display_handle_t handle() const{PR;return this->wnd;}

    bool can_consume_frame() const
    {PR;
        utils::video_provider& video_provider = g_fxo->get<utils::video_provider>();
        return video_provider.can_consume_frame();
    }

    void present_frame(std::vector<u8>&& data, u32 pitch, u32 width, u32 height, bool is_bgra) const
    {PR;
        utils::video_provider& video_provider = g_fxo->get<utils::video_provider>();
        video_provider.present_frame(std::move(data), pitch, width, height, is_bgra);
    }

    void take_screenshot(std::vector<u8>&& sshot_data, u32 sshot_width, u32 sshot_height, bool is_bgra) {PR;}
    //void take_screenshot(const std::vector<u8> sshot_data, u32 sshot_width, u32 sshot_height, bool is_bgra){PR;}
};

class android_save_dialog:public SaveDialogBase{
public:
    s32 ShowSaveDataList(const std::string& base_dir, std::vector<SaveDataEntry>& save_entries, s32 focused, u32 op, vm::ptr<CellSaveDataListSet> listSet, bool enable_overlay) override
    {
        LOGI("ShowSaveDataList(save_entries=%d, focused=%d, op=0x%x, listSet=*0x%x, enable_overlay=%d)", save_entries.size(), focused, op, listSet, enable_overlay);

        // TODO: Implement proper error checking in savedata_op?
        const bool use_end = sysutil_send_system_cmd(CELL_SYSUTIL_DRAWING_BEGIN, 0) >= 0;

        if (!use_end)
        {
            LOGE("ShowSaveDataList(): Not able to notify DRAWING_BEGIN callback because one has already been sent!");
        }

        // TODO: Install native shell as an Emu callback
        if (auto manager = g_fxo->try_get<rsx::overlays::display_manager>())
        {
            LOGI("ShowSaveDataList: Showing native UI dialog");

            const s32 result = manager->create<rsx::overlays::save_dialog>()->show(base_dir, save_entries, focused, op, listSet, enable_overlay);
            if (result != rsx::overlays::user_interface::selection_code::error)
            {
                LOGI("ShowSaveDataList: Native UI dialog returned with selection %d", result);
                if (use_end) sysutil_send_system_cmd(CELL_SYSUTIL_DRAWING_END, 0);
                return result;
            }
            LOGE("ShowSaveDataList: Native UI dialog returned error");
        }

        //if (!Emu.HasGui())
        {
            LOGI("ShowSaveDataList(): Aborting: Emulation has no GUI attached");
            if (use_end) sysutil_send_system_cmd(CELL_SYSUTIL_DRAWING_END, 0);
            return -2;
        }

        // Fall back to front-end GUI
        /*cellSaveData.notice("ShowSaveDataList(): Using fallback GUI");
        atomic_t<s32> selection = 0;

        input::SetIntercepted(true);

        Emu.BlockingCallFromMainThread([&]()
                                       {
                                           save_data_list_dialog sdid(save_entries, focused, op, listSet);
                                           sdid.exec();
                                           selection = sdid.GetSelection();
                                       });

        input::SetIntercepted(false);

        if (use_end) sysutil_send_system_cmd(CELL_SYSUTIL_DRAWING_END, 0);

        return selection.load();*/
    }
    ~android_save_dialog(){}
};

class android_trophy_notification:public TrophyNotificationBase
{
public:
    s32 ShowTrophyNotification(const SceNpTrophyDetails& trophy, const std::vector<uchar>& trophy_icon_buffer) override
    {
        if (auto manager = g_fxo->try_get<rsx::overlays::display_manager>())
        {
            // Allow adding more than one trophy notification. The notification class manages scheduling
            auto popup = std::make_shared<rsx::overlays::trophy_notification>();
            return manager->add(popup, false)->show(trophy, trophy_icon_buffer);
        }

        //if (!Emu.HasGui())
        {
            return 0;
        }
    }
    ~android_trophy_notification(){}
};

class dummy_msg_dialog:public MsgDialogBase
{
public:

    dummy_msg_dialog()=default;
    ~dummy_msg_dialog(){}

    void Create(const std::string& msg, const std::string& title) override
    {
        PRE("dummy_msg_dialog::Create");
        state = MsgDialogState::Open;
        Close(true);
        if(type.button_type.unshifted()==CELL_MSGDIALOG_TYPE_BUTTON_TYPE_YESNO)
            on_close(CELL_MSGDIALOG_BUTTON_YES);
        else if(type.button_type.unshifted()==CELL_MSGDIALOG_TYPE_BUTTON_TYPE_OK)
            on_close(CELL_MSGDIALOG_BUTTON_OK);

    }
    void Close(bool success) override
    {
        PRE("dummy_msg_dialog::Close");
    }
    void SetMsg(const std::string& msg) override
    {
        PRE("dummy_msg_dialog::SetMsg");
    }
    void ProgressBarSetMsg(u32 progressBarIndex, const std::string& msg) override
    {
        PRE("dummy_msg_dialog::ProgressBarSetMsg");
    }
    void ProgressBarReset(u32 progressBarIndex) override
    {
        PRE("dummy_msg_dialog::ProgressBarReset");
    }
    void ProgressBarInc(u32 progressBarIndex, u32 delta) override
    {
        PRE("dummy_msg_dialog::ProgressBarInc");
    }
    void ProgressBarSetValue(u32 progressBarIndex, u32 value) override
    {
        PRE("dummy_msg_dialog::ProgressBarSetValue");
    }
    void ProgressBarSetLimit(u32 index, u32 limit) override
    {
        PRE("dummy_msg_dialog::ProgressBarSetLimit");
    }
};

class dummy_osk_dialog:public OskDialogBase{
public:

    dummy_osk_dialog()=default;
    ~dummy_osk_dialog(){}

    void Create(const OskDialogBase::osk_params& params) override
    {
        PRE("dummy_osk_dialog::Create");
        state = OskDialogState::Open;
        //FIXME
        on_osk_close(CELL_OSKDIALOG_CLOSE_CANCEL);
    }
    void Clear(bool clear_all_data) override
    {
        PRE("dummy_osk_dialog::Clear");
    }
    void Insert(const std::u16string& text) override
    {
        PRE("dummy_osk_dialog::Insert");
    }
    void SetText(const std::u16string& text) override
    {
        PRE("dummy_osk_dialog::SetText");
    }
    void Close(s32 status) override
    {
        PRE("dummy_osk_dialog::Close");
    }
};

class dummy_send_message_dialog:public SendMessageDialogBase
{
public:
    error_code Exec(message_data& msg_data, std::set<std::string>& npids) override
    {
        PRE("dummy_send_message_dialog::Exec");
        return CELL_CANCEL;
    }
    void callback_handler(rpcn::NotificationType ntype, const std::string& username, bool status) override
    {
        PRE("dummy_send_message_dialog::callback_handler");
    }
};

class dummy_recv_message_dialog:public RecvMessageDialogBase
{
public:
    error_code Exec(SceNpBasicMessageMainType type, SceNpBasicMessageRecvOptions options, SceNpBasicMessageRecvAction& recv_result, u64& chosen_msg_id) override
    {
        PRE("dummy_recv_message_dialog::Exec");
        return CELL_CANCEL;
    }
    void callback_handler(const shared_ptr<std::pair<std::string, message_data>> new_msg, u64 msg_id) override
    {
        PRE("dummy_recv_message_dialog::callback_handler");
    }
};

class dummy_video_source : public video_source
{
public:
    dummy_video_source():video_source(){}
    virtual ~dummy_video_source(){}

    void set_video_path(const std::string& video_path) override{}
    void set_audio_path(const std::string& audio_path) override{}

    void get_image(std::vector<u8>& data, int& w, int& h, int& ch, int& bpp) override{}
    bool has_new() const override { return m_has_new; }

    void set_active(bool active) override { m_active = active; }
    bool get_active() const override { return m_active; }


protected:

    shared_mutex m_image_mutex;

    atomic_t<bool> m_active = false;
    atomic_t<bool> m_has_new = false;
};
#endif //APS3E_APS3E_RP3_IMPL_H
