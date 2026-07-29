// SPDX-License-Identifier: WTFPL
package aenu.aps3e;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.DialogFragment;

import androidx.preference.Preference;
import androidx.preference.PreferenceDataStore;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import org.json.JSONObject;

import aenu.preference.CheckBoxPreference;
import aenu.preference.ListPreference;
import aenu.preference.SeekBarPreference;


import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;

import java.io.InputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public class EmulatorSettings extends AppCompatActivity {

    static final String EXTRA_CONFIG_PATH="config_path";

    static final int REQUEST_CODE_SELECT_CUSTOM_DRIVER=6101;
    static final int REQUEST_CODE_SELECT_CUSTOM_FONT=6102;

    static final int WARNING_COLOR=0xffff8000;

    static final String Miscellaneous$Font_File_Selection="Miscellaneous|Font File Selection";
    static final String Miscellaneous$Custom_Font_File_Path="Miscellaneous|Custom Font File Path";
    static final String Video$Vulkan$Use_Custom_Driver="Video|Vulkan|Use Custom Driver";
    static final String Video$Vulkan$Custom_Driver_Library_Path="Video|Vulkan|Custom Driver Library Path";
    //static final String Video$Use_BGRA_Format="Video|Use BGRA Format";
    //static final String Video$Force_Convert_Texture="Video|Force Convert Texture";
    //static final String Video$Texture_Upload_Mode ="Video|Texture Upload Mode";
    static final String Core$Use_LLVM_CPU="Core|Use LLVM CPU";
    static final String Video$Write_Color_Buffers="Video|Write Color Buffers";
    static final String Video$Read_Color_Buffers="Video|Read Color Buffers";
    static final String Video$Strict_Rendering_Mode="Video|Strict Rendering Mode";
    static final String Video$Resolution_Scale="Video|Resolution Scale";
    static final String Video$Vulkan$Asynchronous_Texture_Streaming="Video|Vulkan|Asynchronous Texture Streaming";
    static final String Video$Vulkan$Asynchronous_Queue_Scheduler="Video|Vulkan|Asynchronous Queue Scheduler";
    static final String Video$Resolution="Video|Resolution";
    //"Video|Aspect ratio"
    static final String Video$Aspect_ratio="Video|Aspect ratio";
    //"Core|Thread Affinity Mask"
    static final String Core$Thread_Affinity_Mask="Core|Thread Affinity Mask";
    //"Core|Thread Scheduler Mode"
    static final String Core$Thread_Scheduler_Mode="Core|Thread Scheduler Mode";
    @FunctionalInterface
    public static interface InstallCallback {
        void onInstallComplete(String path);
    }
    @SuppressLint("ValidFragment")
    public static class SettingsFragment extends PreferenceFragmentCompat implements
            Preference.OnPreferenceClickListener,Preference.OnPreferenceChangeListener{

        static final String SS_CURRENT_SCREEN="current_screen";
        boolean is_global;
        String config_path;
        Emulator.Config original_config;
        Emulator.Config config;
        PreferenceScreen root_pref;
        String current_screen_key;

        static SettingsFragment new_instance(String config_path,boolean is_global){
            SettingsFragment frag=new SettingsFragment();
            Bundle args=new Bundle();
            args.putString("config_path",config_path);
            args.putBoolean("is_global",is_global);
            frag.setArguments(args);
            return frag;
        }

        OnBackPressedCallback back_callback=new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                String current=SettingsFragment.this.getPreferenceScreen().getKey();
                if (current==null){
                    requireActivity().finish();
                    return;
                }
                int p=current.lastIndexOf('|');
                if (p==-1)
                    setPreferenceScreen(root_pref);
                else
                    setPreferenceScreen(root_pref.findPreference(current.substring(0,p)));
            }
        };

        final PreferenceDataStore data_store=new PreferenceDataStore(){

            public void putString(String key, @Nullable String value) {
                config.save_config_entry(key,value);
            }

            public void putStringSet(String key, @Nullable Set<String> values) {
                throw new UnsupportedOperationException("Not implemented on this data store");
            }

            public void putInt(String key, int value) {
                config.save_config_entry(key,Integer.toString(value));
            }

            public void putLong(String key, long value) {
                throw new UnsupportedOperationException("Not implemented on this data store");
            }

            public void putFloat(String key, float value) {
                throw new UnsupportedOperationException("Not implemented on this data store");
            }

            public void putBoolean(String key, boolean value) {
                config.save_config_entry(key,Boolean.toString(value));
            }

            @Nullable
            public String getString(String key, @Nullable String defValue) {
                return config.load_config_entry(key);
            }

            @Nullable
            public Set<String> getStringSet(String key, @Nullable Set<String> defValues) {
                //return defValues;
                throw new UnsupportedOperationException("Not implemented on this data store");
            }

            public int getInt(String key, int defValue) {
                String v=config.load_config_entry(key);
                return v!=null?Integer.parseInt(v):defValue;
            }

            public long getLong(String key, long defValue) {
                throw new UnsupportedOperationException("Not implemented on this data store");
            }

            public float getFloat(String key, float defValue) {
                throw new UnsupportedOperationException("Not implemented on this data store");
            }

            public boolean getBoolean(String key, boolean defValue) {
                String v=config.load_config_entry(key);
                return v!=null?Boolean.parseBoolean(v):defValue;
            }
        };

        Preference reset_as_default_pref(File _config_file){
            Preference p=new Preference(requireContext());
            p.setTitle(R.string.reset_as_default);
            p.setIconSpaceReserved(false);
            p.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener(){
                public boolean onPreferenceClick(@NonNull Preference preference) {
                    new AlertDialog.Builder(requireContext())
                            .setMessage(getString(R.string.reset_as_default)+"?")
                            .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                                @Override
                                public void onClick(DialogInterface dialog, int which) {
                                    if (config!=null) {
                                        config.close_config_file();
                                        config=null;
                                    }
                                    if(original_config!=null){
                                        original_config.close_config();
                                        original_config=null;
                                    }
                                    Utils.copy_file(Application.get_default_config_file(),_config_file);
                                    requireActivity().finish();
                                }
                            })
                            .setNegativeButton(android.R.string.cancel, null)
                            .create().show();
                    return true;
                }
            });
            return p;
        }

        Preference reset_as_global_pref(){
            Preference p=new Preference(requireContext());
            p.setTitle(R.string.use_global_config);
            p.setIconSpaceReserved(false);
            p.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener(){
                public boolean onPreferenceClick(@NonNull Preference preference) {
                    new AlertDialog.Builder(requireContext())
                            .setMessage(getString(R.string.use_global_config)+"?")
                            .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                                @Override
                                public void onClick(DialogInterface dialog, int which) {
                                    if (config!=null) {
                                        config.close_config_file();
                                        config=null;
                                    }
                                    if(original_config!=null){
                                        original_config.close_config();
                                        original_config=null;
                                    }

                                    new File(config_path).delete();
                                    requireActivity().finish();
                                }
                            })
                            .setNegativeButton(android.R.string.cancel, null)
                            .create().show();
                    return true;
                }
            });
            return p;
        }

        Preference apply_database_config_pref(){
            Preference p=new Preference(requireContext());
            p.setTitle(R.string.apply_recommended_config);
            p.setIconSpaceReserved(false);
            p.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener(){
                public boolean onPreferenceClick(@NonNull Preference preference) {
                    new AlertDialog.Builder(requireContext())
                            .setMessage(getString(R.string.apply_recommended_config_confirm))
                            .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                                @Override
                                public void onClick(DialogInterface dialog, int which) {
                                    apply_database_config();
                                }
                            })
                            .setNegativeButton(android.R.string.cancel, null)
                            .create().show();
                    return true;
                }
            });
            return p;
        }

        void apply_database_config(){
            try{
                String serial;
                {
                    String yml_name=new File(config_path).getName();
                    serial=yml_name.substring(0,yml_name.length()-4);
                }

                byte[] json_bytes=Utils.load_assets_file(requireContext(),"database_config.json");
                JSONObject db=new JSONObject(new String(json_bytes));
                if(!db.has(serial)){
                    Toast.makeText(requireContext(),R.string.apply_recommended_config_not_found,Toast.LENGTH_SHORT).show();
                    return;
                }

                String config_yaml=db.getJSONObject(serial).getString("config");

                Emulator.Config db_cfg=Emulator.Config.open_config_from_string(config_yaml);

                final String[] BOOL_KEYS={
                        "Audio|Enable Buffering",
                        "Audio|Enable Time Stretching",
                        "Core|Accurate RSX reservation access",
                        "Core|Accurate SPU DMA",
                        "Core|Debug Console Mode",
                        "Core|Disable SPU GETLLAR Spin Optimization",
                        "Core|SPU loop detection",
                        "Video|Accurate ZCULL stats",
                        "Video|Disable Vertex Cache",
                        "Video|Emulate Special Depth Comparison",
                        "Video|Handle RSX Memory Tiling",
                        "Video|Multithreaded RSX",
                        "Video|Read Color Buffers",
                        "Video|Read Depth Buffer",
                        "Video|Relaxed ZCULL Sync",
                        "Video|Strict Rendering Mode",
                        "Video|Vblank NTSC Fixup",
                        "Video|Vulkan|Asynchronous Texture Streaming",
                        "Video|Write Color Buffers",
                        "Video|Write Depth Buffer",
                };

                final String[] INT_KEYS={
                        "Core|MFC Commands Shuffling Limit",
                        "Core|Max SPURS Threads",
                        "Video|Anisotropic Filter Override",
                };

                final String[] STR_KEYS={
                        "Audio|Audio Format",
                        "Audio|Microphone Type",
                        "Core|Libraries Control|- libaudio.sprx",
                        "Core|Libraries Control|- libvdec.sprx",
                        "Core|PPU Decoder",
                        "Core|RSX FIFO Fetch Accuracy",
                        "Core|SPU Block Size",
                        "Core|SPU Decoder",
                        "Core|SPU XFloat Accuracy",
                        "Core|Sleep Timers Accuracy",
                        "Input/Output|Camera",
                        "Input/Output|Camera type",
                        "Input/Output|Keyboard",
                        "Input/Output|Mouse",
                        "Input/Output|Move",
                        "Video|Frame limit",
                        "Video|MSAA",
                        "Video|Renderer",
                        "Video|Resolution",
                        "Video|Shader Mode",
                        "Video|Shader Precision",
                        "Video|VSync Mode",
                };

                final String[] ARR_KEYS={
                        "Core|Libraries Control",
                };

                for(String key:BOOL_KEYS){
                    String val_str=db_cfg.load_config_entry(key);
                    if(val_str!=null){
                        CheckBoxPreference pref=findPreference(key);
                        config.save_config_entry(key,val_str);
                        pref.setChecked(Boolean.parseBoolean(val_str));
                        setup_pref_title_color(pref,val_str);
                        setup_config_dependency(pref,val_str);

                    }
                }

                for(String key:INT_KEYS){
                    String val_str=db_cfg.load_config_entry(key);
                    if(val_str!=null){
                        SeekBarPreference pref=findPreference(key);
                        config.save_config_entry(key,val_str);
                        try {
                            int val = Integer.parseInt(val_str);
                            pref.setValue(val);
                            setup_pref_title_color(pref,val_str);
                            setup_config_dependency(pref,val_str);
                        } catch (NumberFormatException e) {
                            pref.setEnabled(false);
                        }
                    }
                }

                for(String key:STR_KEYS){
                    String val_str=db_cfg.load_config_entry(key);
                    if(val_str!=null){
                        config.save_config_entry(key,val_str);
                        ListPreference pref=findPreference(key);
                        pref.setValue(val_str);
                        setup_pref_title_color(pref,val_str);
                        setup_config_dependency(pref,val_str);
                    }
                }

                for(String key:ARR_KEYS){
                    String[] val=db_cfg.load_config_entry_ty_arr(key);
                    if(val!=null){
                        config.save_config_entry_ty_arr(key,val);
                    }
                }

                Toast.makeText(requireContext(),R.string.apply_recommended_config_success,Toast.LENGTH_SHORT).show();
            }catch(Exception e){
                Log.e("EmulatorSettings",e.toString());
                Toast.makeText(requireContext(),R.string.apply_recommended_config_failed,Toast.LENGTH_SHORT).show();
            }
        }

        public void setPreferenceScreen(PreferenceScreen preferenceScreen){
            super.setPreferenceScreen(preferenceScreen);
            current_screen_key=preferenceScreen.getKey();
            CharSequence title=preferenceScreen.getTitle();
            if(title==null)
                title=getString(R.string.settings);
            //requireActivity().setTitle(title);
            EmulatorSettings settings=(EmulatorSettings) requireActivity();
            if(settings.getSupportActionBar()!=null) {
                settings.getSupportActionBar().setTitle(title);
            }
        }

        @Override
        public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {

            this.config_path=getArguments().getString("config_path");
            this.is_global=getArguments().getBoolean("is_global");

            setPreferencesFromResource(R.xml.emulator_settings, null);
            root_pref=getPreferenceScreen();

            if(is_global) {
                root_pref.addPreference(reset_as_default_pref(Application.get_global_config_file()));
            }
            else{
                root_pref.addPreference(apply_database_config_pref());
                root_pref.addPreference(reset_as_default_pref(new File(config_path)));
                root_pref.addPreference(reset_as_global_pref());
            }

            requireActivity().getOnBackPressedDispatcher().addCallback(back_callback);

            if(!new File(config_path).exists()){
                root_pref.setEnabled(false);
                return;
            }

            try{
                config=Emulator.Config.open_config_file(config_path);
                original_config=Emulator.Config.open_config_from_string(QuickStartActivity.load_default_config_str(getContext()));
            }catch(Exception e){
                Log.e("EmulatorSettings",e.toString());
                root_pref.setEnabled(false);
                return;
            }





            final String[] BOOL_KEYS={
                    "Core|PPU Debug",
                    "Core|PPU Calling History",
                    "Core|Save LLVM logs",
                    "Core|LLVM Precompilation",
                    "Core|Set DAZ and FTZ",
                    "Core|SPU Reservation Busy Waiting Enabled",
                    "Core|Disable SPU GETLLAR Spin Optimization",
                    "Core|SPU Debug",
                    "Core|MFC Debug",
                    "Core|SPU loop detection",
                    "Core|Accurate SPU DMA",
                    "Core|Accurate SPU Reservations",
                    "Core|Accurate Cache Line Stores",
                    "Core|Accurate RSX reservation access",
                    "Core|SPU Verification",
                    "Core|SPU Cache",
                    "Core|SPU Profiler",
                    "Core|PPU Profiler",
                    "Core|MFC Commands Shuffling In Steps",
                    "Core|Precise SPU Verification",
                    "Core|PPU LLVM Java Mode Handling",
                    "Core|PPU Vector NaN Handling",
                    "Core|Use Accurate DFMA",
                    "Core|PPU Set Saturation Bit",
                    "Core|PPU Accurate Non-Java Mode",
                    "Core|PPU Accurate Vector NaN Values",
                    "Core|PPU Set FPCC Bits",
                    "Core|Debug Console Mode",
                    "Core|Hook static functions",
                    "Core|HLE lwmutex",
                    "Core|Allow RSX CPU Preemptions",
                    "Core|Enable Performance Report",
                    "Core|Assume External Debugger",
                    Video$Write_Color_Buffers,
                    "Video|Write Depth Buffer",
                    Video$Read_Color_Buffers,
                    "Video|Read Depth Buffer",
                    "Video|Handle RSX Memory Tiling",
                    "Video|Log shader programs",
                    "Video|Debug output",
                    "Video|Debug overlay",
                    "Video|Renderdoc Compatibility Mode",
                    "Video|Use GPU texture scaling",
                    "Video|Stretch To Display Area",
                    "Video|Force High Precision Z buffer",
                    Video$Strict_Rendering_Mode,
                    "Video|Disable ZCull Occlusion Queries",
                    "Video|Disable Video Output",
                    "Video|Disable Vertex Cache",
                    "Video|Disable FIFO Reordering",
                    "Video|Enable Frame Skip",
                    "Video|Force CPU Blit",
                    "Video|Disable On-Disk Shader Cache",
                    "Video|Disable Vulkan Memory Allocator",
                    "Video|Use full RGB output range",
                    "Video|Strict Texture Flushing",
                    "Video|Multithreaded RSX",
                    "Video|Relaxed ZCULL Sync",
                    "Video|Force Hardware MSAA Resolve",
                    "Video|3D Display Enabled",
                    "Video|Debug Program Analyser",
                    "Video|Accurate ZCULL stats",
                    "Video|Vblank NTSC Fixup",
                    "Video|DECR memory layout",
                    "Video|Allow Host GPU Labels",
                    "Video|Disable Asynchronous Memory Manager",
                    "Video|Record With Overlays",
                    "Video|Disable Hardware ColorSpace Remapping",
                    "Video|Vulkan|Force primitive restart flag",
                    //"Video|Vulkan|Asynchronous Texture Streaming",
                    Video$Vulkan$Asynchronous_Texture_Streaming,
                    "Video|Vulkan|Use Re-BAR for GPU uploads",
                    //"Video|Vulkan|Use Custom Driver",
                    Video$Vulkan$Use_Custom_Driver,
                    "Video|Vulkan|Custom Driver Force Max Clocks",
                    "Video|Performance Overlay|Enabled",
                    "Video|Performance Overlay|Enable Framerate Graph",
                    "Video|Performance Overlay|Enable Frametime Graph",
                    "Video|Performance Overlay|Center Horizontally",
                    "Video|Performance Overlay|Center Vertically",
                    "Video|Performance Overlay|Use Window Space",
                    "Video|Shader Loading Dialog|Allow custom background",
                    "Audio|Dump to file",
                    "Audio|Convert to 16 bit",
                    "Audio|Enable Buffering",
                    "Audio|Enable Time Stretching",
                    "Audio|Disable Sampling Skip",
                    "Input/Output|Keep pads connected",
                    "Input/Output|Background input enabled",
                    "Input/Output|Show move cursor",
                    "Input/Output|Paint move spheres",
                    "Input/Output|Allow move hue set by game",
                    "Input/Output|Lock overlay input to player one",
                    "Input/Output|Load SDL GameController Mappings",
                    "Input/Output|IO Debug overlay",
                    "Input/Output|Mouse Debug overlay",
                    "Savestate|Start Paused",
                    "Savestate|Suspend Emulation Savestate Mode",
                    "Savestate|Compatible Savestate Mode",
                    "Savestate|Inspection Mode Savestates",
                    "Savestate|Save Disc Game Data",
                    "Miscellaneous|Automatically start games after boot",
                    "Miscellaneous|Exit RPCS3 when process finishes",
                    "Miscellaneous|Pause emulation on RPCS3 focus loss",
                    "Miscellaneous|Start games in fullscreen mode",
                    "Miscellaneous|Prevent display sleep while running games",
                    "Miscellaneous|Show trophy popups",
                    "Miscellaneous|Show RPCN popups",
                    "Miscellaneous|Show shader compilation hint",
                    "Miscellaneous|Show PPU compilation hint",
                    "Miscellaneous|Show autosave/autoload hint",
                    "Miscellaneous|Show pressure intensity toggle hint",
                    "Miscellaneous|Show analog limiter toggle hint",
                    "Miscellaneous|Show mouse and keyboard toggle hint",
                    "Miscellaneous|Show fatal error hints",
                    "Miscellaneous|Show capture hints",
                    "Miscellaneous|Use native user interface",
                    "Miscellaneous|Silence All Logs",
                    "Miscellaneous|Pause Emulation During Home Menu",
                    "Miscellaneous|Play music during boot sequence",
                    "Miscellaneous|Enable GameMode",
            };
            final String[] INT_KEYS={
                    "Core|PPU Threads",
                    "Core|Max LLVM Compile Threads",
                    "Core|Preferred SPU Threads",
                    "Core|SPU delay penalty",
                    "Core|Max SPURS Threads",
                    "Core|Accurate PPU 128-byte Reservation Op Max Length",
                    "Core|Stub PPU Traps",
                    "Core|Clocks scale",
                    "Core|Usleep Time Addend",
                    "Video|Second Frame Limit",
                    "Video|Screen size in inches",
                    "Video|Consecutive Frames To Draw",
                    "Video|Consecutive Frames To Skip",
                    "Video|Texture LOD Bias Addend",
                    "Video|Shader Compiler Threads",
                    "Video|Driver Recovery Timeout",
                    "Video|Vblank Rate",
                    "Video|Performance Overlay|Horizontal Margin (%)",
                    "Video|Performance Overlay|Vertical Margin (%)",
                    "Audio|Master Volume",
                    "Audio|Desired Audio Buffer Duration",
                    "Audio|Time Stretching Threshold",
                    "System|Console time offset (s)",
                    "Core|SPU Reservation Busy Waiting Percentage 1",
                    "Core|SPU GETLLAR Busy Waiting Percentage",
                    "Core|MFC Commands Shuffling Limit",
                    "Core|MFC Commands Timeout",
                    "Core|SPU Wake-Up Delay",
                    "Core|SPU Wake-Up Delay Thread Mask",
                    "Core|Max CPU Preempt Count",
                    "Core|Performance Report Threshold",
                    "Video|Resolution Scale",
                    "Video|Anisotropic Filter Override",
                    "Video|Minimum Scalable Dimension",
                    "Video|Driver Wake-Up Delay",
                    "Video|FidelityFX CAS Sharpening Intensity",
                    "Video|Vulkan|VRAM allocation limit (MB)",
                    "Video|Performance Overlay|Framerate datapoints",
                    "Video|Performance Overlay|Frametime datapoints",
                    "Video|Performance Overlay|Metrics update interval (ms)",
                    "Video|Performance Overlay|Font size (px)",
                    "Video|Performance Overlay|Opacity (%)",
                    "Video|Shader Loading Dialog|Darkening effect strength",
                    "Video|Shader Loading Dialog|Blur effect strength",
                    "Audio|Audio Formats",
                    "Input/Output|Pad handler sleep (microseconds)",
                    "Input/Output|Fake Move Rotation Cone",
                    "Input/Output|Fake Move Rotation Cone (Vertical)",
                    "Savestate|Maximum SaveState Files",
                    "Savestate|Maximum SaveState Files Space (MiB)",
            };
            final String[] STRING_ARR_KEYS={
                    "Core|PPU Decoder",
                    //"Core|Thread Scheduler Mode",
                    Core$Thread_Scheduler_Mode,
                    "Core|SPU Decoder",
                    "Core|SPU Block Size",
                    "Core|RSX FIFO Fetch Accuracy",
                    "Core|SPU XFloat Accuracy",
                    "Core|Sleep Timers Accuracy",
                    "Video|Renderer",
                    //"Video|Resolution",
                    Video$Resolution,
                    //"Video|Aspect ratio",
                    Video$Aspect_ratio,
                    "Video|Frame limit",
                    "Video|MSAA",
                    "Video|Shader Mode",
                    "Video|Shader Precision",
                    "Video|VSync Mode",
                    "Video|3D Display Mode",
                    "Video|Output Scaling Mode",
                    "Video|Vertex Buffer Upload Mode",
                    "Video|Vulkan|Exclusive Fullscreen Mode",
                    //"Video|Vulkan|Asynchronous Queue Scheduler",
                    Video$Vulkan$Asynchronous_Queue_Scheduler,
                    "Video|Performance Overlay|Detail level",
                    "Video|Performance Overlay|Framerate graph detail level",
                    "Video|Performance Overlay|Frametime graph detail level",
                    "Video|Performance Overlay|Position",
                    "Audio|Renderer",
                    "Audio|Audio Provider",
                    "Audio|RSXAudio Avport",
                    "Audio|Audio Format",
                    "Audio|Audio Channel Layout",
                    "Audio|Microphone Type",
                    "Audio|Music Handler",
                    "Input/Output|Keyboard",
                    "Input/Output|Mouse",
                    "Input/Output|Camera",
                    "Input/Output|Camera type",
                    "Input/Output|Camera flip",
                    "Input/Output|Move",
                    "Input/Output|Buzz emulated controller",
                    "Input/Output|Turntable emulated controller",
                    "Input/Output|GHLtar emulated controller",
                    "Input/Output|Pad handler mode",
                    "System|License Area",
                    "System|Language",
                    "System|Keyboard Type",
                    "System|Enter button assignment",
                    "System|Date Format",
                    "System|Time Format",
                    //"Miscellaneous|Font File Selection",
                    Miscellaneous$Font_File_Selection,
            };
            final String[] NODE_KEYS={
                    "Core",
                    "Video",
                    "Video|Vulkan",
                    "Video|Performance Overlay",
                    "Video|Shader Loading Dialog",
                    "Audio",
                    "Input/Output",
                    "System",
                    "Savestate",
                    "Miscellaneous",
            };


            for (String key:BOOL_KEYS){
                CheckBoxPreference pref=findPreference(key);
                String val_str=config.load_config_entry(key);
                if (val_str!=null) {
                    boolean val=Boolean.parseBoolean(val_str);
                    pref.setChecked(val);
                    setup_pref_title_color(pref,val_str);
                    setup_config_dependency(pref,val_str);
                }
                pref.setOnPreferenceChangeListener(this);
                pref.setPreferenceDataStore(data_store);
            }

            for (String key:INT_KEYS){
                SeekBarPreference pref=findPreference(key);
                String val_str=config.load_config_entry(key);
                if (val_str!=null) {
                    //FIXME
                    try {
                        int val = Integer.parseInt(val_str);
                        pref.setValue(val);
                        setup_pref_title_color(pref,val_str);
                        setup_config_dependency(pref,val_str);
                    } catch (NumberFormatException e) {
                        pref.setEnabled(false);
                    }
                }

                pref.setOnPreferenceChangeListener(this);
                pref.setPreferenceDataStore(data_store);
            }

            /* Preference.OnPreferenceChangeListener list_pref_change_listener=new Preference.OnPreferenceChangeListener() {
                @Override
                public boolean onPreferenceChange(@NonNull Preference preference, Object newValue) {
                    ListPreference pref=(ListPreference) preference;
                    CharSequence value=(CharSequence) newValue;
                    CharSequence[] values=pref.getEntryValues();
                    CharSequence[] entries=pref.getEntries();
                    for (int i=0;i<values.length;i++){
                        if (values[i].equals(value)){
                            pref.setSummary(entries[i]);
                            break;
                        }
                    }
                    return true;
                }
            };*/
            for (String key:STRING_ARR_KEYS){
                ListPreference pref=findPreference(key);
                String val_str=config.load_config_entry(key);
                if (val_str!=null) {
                    pref.setValue(val_str);
                    pref.setSummary(pref.getEntry());
                    setup_pref_title_color(pref,val_str);
                    setup_config_dependency(pref,val_str);
                }
                pref.setOnPreferenceChangeListener(this);
                pref.setPreferenceDataStore(data_store);
            }

            for (String key:NODE_KEYS){
                PreferenceScreen pref=findPreference(key);
                pref.setOnPreferenceClickListener(this);
            }

            findPreference("Core|Libraries Control").setOnPreferenceClickListener(this);

            String val;
            findPreference(Core$Use_LLVM_CPU).setOnPreferenceClickListener(this);
            if((val=config.load_config_entry(Core$Use_LLVM_CPU))!=null) findPreference(Core$Use_LLVM_CPU).setSummary(val);

            findPreference("Video|Vulkan|Adapter").setOnPreferenceClickListener(this);
            if((val=config.load_config_entry("Video|Vulkan|Adapter"))!=null) findPreference("Video|Vulkan|Adapter").setSummary(val);

            findPreference(Video$Vulkan$Custom_Driver_Library_Path).setOnPreferenceClickListener(this);
            findPreference(Miscellaneous$Custom_Font_File_Path).setOnPreferenceClickListener(this);
            findPreference(Core$Thread_Affinity_Mask).setOnPreferenceClickListener(this);

            setup_costom_driver_library_path(null);
            setup_costom_font_path(null);

            if(!Emulator.get.support_custom_driver()){
                findPreference(Video$Vulkan$Use_Custom_Driver).setEnabled(false);
                findPreference(Video$Vulkan$Custom_Driver_Library_Path).setEnabled(false);
                findPreference("Video|Vulkan|Custom Driver Force Max Clocks").setEnabled(false);
                //return;
            }

            /*String saved_screen=null;
            if(savedInstanceState!=null&&(saved_screen=savedInstanceState.getString(SS_CURRENT_SCREEN))!=null){
                final Preference current= root_pref.findPreference(saved_screen);
                setPreferenceScreen((PreferenceScreen) current);
                Toast.makeText(getActivity(),"Restoring screen: "+saved_screen,Toast.LENGTH_SHORT).show();
            }*/
        }

        @Override
        public void onSaveInstanceState(@NonNull Bundle outState) {
            super.onSaveInstanceState(outState);
            //outState.putString(SS_CURRENT_SCREEN,current_screen_key);
        }

        @Override
    public void onDisplayPreferenceDialog( @NonNull Preference pref) {

        if (pref instanceof SeekBarPreference) {
            final DialogFragment f = SeekBarPreference.SeekBarPreferenceFragmentCompat.newInstance(pref.getKey());
            f.setTargetFragment(this, 0);
            f.show(getParentFragmentManager(), "DIALOG_FRAGMENT_TAG");
            return;
        }
        super.onDisplayPreferenceDialog(pref);
    }



        void setup_costom_driver_library_path(String new_path) {
            final String key=Video$Vulkan$Custom_Driver_Library_Path;
            if(new_path==null){
                String val_str=config.load_config_entry(key);
                if (val_str!=null) {
                    findPreference(key).setSummary(val_str);
                }
                return;
            }

            config.save_config_entry(key,new_path);
            findPreference(key).setSummary(new_path);
        }

        void setup_costom_font_path(String new_path) {
            final String key=Miscellaneous$Custom_Font_File_Path;
            if(new_path==null){
                String val_str=config.load_config_entry(key);
                if (val_str!=null) {
                    findPreference(key).setSummary(val_str);
                }
                return;
            }

            config.save_config_entry(key,new_path);
            findPreference(key).setSummary(new_path);
        }

        @Override
        public void onDestroy() {
            super.onDestroy();
            if (config!=null)
                config.close_config_file();
        }

        /*@Override
        public boolean onPreferenceChange(Preference preference, Object newValue) {
            Log.i("onPreferenceChange",preference.getKey()+" "+newValue);
            if (preference instanceof CheckBoxPreference){
                config.save_config_entry(preference.getKey(),newValue.toString());
            }else if (preference instanceof ListPreference){
                config.save_config_entry(preference.getKey(),newValue.toString());
            }else if (preference instanceof SeekBarPreference){
                config.save_config_entry(preference.getKey(),newValue.toString());
            }
            return true;
        }*/

        void request_select_custom_driver_file(){
            Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            ((Activity)requireActivity()).startActivityForResult(intent, REQUEST_CODE_SELECT_CUSTOM_DRIVER);
        }

        void request_select_font_file(){
            Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            ((Activity)requireActivity()).startActivityForResult(intent, REQUEST_CODE_SELECT_CUSTOM_FONT);
        }

        @Override
        public boolean onPreferenceClick(@NonNull Preference preference) {

            if(preference.getKey().equals(Core$Use_LLVM_CPU)){
                show_select_llvm_cpu_list();
                return false;
            }

            if(preference.getKey().equals("Video|Vulkan|Adapter")){
                show_select_vulkan_phy_dev_list();
                return false;
            }

            if(preference.getKey().equals(Video$Vulkan$Custom_Driver_Library_Path)){
                show_select_custom_driver_list();
                return false;
            }
            if(preference.getKey().equals(Miscellaneous$Custom_Font_File_Path)){
                //request_select_font_file();
                show_select_font_file_list();
                return false;
            }

            if(preference.getKey().equals(Core$Thread_Affinity_Mask)){
                show_affinity_mask_view();
                return false;
            }
            if (preference.getKey().equals("Core|Libraries Control")){
                show_library_control_view();
                return false;
            }
            if(preference instanceof PreferenceScreen){
                setPreferenceScreen(root_pref.findPreference(preference.getKey()));
                return false;
            }
            return false;
        }

        void create_list_dialog(String title, String[] items, DialogInterface.OnClickListener listener){
            AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
            builder.setTitle(title)
                    .setItems(items, listener)
                    .setNegativeButton(android.R.string.cancel, null);
            builder.create().show();
        }

        void show_select_llvm_cpu_list(){
            String[] items=Emulator.get.get_support_llvm_cpu_list();
            create_list_dialog(getString(R.string.emulator_settings_core_use_llvm_cpu)
                    , items, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    dialog.dismiss();
                    config.save_config_entry(Core$Use_LLVM_CPU,items[which]);
                    findPreference(Core$Use_LLVM_CPU).setSummary(items[which]);
                }
            });
        }

        void show_select_vulkan_phy_dev_list(){
            String[] items=Emulator.get.get_vulkan_physical_dev_list();
            create_list_dialog(getString(R.string.emulator_settings_video_vulkan_adapter)
                    , items, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    dialog.dismiss();
                    config.save_config_entry("Video|Vulkan|Adapter",items[which]);
                    findPreference("Video|Vulkan|Adapter").setSummary(items[which]);
                }
                    }
            );
        }

        void show_select_custom_driver_list(){
            File[] files=Application.get_custom_driver_dir().listFiles();
            if(files==null||files.length==0){
                create_list_dialog(getString(R.string.emulator_settings_video_vulkan_custom_driver_library_path_dialog_title)
                        , new String[]{getString(R.string.emulator_settings_video_vulkan_custom_driver_library_path_dialog_add_hint)}, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                        request_select_custom_driver_file();
                    }
                });
                return;
            }

            String  items[]=new String[files.length+1];
            for(int i=0;i<files.length;i++){
                if(files[i].isFile())
                    items[i]=files[i].getName();
                else{
                    File[] sub_files=files[i].listFiles();
                    if(sub_files.length==1)
                        items[i]=files[i].getName()+"/"+sub_files[0].getName();
                    else{
                        File json_f=new File(files[i], "meta.json");
                        if(json_f.exists()){
                            try {
                                JSONObject json = new JSONObject(Utils.read_file_as_str(json_f));
                                items[i]=files[i].getName()+"/"+json.getString("libraryName");
                            } catch (Exception e) {
                                items[i]="";
                            }
                        }
                        else
                            items[i]="";
                    }
                }
            }
            items[files.length]=getString(R.string.emulator_settings_video_vulkan_custom_driver_library_path_dialog_add_hint);
            create_list_dialog(getString(R.string.emulator_settings_video_vulkan_custom_driver_library_path_dialog_title), items, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    dialog.dismiss();
                    if(which==files.length){
                        request_select_custom_driver_file();
                    }else{
                        //setup_costom_driver_library_path(files[which].getAbsolutePath());
                        File f=new File(files[which].getParentFile(),items[which]);
                        setup_costom_driver_library_path(f.getAbsolutePath());
                    }
                }
            });
        }

        void show_select_font_file_list(){
            File[] files=Application.get_custom_font_dir().listFiles();
            if(files==null||files.length==0){
                create_list_dialog(getString(R.string.emulator_settings_miscellaneous_custom_font_file_path_dialog_title)
                        , new String[]{getString(R.string.emulator_settings_miscellaneous_custom_font_file_path_dialog_add_hint)}, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                        request_select_font_file();
                    }
                });
                return;
            }

            String  items[]=new String[files.length+1];
            for(int i=0;i<files.length;i++){
                items[i]=files[i].getName();
            }
            items[files.length]=getString(R.string.emulator_settings_miscellaneous_custom_font_file_path_dialog_add_hint);
            create_list_dialog(getString(R.string.emulator_settings_miscellaneous_custom_font_file_path_dialog_title), items, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    dialog.dismiss();
                    if(which==files.length)
                        request_select_font_file();
                        else
                            setup_costom_font_path(files[which].getAbsolutePath());
                }
            });
        }

        void show_affinity_mask_view(){
            final View layout=LayoutInflater.from(getContext()).inflate(R.layout.setting_affinity_mask,null);
            load_affinity_mask(layout,config);
            AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
            builder.setView(layout);
            builder.setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    save_affinity_mask(layout,config);
                }
            });
            builder.setCancelable( false);
            builder.setOnKeyListener(new DialogInterface.OnKeyListener() {
                @Override
                public boolean onKey(DialogInterface dialog, int keyCode, KeyEvent event) {
                    if(keyCode==KeyEvent.KEYCODE_BACK)
                        return true;
                    return false;
                }
            });
            builder.create().show();
        }

        void show_library_control_view(){
            LibraryControlAdapter adapter=new LibraryControlAdapter(getContext());
            adapter.set_modify_libs(config.load_config_entry_ty_arr("Core|Libraries Control"));
            ListView view = new ListView(getContext());
            view.setAdapter(adapter);
            view.setOnItemClickListener(new AdapterView.OnItemClickListener() {
                @Override
                public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                    LibraryControlAdapter  adapter= (LibraryControlAdapter) parent.getAdapter();
                    int new_lib_type=adapter.get_lib_type(position)^1;
                    adapter.set_lib_trpe(position,new_lib_type);
                    config.save_config_entry_ty_arr("Core|Libraries Control",adapter.get_modify_libs());
                }
            });

            AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
            builder.setTitle(R.string.emulator_settings_core_libraries_control)
                    .setView(view)
                    .setNegativeButton(android.R.string.cancel, null);
            builder.create().show();
        }

        void setup_pref_title_color(Preference preference,String cur_val){
            if(preference instanceof CheckBoxPreference){
                CheckBoxPreference pref=(CheckBoxPreference) preference;
                boolean modify=!original_config.load_config_entry(pref.getKey()).equals((cur_val));
                pref.set_is_modify_color(modify);
            }
            else if(preference instanceof SeekBarPreference){
                SeekBarPreference pref=(SeekBarPreference) preference;
                boolean modify=!original_config.load_config_entry(pref.getKey()).equals((cur_val));
                pref.set_is_modify_color(modify);
            }
            else if(preference instanceof ListPreference){
                ListPreference pref=(ListPreference) preference;
                boolean modify=!original_config.load_config_entry(pref.getKey()).equals((cur_val));
                pref.set_is_modify_color(modify);
            }
        }

        void setup_config_dependency(Preference preference,String cur_val){
            String key=preference.getKey();
            switch (key){
                case Video$Strict_Rendering_Mode:
                    if(cur_val.equals("true")){
                        ((SeekBarPreference)findPreference(Video$Resolution_Scale)).setValue(100);
                        ((SeekBarPreference)findPreference(Video$Resolution_Scale)).setEnabled(false);
                        config.save_config_entry(Video$Resolution_Scale,"100");
                    }
                    else{
                        ((SeekBarPreference)findPreference(Video$Resolution_Scale)).setEnabled(true);
                    }
                    break;
                /*case Video$Use_BGRA_Format:
                    if(cur_val.equals("true")){
                        ((CheckBoxPreference)findPreference(Video$Read_Color_Buffers)).setEnabled(true);
                        ((CheckBoxPreference)findPreference(Video$Write_Color_Buffers)).setEnabled(true);
                    }
                    else{
                        ((CheckBoxPreference)findPreference(Video$Read_Color_Buffers)).setChecked(false);
                        ((CheckBoxPreference)findPreference(Video$Read_Color_Buffers)).setEnabled(false);
                        ((CheckBoxPreference)findPreference(Video$Write_Color_Buffers)).setChecked(false);
                        ((CheckBoxPreference)findPreference(Video$Write_Color_Buffers)).setEnabled(false);
                        config.save_config_entry(Video$Read_Color_Buffers,"false");
                        config.save_config_entry(Video$Write_Color_Buffers,"false");
                    }
                    break;*/
                case Video$Vulkan$Asynchronous_Texture_Streaming:
                    if(cur_val.equals("true")){
                        findPreference(Video$Vulkan$Asynchronous_Queue_Scheduler).setEnabled( true);
                    }
                    else{
                        findPreference(Video$Vulkan$Asynchronous_Queue_Scheduler).setEnabled(false);
                    }
                    break;
                case Video$Vulkan$Use_Custom_Driver:
                    if(cur_val.equals("true")){
                        findPreference(Video$Vulkan$Custom_Driver_Library_Path).setEnabled( true);
                    }
                    else{
                        findPreference(Video$Vulkan$Custom_Driver_Library_Path).setEnabled(false);
                    }
                    break;
                case Miscellaneous$Font_File_Selection:
                    if(cur_val.equals("Custom")){
                        findPreference(Miscellaneous$Custom_Font_File_Path).setEnabled(true);
                    }
                    else{
                        findPreference(Miscellaneous$Custom_Font_File_Path).setEnabled(false);
                    }
                    break;
                case Core$Thread_Scheduler_Mode:
                    if(cur_val.equals("Affinity")){
                        findPreference(Core$Thread_Affinity_Mask).setEnabled(true);
                    }
                    else{
                        findPreference(Core$Thread_Affinity_Mask).setEnabled(false);
                    }
                    break;
            }
        }

        @Override
        public boolean onPreferenceChange(@NonNull Preference preference, Object newValue) {
            if(preference instanceof CheckBoxPreference){
                CheckBoxPreference pref=(CheckBoxPreference) preference;
                String value=Boolean.toString((boolean)newValue);
                setup_pref_title_color(pref,value);
                setup_config_dependency(pref,value);
                return true;
            }
            else if(preference instanceof SeekBarPreference){
                SeekBarPreference pref=(SeekBarPreference) preference;
                String value=Integer.toString((int)newValue);
                setup_pref_title_color(pref,value);
                setup_config_dependency(pref,value);
                return true;
            }
            else if(preference instanceof ListPreference){
                ListPreference pref=(ListPreference) preference;
                CharSequence value=(CharSequence) newValue;
                CharSequence[] values=pref.getEntryValues();
                CharSequence[] entries=pref.getEntries();
                for (int i=0;i<values.length;i++){
                    if (values[i].equals(value)){
                        pref.setSummary(entries[i]);
                        break;
                    }
                }
                setup_pref_title_color(pref,value.toString());
                setup_config_dependency(pref,value.toString());
                return true;
            }

            return false;
        }
    }

    SettingsFragment fragment;
    @Override
    protected void onCreate(Bundle savedInstanceState) {

        supportRequestWindowFeature(Window.FEATURE_NO_TITLE);

        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_emulator_settings);

        // 设置 Toolbar
        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        if(getSupportActionBar()!=null) {
            getSupportActionBar().setTitle(getString(R.string.settings));
        }

        String config_path=getIntent().getStringExtra(EXTRA_CONFIG_PATH);


        if(config_path!=null) {
            fragment=SettingsFragment.new_instance(config_path,false);
        }
        else{
            fragment=SettingsFragment.new_instance(Application.get_global_config_file().getAbsolutePath(),true);
        }

        getSupportFragmentManager().beginTransaction().replace(R.id.settings_container,fragment).commit();
    }

    static boolean install_custom_driver_from_zip(Context ctx, Uri uri, InstallCallback cb) {
        try {
            ParcelFileDescriptor pfd = ctx.getContentResolver().openFileDescriptor(uri, "r");
            FileInputStream fis = new FileInputStream(pfd.getFileDescriptor());
            ZipInputStream zis = new ZipInputStream(fis);

            Map<String, byte[]> entries = new HashMap<>();
            for (ZipEntry ze = zis.getNextEntry(); ze != null; ze = zis.getNextEntry()) {
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                byte[] buffer = new byte[16384];
                int n;
                while ((n = zis.read(buffer)) != -1)
                    baos.write(buffer, 0, n);
                entries.put(ze.getName(), baos.toByteArray());
                zis.closeEntry();
            }

            zis.close();
            fis.close();
            pfd.close();

            String libs_dir_name = Utils.getFileNameFromUri(uri);
            libs_dir_name = libs_dir_name.substring(0, libs_dir_name.lastIndexOf('.'));
            File libs_dir = new File(Application.get_custom_driver_dir(), libs_dir_name);

            String driver_lib_path = null;
            if (entries.containsKey("meta.json")) {
                JSONObject meta = new JSONObject(new String(entries.get("meta.json")));
                if (!meta.has("libraryName")) {
                    Toast.makeText(ctx, "ERR: invalid meta.json", Toast.LENGTH_SHORT).show();
                    return false;
                }
                String driver_lib_name = meta.getString("libraryName");
                if(!entries.containsKey( driver_lib_name)){
                    Toast.makeText(ctx, "ERR: not found "+driver_lib_name, Toast.LENGTH_SHORT).show();
                    return false;
                }

                driver_lib_path = new File(libs_dir, driver_lib_name).getAbsolutePath();
            }

            if(driver_lib_path==null){
                int lib_count=0;
                String lib_name = null;
                for(String entry_name : entries.keySet()){
                    if (entry_name.endsWith(".so")) {
                        lib_name = entry_name;
                        lib_count++;
                    }
                }
                if(lib_count!=1){
                    Toast.makeText(ctx, "ERR: invalid file", Toast.LENGTH_SHORT).show();
                    return false;
                }
                driver_lib_path = new File(libs_dir, lib_name).getAbsolutePath();
            }

            if (!libs_dir.exists()) libs_dir.mkdirs();
            for (Map.Entry<String, byte[]> entry : entries.entrySet()) {
                if (entry.getKey().endsWith(".so")||entry.getKey().equals("meta.json")) {
                    String lib_path = new File(libs_dir, entry.getKey()).getAbsolutePath();
                    FileOutputStream lib_os = new FileOutputStream(lib_path);
                    lib_os.write(entry.getValue());
                    lib_os.close();
                }
            }
            cb.onInstallComplete(driver_lib_path);
            return true;
        } catch (Exception e) {
            Toast.makeText(ctx, e.toString(), Toast.LENGTH_SHORT).show();
            return false;
        }
    }


    static boolean install_custom_driver_from_lib(Context ctx, Uri uri, InstallCallback cb){
        try {
            ParcelFileDescriptor pfd = ctx.getContentResolver().openFileDescriptor(uri, "r");
            FileInputStream lib_is = new FileInputStream(pfd.getFileDescriptor());
            String lib_path=new File(Application.get_custom_driver_dir() , Utils.getFileNameFromUri(uri)).getAbsolutePath();
            FileOutputStream lib_os = new FileOutputStream(lib_path);

            byte[] buffer = new byte[16384];
            int n;
            while ((n = lib_is.read(buffer)) != -1)
                lib_os.write(buffer, 0, n);
            lib_os.close();
            lib_is.close();
            pfd.close();
            cb.onInstallComplete(lib_path);
            return true;
        } catch (Exception e) {
            Toast.makeText(ctx, e.toString(), Toast.LENGTH_SHORT).show();
            return false;
        }
    }

    static boolean install_custom_font(Context ctx, Uri uri, InstallCallback cb){
        try {
            ParcelFileDescriptor pfd = ctx.getContentResolver().openFileDescriptor(uri, "r");
            FileInputStream font_is = new FileInputStream(pfd.getFileDescriptor());
            String font_path=new File(Application.get_custom_font_dir(), Utils.getFileNameFromUri(uri)).getAbsolutePath();
            FileOutputStream font_os = new FileOutputStream(font_path);

            byte[] buffer = new byte[16384];
            int n;
            while ((n = font_is.read(buffer)) != -1)
                font_os.write(buffer, 0, n);
            font_os.close();
            font_is.close();
            pfd.close();
            cb.onInstallComplete(font_path);
            return true;
        } catch (Exception e) {
            Toast.makeText(ctx, e.toString(), Toast.LENGTH_SHORT).show();
            return false;
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (resultCode != RESULT_OK || data == null) return;

        Uri uri=data.getData();
        String file_name = Utils.getFileNameFromUri(uri);

        switch (requestCode){
            case REQUEST_CODE_SELECT_CUSTOM_DRIVER:
                if(file_name.endsWith(".zip"))
                    install_custom_driver_from_zip(this,uri,(path)->{ fragment.setup_costom_driver_library_path(path);});
                else if(file_name.endsWith(".so"))
                    install_custom_driver_from_lib(this,uri,(path)->{ fragment.setup_costom_driver_library_path(path);});
                break;
                case REQUEST_CODE_SELECT_CUSTOM_FONT:
                    if(file_name.endsWith(".ttf")||file_name.endsWith(".ttc")||file_name.endsWith(".otf"))
                        install_custom_font(this,uri,(path)->{ fragment.setup_costom_font_path(path);});
                    break;
        }
    }

    static class LibraryControlAdapter extends BaseAdapter{

        static final int LIB_TYPE_LLE=0;
        static final int LIB_TYPE_HLE=1;

        static final Map<String, Integer> libs=new HashMap<>();

        static {
libs.put("/dev_flash/sys/internal/libfs_utility_init.sprx", 1);
libs.put("libaacenc.sprx", 0);
libs.put("libaacenc_spurs.sprx", 0);
libs.put("libac3dec.sprx", 0);
libs.put("libac3dec2.sprx", 0);
libs.put("libadec.sprx", 1);
libs.put("libadec2.sprx", 0);
libs.put("libadec_internal.sprx", 0);
libs.put("libad_async.sprx", 0);
libs.put("libad_billboard_util.sprx", 0);
libs.put("libad_core.sprx", 0);
libs.put("libapostsrc_mini.sprx", 0);
libs.put("libasfparser2_astd.sprx", 0);
libs.put("libat3dec.sprx", 0);
libs.put("libat3multidec.sprx", 0);
libs.put("libatrac3multi.sprx", 0);
libs.put("libatrac3plus.sprx", 0);
libs.put("libatxdec.sprx", 1);
libs.put("libatxdec2.sprx", 0);
libs.put("libaudio.sprx", 1);
libs.put("libavcdec.sprx", 0);
libs.put("libavcenc.sprx", 0);
libs.put("libavcenc_small.sprx", 0);
libs.put("libavchatjpgdec.sprx", 0);
libs.put("libbeisobmf.sprx", 0);
libs.put("libbemp2sys.sprx", 0);
libs.put("libcamera.sprx", 1);
libs.put("libcelp8dec.sprx", 0);
libs.put("libcelp8enc.sprx", 0);
libs.put("libcelpdec.sprx", 0);
libs.put("libcelpenc.sprx", 0);
libs.put("libddpdec.sprx", 0);
libs.put("libdivxdec.sprx", 0);
libs.put("libdmux.sprx", 0);
libs.put("libdmuxpamf.sprx", 0);
libs.put("libdtslbrdec.sprx", 0);
libs.put("libfiber.sprx", 0);
libs.put("libfont.sprx", 0);
libs.put("libfontFT.sprx", 0);
libs.put("libfreetype.sprx", 0);
libs.put("libfreetypeTT.sprx", 0);
libs.put("libfs.sprx", 0);
libs.put("libfs_155.sprx", 0);
libs.put("libgcm_sys.sprx", 0);
libs.put("libgem.sprx", 1);
libs.put("libgifdec.sprx", 0);
libs.put("libhttp.sprx", 0);
libs.put("libio.sprx", 1);
libs.put("libjpgdec.sprx", 0);
libs.put("libjpgenc.sprx", 0);
libs.put("libkey2char.sprx", 0);
libs.put("libl10n.sprx", 0);
libs.put("liblv2.sprx", 0);
libs.put("liblv2coredump.sprx", 0);
libs.put("liblv2dbg_for_cex.sprx", 0);
libs.put("libm2bcdec.sprx", 0);
libs.put("libm4aacdec.sprx", 0);
libs.put("libm4aacdec2ch.sprx", 0);
libs.put("libm4hdenc.sprx", 0);
libs.put("libm4venc.sprx", 0);
libs.put("libmedi.sprx", 1);
libs.put("libmic.sprx", 1);
libs.put("libmp3dec.sprx", 0);
libs.put("libmp4.sprx", 0);
libs.put("libmpl1dec.sprx", 0);
libs.put("libmvcdec.sprx", 0);
libs.put("libnet.sprx", 0);
libs.put("libnetctl.sprx", 1);
libs.put("libpamf.sprx", 1);
libs.put("libpngdec.sprx", 0);
libs.put("libpngenc.sprx", 0);
libs.put("libresc.sprx", 0);
libs.put("librtc.sprx", 1);
libs.put("librudp.sprx", 0);
libs.put("libsail.sprx", 0);
libs.put("libsail_avi.sprx", 0);
libs.put("libsail_rec.sprx", 0);
libs.put("libsjvtd.sprx", 0);
libs.put("libsmvd2.sprx", 0);
libs.put("libsmvd4.sprx", 0);
libs.put("libspurs_jq.sprx", 0);
libs.put("libsre.sprx", 0);
libs.put("libssl.sprx", 0);
libs.put("libsvc1d.sprx", 0);
libs.put("libsync2.sprx", 0);
libs.put("libsysmodule.sprx", 0);
libs.put("libsysutil.sprx", 1);
libs.put("libsysutil_ap.sprx", 1);
libs.put("libsysutil_authdialog.sprx", 1);
libs.put("libsysutil_avc2.sprx", 1);
libs.put("libsysutil_avconf_ext.sprx", 1);
libs.put("libsysutil_avc_ext.sprx", 1);
libs.put("libsysutil_bgdl.sprx", 1);
libs.put("libsysutil_cross_controller.sprx", 1);
libs.put("libsysutil_dec_psnvideo.sprx", 1);
libs.put("libsysutil_dtcp_ip.sprx", 1);
libs.put("libsysutil_game.sprx", 1);
libs.put("libsysutil_game_exec.sprx", 1);
libs.put("libsysutil_imejp.sprx", 1);
libs.put("libsysutil_misc.sprx", 1);
libs.put("libsysutil_music.sprx", 1);
libs.put("libsysutil_music_decode.sprx", 1);
libs.put("libsysutil_music_export.sprx", 1);
libs.put("libsysutil_np.sprx", 1);
libs.put("libsysutil_np2.sprx", 1);
libs.put("libsysutil_np_clans.sprx", 1);
libs.put("libsysutil_np_commerce2.sprx", 1);
libs.put("libsysutil_np_eula.sprx", 1);
libs.put("libsysutil_np_installer.sprx", 1);
libs.put("libsysutil_np_sns.sprx", 1);
libs.put("libsysutil_np_trophy.sprx", 1);
libs.put("libsysutil_np_tus.sprx", 1);
libs.put("libsysutil_np_util.sprx", 1);
libs.put("libsysutil_oskdialog_ext.sprx", 1);
libs.put("libsysutil_pesm.sprx", 1);
libs.put("libsysutil_photo_decode.sprx", 1);
libs.put("libsysutil_photo_export.sprx", 1);
libs.put("libsysutil_photo_export2.sprx", 1);
libs.put("libsysutil_photo_import.sprx", 1);
libs.put("libsysutil_photo_network_sharing.sprx", 1);
libs.put("libsysutil_print.sprx", 1);
libs.put("libsysutil_rec.sprx", 1);
libs.put("libsysutil_remoteplay.sprx", 1);
libs.put("libsysutil_rtcalarm.sprx", 1);
libs.put("libsysutil_savedata.sprx", 1);
libs.put("libsysutil_savedata_psp.sprx", 1);
libs.put("libsysutil_screenshot.sprx", 1);
libs.put("libsysutil_search.sprx", 1);
libs.put("libsysutil_storagedata.sprx", 1);
libs.put("libsysutil_subdisplay.sprx", 1);
libs.put("libsysutil_syschat.sprx", 1);
libs.put("libsysutil_sysconf_ext.sprx", 1);
libs.put("libsysutil_userinfo.sprx", 1);
libs.put("libsysutil_video_export.sprx", 1);
libs.put("libsysutil_video_player.sprx", 1);
libs.put("libsysutil_video_upload.sprx", 1);
libs.put("libusbd.sprx", 0);
libs.put("libusbpspcm.sprx", 0);
libs.put("libvdec.sprx", 1);
libs.put("libvoice.sprx", 1);
libs.put("libvpost.sprx", 0);
libs.put("libvpost2.sprx", 0);
libs.put("libwmadec.sprx", 0);
        }

        String[] libs_name=libs.keySet().toArray(new String[0]);

        final Map<String,  Integer> modify=new HashMap<>();

        Context  context;
        LibraryControlAdapter(Context ctx){
            this.context=ctx;
            Arrays.sort(libs_name);
        }

        int get_lib_type(int pos){
            if(modify.containsKey(libs_name[pos]))
                return modify.get(libs_name[pos]);
            return libs.get(libs_name[pos]);
        }

        void set_lib_trpe(int pos,int type){
            int default_type=libs.get(libs_name[pos]);
            if(type==default_type){
                if(modify.containsKey(libs_name[pos]))
                    modify.remove(libs_name[pos]);
            }
            else{
                modify.put(libs_name[pos], type);
            }
            notifyDataSetChanged();
        }

        void set_modify_libs(String[] modify_libs){
            modify.clear();
            if(modify_libs==null||modify_libs.length==0) return;
            for(String s:modify_libs){
                String[] split=s.split(":");
                String lib_name=split[0];
                int type=split[1].equals("lle")?LIB_TYPE_LLE:LIB_TYPE_HLE;
                if(!libs.containsKey(lib_name))
                    continue;
                if(libs.get(lib_name)==type)
                    continue;
                modify.put(lib_name, type);
            }
        }
        String[] get_modify_libs(){
            List<String> l=new ArrayList<>();
            for(Map.Entry<String, Integer> e:modify.entrySet()){
                String lib_name=e.getKey();
                String lib_ty=e.getValue()==LIB_TYPE_LLE?"lle":"hle";
                l.add(lib_name+":"+lib_ty);
            }
            return l.toArray(new String[0]);
        }

        @Override
        public int getCount() {
            return libs_name.length;
        }

        @Override
        public Object getItem(int position) {
            return null;
        }

        @Override
        public long getItemId(int position) {
            return 0;
        }

        LayoutInflater get_inflater(){
            return (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        }

        void setup_type_view(TextView v,int type){
            CharSequence text=type==LIB_TYPE_LLE?"lle":"hle";
            SpannableString span=new SpannableString(text);

            ForegroundColorSpan[] lle_hle_span_color=new ForegroundColorSpan[2];
            if(theme_is_night(context)){
                lle_hle_span_color[0]=new ForegroundColorSpan(Color.YELLOW);
                lle_hle_span_color[1]=new ForegroundColorSpan(Color.rgb(32,255,255));
            }else{

                lle_hle_span_color[0]=new ForegroundColorSpan(Color.rgb(255,127,63));
                lle_hle_span_color[1]=new ForegroundColorSpan(Color.BLUE);
            }

            if(type==LIB_TYPE_LLE){
                span.setSpan(lle_hle_span_color[0],0,text.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
            else{//HLE
                span.setSpan(lle_hle_span_color[1],0,text.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
            v.setText(span);
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            if(convertView==null){
                convertView=get_inflater().inflate(R.layout.two_state_entry,null);
            }
            TextView  name_v=(TextView) convertView.findViewById(R.id.text);
            String name=libs_name[position];
            name_v.setText(name);
            int ty=modify.containsKey(name)?modify.get(name):libs.get(name);
            TextView  type_v=(TextView) convertView.findViewById(R.id.state);
            setup_type_view(type_v,ty);

            return convertView;
        }
    }

    static boolean theme_is_night(Context ctx){
        int current_mode=ctx.getResources().getConfiguration().uiMode & android.content.res.Configuration.UI_MODE_NIGHT_MASK;
        return current_mode==android.content.res.Configuration.UI_MODE_NIGHT_YES;
    }

    static class CoreMaskAdapter extends BaseAdapter implements ListView.OnItemClickListener{

        static class CoreInfo{
            int id;
            int max_mhz;
            String arch;
        }
        CoreInfo[] cores;
        Context ctx;
        int mask;
        CoreMaskAdapter(Context ctx,int mask){
            this.ctx=ctx;
            this.mask=mask;
            final int core_count=Emulator.get.get_cpu_core_count();
            cores=new CoreInfo[core_count];
            for(int i=0;i<core_count;i++){
                cores[i]=new CoreInfo();
                cores[i].id=i;
                cores[i].max_mhz=Emulator.get.get_cpu_max_mhz(i);
                cores[i].arch=Emulator.get.get_cpu_name(i);
            }
        }

        void switch_state(int core_id){
            mask^=(1<<core_id);
        }

        int get_mask(){
            return mask;
        }
        @Override
        public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
            switch_state(position);
            notifyDataSetChanged();
        }

        @Override
        public int getCount() {
            return cores.length;
        }

        @Override
        public Object getItem(int position) {
            return null;
        }

        @Override
        public long getItemId(int position) {
            return 0;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            if(convertView==null){
                convertView=LayoutInflater.from(ctx).inflate(R.layout.two_state_entry,null);
            }
            TextView  name_v=(TextView) convertView.findViewById(R.id.text);
            name_v.setText(cores[position].arch+": "+cores[position].max_mhz+"Mhz");
            TextView  state_v=(TextView) convertView.findViewById(R.id.state);


            ForegroundColorSpan[] off_on_span_color={
                    new ForegroundColorSpan(Color.GRAY),
                    new ForegroundColorSpan(theme_is_night(ctx)?Color.YELLOW:Color.rgb(255,127,63))
            };
            SpannableString span_text=new SpannableString(((mask & (1<<position))!=0)?"on":"off");
            span_text.setSpan(off_on_span_color[(mask>>position)&1],0,span_text.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            state_v.setText(span_text);
            return convertView;
        }
    }

    static int load_int_val(Emulator.Config cfg,String key,int def_val){
        String val=cfg.load_config_entry(key);
        return val==null?def_val:Integer.parseInt(val);
    };
    static void load_affinity_mask(View root, Emulator.Config cfg){
        ListView[] lv=new ListView[]{
                (ListView) root.findViewById(R.id.ppu_use_cores),
                (ListView) root.findViewById(R.id.spu_use_cores),
                (ListView) root.findViewById(R.id.rsx_use_cores)
        };

        int[] mask=new int[]{
                load_int_val(cfg,EmulatorSettings.Core$Thread_Affinity_Mask+"|PPU Threads",1),
                load_int_val(cfg,EmulatorSettings.Core$Thread_Affinity_Mask+"|SPU Threads",1),
                load_int_val(cfg,EmulatorSettings.Core$Thread_Affinity_Mask+"|RSX Threads",1)
        };

        for(int i=0;i<lv.length;i++){
            CoreMaskAdapter adapter=new CoreMaskAdapter(root.getContext(),mask[i]);
            lv[i].setAdapter(adapter);
            lv[i].setOnItemClickListener(adapter);
        }
    }

    static void save_affinity_mask(View root,Emulator.Config cfg){
        ListView[] lv=new ListView[]{
                (ListView) root.findViewById(R.id.ppu_use_cores),
                (ListView) root.findViewById(R.id.spu_use_cores),
                (ListView) root.findViewById(R.id.rsx_use_cores)
        };
        String[] keys=new String[]{
                EmulatorSettings.Core$Thread_Affinity_Mask+"|PPU Threads",
                EmulatorSettings.Core$Thread_Affinity_Mask+"|SPU Threads",
                EmulatorSettings.Core$Thread_Affinity_Mask+"|RSX Threads"

        };
        for(int i=0;i<lv.length;i++){
            CoreMaskAdapter adapter=(CoreMaskAdapter) lv[i].getAdapter();
            cfg.save_config_entry(keys[i],Integer.toString(adapter.get_mask()));
        }
    }
}
