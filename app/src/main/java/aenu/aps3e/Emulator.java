// SPDX-License-Identifier: WTFPL

package aenu.aps3e;
import java.io.*;
import java.util.ArrayList;
import java.util.Base64;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import android.content.Context;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.util.Log;
import android.view.*;

import androidx.annotation.NonNull;

import org.json.JSONException;
import org.json.JSONObject;

public class Emulator extends aenu.emulator.Emulator
{
	public static class MetaInfo implements Serializable{
		String eboot_path;
		String iso_uri;
		int iso_fd;
		byte[] icon;
		String name;
		String serial;
		String category;
		String version;
		boolean decrypt;
		int dec_key_fd = -1;
		int resolution=0;
		int sound_format=0;


		static JSONObject to_json(MetaInfo  info) throws JSONException {
			JSONObject jo=new JSONObject();
			if(info.eboot_path!=null)
				jo.put("eboot_path",info.eboot_path);
			if(info.iso_uri!=null)
				jo.put("iso_uri",info.iso_uri);
			if(info.icon!=null)
				jo.put("icon",Base64.getEncoder().encodeToString(info.icon));
			jo.put("name",info.name);
			jo.put("serial",info.serial);
			jo.put("category",info.category);
			jo.put("version",info.version);
			jo.put("decrypt",info.decrypt);
			jo.put("resolution",info.resolution);
			jo.put("sound_format",info.sound_format);
			return jo;
		}

		static MetaInfo from_json(JSONObject jo) throws JSONException{
			MetaInfo meta=new MetaInfo();
			if(jo.has("eboot_path")) meta.eboot_path=jo.getString("eboot_path");
			if(jo.has("iso_uri")) meta.iso_uri=jo.getString("iso_uri");
			if(jo.has("icon")) meta.icon=Base64.getDecoder().decode(jo.getString("icon"));
			meta.name=jo.getString("name");
			meta.serial=jo.getString("serial");
			meta.category=jo.getString("category");
			meta.version=jo.getString("version");
			meta.decrypt=jo.getBoolean("decrypt");
			if(jo.has("resolution")) meta.resolution=jo.getInt("resolution");
			if(jo.has("sound_format")) meta.sound_format=jo.getInt("sound_format");
			return meta;
		}

		String _space(int n){
			StringBuilder sb=new StringBuilder();
			while(n-->0){
				sb.append(' ');
			}
			return sb.toString();
		}

		List<String> get_support_resolution(){
			String[] resolution_table={
					"720x480 [4:3]",
					"720x576 [4:3]",
					"1280x720 [4:3]",
					"1920x1080 [4:3]",
					"720x480 [16:9]",
					"720x576 [16:9]"
			};
			List<String> result=new ArrayList<>();
			for(int i=0;i<=5;i++){
				if((resolution&(1<<i))!=0)
					result.add(resolution_table[i]);
			}
			return result;
		}
		String print_resolution()
		{

			StringBuilder sb=new StringBuilder();
			List<String> resolutions=get_support_resolution();
			for(int i=0;i<resolutions.size();i++){
				sb.append(_space(2)).append(resolutions.get(i));
				if(i!=resolutions.size()-1) sb.append("\n");
			}
			String r=sb.toString();
			if(r.isEmpty())
				return "  N/A";
			return r;
		}

		String _parse_sound_format() {
			String[] sound_format_names=new String[]{
					"lpcm_2",
					null,
					"lpcm_5_1",
					null,
					"lpcm_7_1",
					null,
					null,
					null,
					"ac3",
					"dts"};
			StringBuilder sb=new StringBuilder();
			for(int i=0;i<=9;i++) {
				if(sound_format_names[i]==null) continue;
				if((sound_format & (1<<i))!=0)
					sb.append(_space(2)).append(sound_format_names[i]).append("\n");
			}
			String r=sb.toString();
			if(r.isEmpty())
				return "  N/A";
			return r.substring(0,r.length()-1);
		}

		@NonNull
		@Override
		public String toString() {
			StringBuilder sb=new StringBuilder();
			sb.append("Name:\n").append(_space(2)).append(name).append("\n\n")
					.append("Version:\n").append(_space(2)).append(version).append("\n\n")
					.append("Serial:\n").append(_space(2)).append(serial).append("\n\n")
					.append("Category:\n").append(_space(2)).append(category).append("\n\n")
					.append("Resolution:\n").append(print_resolution()).append("\n\n")
					.append("Sound Format:\n").append(_parse_sound_format()).append("\n\n");
			return sb.toString();
		}
	}

	static class CheatInfo{
		static final int TYPE_U8 =1;
		static final int TYPE_U16=2;
		static final int TYPE_U32=3;
		static final int TYPE_U64=4;

		static final long NOT_FOUND=-1;

		long addr;
		long value;

		int type;
	}

	static class GameTrophyInfo{
		static class TrophyInfo{
			String icon_path;
			String name;
			String description;
			byte type;
			long timestamp;
			boolean unlocked;
		}
		String game_name;
		TrophyInfo[] trophies;
	}


	static class GameTrophyManager{
		static GameTrophyManager instance;
		static GameTrophyManager get_or_init(){
			if(instance==null)
				instance=new GameTrophyManager();
			return instance;
		}

		List<GameTrophyInfo> trophy_list=new ArrayList<GameTrophyInfo>();

		GameTrophyManager(){

			final String user_id="00000001";
			final String child=String.format("config/dev_hdd0/home/%s/trophy",user_id);
			final File trophy_dirs=new File(Application.get_app_data_dir(),child);

			final List<File> trophy_dir_list=new ArrayList<File>();
			if(trophy_dirs.exists()){
				File[] dirs=trophy_dirs.listFiles();
				if(dirs!=null)
					for(File f:dirs)
						if(f.isDirectory())
							trophy_dir_list.add(f);
			}

			for(File dir:trophy_dir_list){
				final File tropusr_file=new File(dir,"TROPUSR.DAT");
				final File tropconf_file=new File(dir,"TROPCONF.SFM");
				if(tropusr_file.exists()&&tropconf_file.exists()){
					String vfs_path=String.format("/dev_hdd0/home/%s/trophy/%s",user_id,dir.getName());
					GameTrophyInfo game_trophy_info = Emulator.get.trophy_info_from_dir(dir.getAbsolutePath(),vfs_path);
					trophy_list.add(game_trophy_info);
				}
			}
		}

		GameTrophyInfo find(String game_name){
			for(GameTrophyInfo info:trophy_list){
				if(info.game_name.equals(game_name))
					return info;
			}
			return null;
		}
	}


	static class PatchManager {

		static final String k_all = "All";
		static final String k_anchors = "Anchors";
		static final String k_author = "Author";
		static final String k_games = "Games";
		static final String k_group = "Group";
		static final String k_notes = "Notes";
		static final String k_patch = "Patch";
		static final String k_patch_version = "Patch Version";
		static final String k_version = "Version";
		static final String k_enabled = "Enabled";
		static final String k_config_values = "Configurable Values";
		static final String k_value = "Value";
		static final String k_type = "Type";
		static final String k_min = "Min";
		static final String k_max = "Max";
		static final String k_allowed_values = "Allowed Values";

		public static class PatchInfo {
			public String id;
			public String name;

			public String game_name;
			public String game_serial;
			public String game_version;
			public boolean isEnabled;

			public PatchInfo(String id,String name, String game_name, String game_serial,
							 String game_version) {
				this.id = id;
				this.name = name;
				this.game_name = game_name;
				this.game_serial = game_serial;
				this.game_version = game_version;
				this.isEnabled = false;
			}
		}

		public static List<PatchManager.PatchInfo> parseAllPatches(String patchYmlPath) {
			List<PatchManager.PatchInfo> patches = new ArrayList<>();
			String patchYmlStr=Utils.read_file_as_str(new File(patchYmlPath));
			final String root_key = null;
			try {
				aenu.emulator.Emulator.Config config = aenu.emulator.Emulator.Config.open_config_from_string(patchYmlStr);

				String[] main_keys = config.list_children(root_key);
				if (main_keys == null) {
					config.close_config(); return patches;
				}

				for (String main_key : main_keys) {
					if (main_key.startsWith(k_anchors)) continue;
					if (!main_key.startsWith("PPU-")&&!main_key.startsWith("SPU-")) continue;
					final String[] patchNames = config.list_children(main_key);

					if (patchNames == null) continue;

					for (String patchName : patchNames) {

						String k_Games=String.join("|", new String[]{main_key, patchName, k_games});
						String[] gameTitles = config.list_children(k_Games);

						if (gameTitles == null) {
							continue;
						}

						for (String gameTitle : gameTitles) {
							String k_Games_gameTitle=String.join("|", new String[]{main_key, patchName, k_games, gameTitle});
							String[] serials = config.list_children(k_Games_gameTitle);
							if (serials == null) {
								continue;
							}

							for (String serial : serials) {
								String versionPath = String.join("|", new String[]{main_key, patchName, k_games, gameTitle, serial});
								String[] versions = config.load_config_entry_ty_arr(versionPath);

								if (versions != null && versions.length > 0) {
									for (String ver : versions) {
										patches.add(new PatchManager.PatchInfo(main_key, patchName,gameTitle, serial, ver));
									}
								}
							}
						}
					}
				}

				config.close_config();
			} catch (Exception e) {
				e.printStackTrace();
			}
			return patches;
		}

		public static Map<String, Boolean> loadEnabledPatches(String patchConfigPath) {
			Map<String, Boolean> enabledPatches = new HashMap<>();
			final String root_key = null;
			if(patchConfigPath==null) return enabledPatches;
			if(!new File(patchConfigPath).exists()) return enabledPatches;
			try {
				String patchYmlStr=Utils.read_file_as_str(new File(patchConfigPath));
				aenu.emulator.Emulator.Config config = aenu.emulator.Emulator.Config.open_config_from_string(patchYmlStr);
				String[] main_keys = config.list_children( root_key);
				if (main_keys != null) {
					for(String main_key : main_keys){
						String[] patch_keys = config.list_children(main_key);
						if (patch_keys != null) {
							for(String _patch_key : patch_keys){
								String patch_key = String.join("|", new String[]{main_key, _patch_key});
								String[] game_keys = config.list_children(patch_key);
								if (game_keys != null) {
									for(String game_key : game_keys){
										String game_patch_key = String.join("|", new String[]{patch_key, game_key});
										String[] serial_keys = config.list_children(game_patch_key);
										if (serial_keys != null) {
											for(String _serial_key : serial_keys){
												String serial_key = String.join("|", new String[]{game_patch_key, _serial_key});
												String[] version_keys = config.list_children(serial_key);
												if (version_keys != null) {
													for(String _version_key : version_keys){
														String version_key = String.join("|", new String[]{serial_key, _version_key});
														String patch_enabled = String.join("|", new String[]{version_key, k_enabled});
														String patch_enabled_str = config.load_config_entry(patch_enabled);
														enabledPatches.put(version_key,Boolean.valueOf(patch_enabled_str) );
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}

				config.close_config();
			} catch (Exception e) {
			}
			return enabledPatches;
		}

		private static void add(List<PatchManager.PatchInfo> gamePatches, PatchManager.PatchInfo patch){
			for(PatchManager.PatchInfo patch2 : gamePatches){
				if(patch2.name.equals(patch.name))
					return;
			}
			gamePatches.add(patch);
		}

		public static List<PatchManager.PatchInfo> getPatchesForGame(String serial, List<PatchManager.PatchInfo> allPatches,
																				Map<String, Boolean> enabledPatches) {
			List<PatchManager.PatchInfo> gamePatches = new ArrayList<>();
			for (PatchManager.PatchInfo patch : allPatches) {
				if (patch.game_serial.equals(serial)) {
					final String patchId = String.join("|", new String[]{patch.id, patch.name, patch.game_name, patch.game_serial, patch.game_version});
					for (Map.Entry<String, Boolean> entry : enabledPatches.entrySet()) {
						if (entry.getKey().equals(patchId)) {
							patch.isEnabled = entry.getValue();
							break;
						}
					}
					add(gamePatches, patch);
				}
			}
			return gamePatches;
		}

		public static void updatePatchStatus(PatchManager.PatchInfo patch, boolean isEnabled, aenu.emulator.Emulator.Config config) {
			patch.isEnabled = isEnabled;
			final String enabledId = String.join("|", new String[]{patch.id,patch.name, patch.game_name, patch.game_serial, patch.game_version,k_enabled});
			config.save_config_entry(enabledId, String.valueOf(isEnabled));
		}

		public static List<PatchManager.PatchInfo> getAllLocalPatches() {
			/*File patchFile = Application.get_patch_yml_file();
			if (patchFile.exists()) {
				return parseAllPatches(patchFile.getAbsolutePath());
			}
			return new ArrayList<>();*/
			ArrayList<PatchManager.PatchInfo> patches = new ArrayList<>();
			if(Application.get_patches_dir().exists()&&Application.get_patches_dir().isDirectory()){
				File[] files = Application.get_patches_dir().listFiles();
				if (files == null)
					return patches;
				for (File file : files) {
					if(file.isFile()&&file.getName().endsWith(".yml")){
						patches.addAll(PatchManager.parseAllPatches(file.getAbsolutePath()));
					}
				}
			}
			return patches;
		}

		public static Map<String, Boolean> getEnabledLocalPatches() {
			File patchConfigFile = Application.get_patch_config_yml_file();
			if (patchConfigFile.exists()) {
				return loadEnabledPatches(patchConfigFile.getAbsolutePath());
			}
			return new HashMap<>();
		}
	}

	public static void setup_preload_env(Application app){
		aenu.lang.System.setenv("APS3E_DATA_DIR",app.get_app_data_dir().getAbsolutePath());
		aenu.lang.System.setenv("APS3E_LOG_DIR",app.get_app_log_dir().getAbsolutePath());
		aenu.lang.System.setenv("APS3E_NATIVE_LIB_DIR",app.get_native_lib_dir());
		aenu.lang.System.setenv("APS3E_ANDROID_API_VERSION", Integer.toString(Build.VERSION.SDK_INT));
		aenu.lang.System.setenv("APS3E_GLOBAL_CONFIG_YAML_PATH", Application.get_global_config_file().getAbsolutePath());

		final String localized_string_keys[] = {
			"INVALID",

					"RSX_OVERLAYS_TROPHY_BRONZE",
					"RSX_OVERLAYS_TROPHY_SILVER",
					"RSX_OVERLAYS_TROPHY_GOLD",
					"RSX_OVERLAYS_TROPHY_PLATINUM",
					"RSX_OVERLAYS_COMPILING_SHADERS",
					"RSX_OVERLAYS_COMPILING_PPU_MODULES",
					"RSX_OVERLAYS_MSG_DIALOG_YES",
					"RSX_OVERLAYS_MSG_DIALOG_NO",
					"RSX_OVERLAYS_MSG_DIALOG_CANCEL",
					"RSX_OVERLAYS_MSG_DIALOG_OK",
					"RSX_OVERLAYS_SAVE_DIALOG_TITLE",
					"RSX_OVERLAYS_SAVE_DIALOG_DELETE",
					"RSX_OVERLAYS_SAVE_DIALOG_LOAD",
					"RSX_OVERLAYS_SAVE_DIALOG_SAVE",
					"RSX_OVERLAYS_OSK_DIALOG_ACCEPT",
					"RSX_OVERLAYS_OSK_DIALOG_CANCEL",
					"RSX_OVERLAYS_OSK_DIALOG_SPACE",
					"RSX_OVERLAYS_OSK_DIALOG_BACKSPACE",
					"RSX_OVERLAYS_OSK_DIALOG_SHIFT",
					"RSX_OVERLAYS_OSK_DIALOG_ENTER_TEXT",
					"RSX_OVERLAYS_OSK_DIALOG_ENTER_PASSWORD",
					"RSX_OVERLAYS_MEDIA_DIALOG_TITLE",
					"RSX_OVERLAYS_MEDIA_DIALOG_TITLE_PHOTO_IMPORT",
					"RSX_OVERLAYS_MEDIA_DIALOG_EMPTY",
					"RSX_OVERLAYS_LIST_SELECT",
					"RSX_OVERLAYS_LIST_CANCEL",
					"RSX_OVERLAYS_LIST_DENY",
					"RSX_OVERLAYS_PRESSURE_INTENSITY_TOGGLED_OFF",
					"RSX_OVERLAYS_PRESSURE_INTENSITY_TOGGLED_ON",
					"RSX_OVERLAYS_ANALOG_LIMITER_TOGGLED_OFF",
					"RSX_OVERLAYS_ANALOG_LIMITER_TOGGLED_ON",
					"RSX_OVERLAYS_MOUSE_AND_KEYBOARD_EMULATED",
					"RSX_OVERLAYS_MOUSE_AND_KEYBOARD_PAD",

					"CELL_GAME_ERROR_BROKEN_GAMEDATA",
					"CELL_GAME_ERROR_BROKEN_HDDGAME",
					"CELL_GAME_ERROR_BROKEN_EXIT_GAMEDATA",
					"CELL_GAME_ERROR_BROKEN_EXIT_HDDGAME",
					"CELL_GAME_ERROR_NOSPACE",
					"CELL_GAME_ERROR_NOSPACE_EXIT",
					"CELL_GAME_ERROR_DIR_NAME",
					"CELL_GAME_DATA_EXIT_BROKEN",
					"CELL_HDD_GAME_EXIT_BROKEN",

					"CELL_HDD_GAME_CHECK_NOSPACE",
					"CELL_HDD_GAME_CHECK_BROKEN",
					"CELL_HDD_GAME_CHECK_NODATA",
					"CELL_HDD_GAME_CHECK_INVALID",

					"CELL_GAMEDATA_CHECK_NOSPACE",
					"CELL_GAMEDATA_CHECK_BROKEN",
					"CELL_GAMEDATA_CHECK_NODATA",
					"CELL_GAMEDATA_CHECK_INVALID",

					"CELL_MSG_DIALOG_ERROR_DEFAULT",
					"CELL_MSG_DIALOG_ERROR_80010001",
					"CELL_MSG_DIALOG_ERROR_80010002",
					"CELL_MSG_DIALOG_ERROR_80010003",
					"CELL_MSG_DIALOG_ERROR_80010004",
					"CELL_MSG_DIALOG_ERROR_80010005",
					"CELL_MSG_DIALOG_ERROR_80010006",
					"CELL_MSG_DIALOG_ERROR_80010007",
					"CELL_MSG_DIALOG_ERROR_80010008",
					"CELL_MSG_DIALOG_ERROR_80010009",
					"CELL_MSG_DIALOG_ERROR_8001000A",
					"CELL_MSG_DIALOG_ERROR_8001000B",
					"CELL_MSG_DIALOG_ERROR_8001000C",
					"CELL_MSG_DIALOG_ERROR_8001000D",
					"CELL_MSG_DIALOG_ERROR_8001000F",
					"CELL_MSG_DIALOG_ERROR_80010010",
					"CELL_MSG_DIALOG_ERROR_80010011",
					"CELL_MSG_DIALOG_ERROR_80010012",
					"CELL_MSG_DIALOG_ERROR_80010013",
					"CELL_MSG_DIALOG_ERROR_80010014",
					"CELL_MSG_DIALOG_ERROR_80010015",
					"CELL_MSG_DIALOG_ERROR_80010016",
					"CELL_MSG_DIALOG_ERROR_80010017",
					"CELL_MSG_DIALOG_ERROR_80010018",
					"CELL_MSG_DIALOG_ERROR_80010019",
					"CELL_MSG_DIALOG_ERROR_8001001A",
					"CELL_MSG_DIALOG_ERROR_8001001B",
					"CELL_MSG_DIALOG_ERROR_8001001C",
					"CELL_MSG_DIALOG_ERROR_8001001D",
					"CELL_MSG_DIALOG_ERROR_8001001E",
					"CELL_MSG_DIALOG_ERROR_8001001F",
					"CELL_MSG_DIALOG_ERROR_80010020",
					"CELL_MSG_DIALOG_ERROR_80010021",
					"CELL_MSG_DIALOG_ERROR_80010022",
					"CELL_MSG_DIALOG_ERROR_80010023",
					"CELL_MSG_DIALOG_ERROR_80010024",
					"CELL_MSG_DIALOG_ERROR_80010025",
					"CELL_MSG_DIALOG_ERROR_80010026",
					"CELL_MSG_DIALOG_ERROR_80010027",
					"CELL_MSG_DIALOG_ERROR_80010028",
					"CELL_MSG_DIALOG_ERROR_80010029",
					"CELL_MSG_DIALOG_ERROR_8001002A",
					"CELL_MSG_DIALOG_ERROR_8001002B",
					"CELL_MSG_DIALOG_ERROR_8001002C",
					"CELL_MSG_DIALOG_ERROR_8001002D",
					"CELL_MSG_DIALOG_ERROR_8001002E",
					"CELL_MSG_DIALOG_ERROR_8001002F",
					"CELL_MSG_DIALOG_ERROR_80010030",
					"CELL_MSG_DIALOG_ERROR_80010031",
					"CELL_MSG_DIALOG_ERROR_80010032",
					"CELL_MSG_DIALOG_ERROR_80010033",
					"CELL_MSG_DIALOG_ERROR_80010034",
					"CELL_MSG_DIALOG_ERROR_80010035",
					"CELL_MSG_DIALOG_ERROR_80010036",
					"CELL_MSG_DIALOG_ERROR_80010037",
					"CELL_MSG_DIALOG_ERROR_80010038",
					"CELL_MSG_DIALOG_ERROR_80010039",
					"CELL_MSG_DIALOG_ERROR_8001003A",
					"CELL_MSG_DIALOG_ERROR_8001003B",
					"CELL_MSG_DIALOG_ERROR_8001003C",
					"CELL_MSG_DIALOG_ERROR_8001003D",
					"CELL_MSG_DIALOG_ERROR_8001003E",

					"CELL_OSK_DIALOG_TITLE",
					"CELL_OSK_DIALOG_BUSY",

					"CELL_SAVEDATA_CB_BROKEN",
					"CELL_SAVEDATA_CB_FAILURE",
					"CELL_SAVEDATA_CB_NO_DATA",
					"CELL_SAVEDATA_CB_NO_SPACE",
					"CELL_SAVEDATA_NO_DATA",
					"CELL_SAVEDATA_NEW_SAVED_DATA_TITLE",
					"CELL_SAVEDATA_NEW_SAVED_DATA_SUB_TITLE",
					"CELL_SAVEDATA_SAVE_CONFIRMATION",
					"CELL_SAVEDATA_DELETE_CONFIRMATION",
					"CELL_SAVEDATA_DELETE_SUCCESS",
					"CELL_SAVEDATA_DELETE",
					"CELL_SAVEDATA_SAVE",
					"CELL_SAVEDATA_LOAD",
					"CELL_SAVEDATA_OVERWRITE",
					"CELL_SAVEDATA_AUTOSAVE",
					"CELL_SAVEDATA_AUTOLOAD",

					"CELL_CROSS_CONTROLLER_MSG",
					"CELL_CROSS_CONTROLLER_FW_MSG",

					"CELL_NP_RECVMESSAGE_DIALOG_TITLE",
					"CELL_NP_RECVMESSAGE_DIALOG_TITLE_INVITE",
					"CELL_NP_RECVMESSAGE_DIALOG_TITLE_ADD_FRIEND",
					"CELL_NP_RECVMESSAGE_DIALOG_FROM",
					"CELL_NP_RECVMESSAGE_DIALOG_SUBJECT",

					"CELL_NP_SENDMESSAGE_DIALOG_TITLE",
					"CELL_NP_SENDMESSAGE_DIALOG_TITLE_INVITE",
					"CELL_NP_SENDMESSAGE_DIALOG_TITLE_ADD_FRIEND",
					"CELL_NP_SENDMESSAGE_DIALOG_CONFIRMATION",
					"CELL_NP_SENDMESSAGE_DIALOG_CONFIRMATION_INVITE",
					"CELL_NP_SENDMESSAGE_DIALOG_CONFIRMATION_ADD_FRIEND",

					"RECORDING_ABORTED",

					"RPCN_NO_ERROR",
					"RPCN_ERROR_INVALID_INPUT",
					"RPCN_ERROR_WOLFSSL",
					"RPCN_ERROR_RESOLVE",
					"RPCN_ERROR_CONNECT",
					"RPCN_ERROR_LOGIN_ERROR",
					"RPCN_ERROR_ALREADY_LOGGED",
					"RPCN_ERROR_INVALID_LOGIN",
					"RPCN_ERROR_INVALID_PASSWORD",
					"RPCN_ERROR_INVALID_TOKEN",
					"RPCN_ERROR_INVALID_PROTOCOL_VERSION",
					"RPCN_ERROR_UNKNOWN",
					"RPCN_SUCCESS_LOGGED_ON",
					"RPCN_FRIEND_REQUEST_RECEIVED",
					"RPCN_FRIEND_ADDED",
					"RPCN_FRIEND_LOST",
					"RPCN_FRIEND_LOGGED_IN",
					"RPCN_FRIEND_LOGGED_OUT",

					"HOME_MENU_TITLE",
					"HOME_MENU_EXIT_GAME",
					"HOME_MENU_RESTART",
					"HOME_MENU_RESUME",
					"HOME_MENU_FRIENDS",
					"HOME_MENU_FRIENDS_REQUESTS",
					"HOME_MENU_FRIENDS_BLOCKED",
					"HOME_MENU_FRIENDS_STATUS_ONLINE",
					"HOME_MENU_FRIENDS_STATUS_OFFLINE",
					"HOME_MENU_FRIENDS_STATUS_BLOCKED",
					"HOME_MENU_FRIENDS_REQUEST_SENT",
					"HOME_MENU_FRIENDS_REQUEST_RECEIVED",
					"HOME_MENU_FRIENDS_BLOCK_USER_MSG",
					"HOME_MENU_FRIENDS_UNBLOCK_USER_MSG",
					"HOME_MENU_FRIENDS_REMOVE_USER_MSG",
					"HOME_MENU_FRIENDS_ACCEPT_REQUEST_MSG",
					"HOME_MENU_FRIENDS_CANCEL_REQUEST_MSG",
					"HOME_MENU_FRIENDS_REJECT_REQUEST_MSG",
					"HOME_MENU_FRIENDS_REJECT_REQUEST",
					"HOME_MENU_FRIENDS_NEXT_LIST",
					"HOME_MENU_SETTINGS",
					"HOME_MENU_SETTINGS_SAVE",
					"HOME_MENU_SETTINGS_SAVE_BUTTON",
					"HOME_MENU_SETTINGS_DISCARD",
					"HOME_MENU_SETTINGS_DISCARD_BUTTON",
					"HOME_MENU_SETTINGS_AUDIO",
					"HOME_MENU_SETTINGS_AUDIO_MASTER_VOLUME",
					"HOME_MENU_SETTINGS_AUDIO_BACKEND",
					"HOME_MENU_SETTINGS_AUDIO_BUFFERING",
					"HOME_MENU_SETTINGS_AUDIO_BUFFER_DURATION",
					"HOME_MENU_SETTINGS_AUDIO_TIME_STRETCHING",
					"HOME_MENU_SETTINGS_AUDIO_TIME_STRETCHING_THRESHOLD",
					"HOME_MENU_SETTINGS_VIDEO",
					"HOME_MENU_SETTINGS_VIDEO_FRAME_LIMIT",
					"HOME_MENU_SETTINGS_VIDEO_ANISOTROPIC_OVERRIDE",
					"HOME_MENU_SETTINGS_VIDEO_OUTPUT_SCALING",
					"HOME_MENU_SETTINGS_VIDEO_RCAS_SHARPENING",
					"HOME_MENU_SETTINGS_VIDEO_STRETCH_TO_DISPLAY",
					"HOME_MENU_SETTINGS_INPUT",
					"HOME_MENU_SETTINGS_INPUT_BACKGROUND_INPUT",
					"HOME_MENU_SETTINGS_INPUT_KEEP_PADS_CONNECTED",
					"HOME_MENU_SETTINGS_INPUT_SHOW_PS_MOVE_CURSOR",
					"HOME_MENU_SETTINGS_INPUT_CAMERA_FLIP",
					"HOME_MENU_SETTINGS_INPUT_PAD_MODE",
					"HOME_MENU_SETTINGS_INPUT_PAD_SLEEP",
					"HOME_MENU_SETTINGS_INPUT_FAKE_MOVE_ROTATION_CONE_H",
					"HOME_MENU_SETTINGS_INPUT_FAKE_MOVE_ROTATION_CONE_V",
					"HOME_MENU_SETTINGS_ADVANCED",
					"HOME_MENU_SETTINGS_ADVANCED_PREFERRED_SPU_THREADS",
					"HOME_MENU_SETTINGS_ADVANCED_MAX_CPU_PREEMPTIONS",
					"HOME_MENU_SETTINGS_ADVANCED_ACCURATE_RSX_RESERVATION_ACCESS",
					"HOME_MENU_SETTINGS_ADVANCED_SLEEP_TIMERS_ACCURACY",
					"HOME_MENU_SETTINGS_ADVANCED_MAX_SPURS_THREADS",
					"HOME_MENU_SETTINGS_ADVANCED_DRIVER_WAKE_UP_DELAY",
					"HOME_MENU_SETTINGS_ADVANCED_VBLANK_FREQUENCY",
					"HOME_MENU_SETTINGS_ADVANCED_VBLANK_NTSC",
					"HOME_MENU_SETTINGS_OVERLAYS",
					"HOME_MENU_SETTINGS_OVERLAYS_SHOW_TROPHY_POPUPS",
					"HOME_MENU_SETTINGS_OVERLAYS_SHOW_RPCN_POPUPS",
					"HOME_MENU_SETTINGS_OVERLAYS_SHOW_SHADER_COMPILATION_HINT",
					"HOME_MENU_SETTINGS_OVERLAYS_SHOW_PPU_COMPILATION_HINT",
					"HOME_MENU_SETTINGS_OVERLAYS_SHOW_AUTO_SAVE_LOAD_HINT",
					"HOME_MENU_SETTINGS_OVERLAYS_SHOW_PRESSURE_INTENSITY_TOGGLE_HINT",
					"HOME_MENU_SETTINGS_OVERLAYS_SHOW_ANALOG_LIMITER_TOGGLE_HINT",
					"HOME_MENU_SETTINGS_OVERLAYS_SHOW_MOUSE_AND_KB_TOGGLE_HINT",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_ENABLE",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_ENABLE_FRAMERATE_GRAPH",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_ENABLE_FRAMETIME_GRAPH",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_DETAIL_LEVEL",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_FRAMERATE_DETAIL_LEVEL",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_FRAMETIME_DETAIL_LEVEL",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_FRAMERATE_DATAPOINT_COUNT",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_FRAMETIME_DATAPOINT_COUNT",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_UPDATE_INTERVAL",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_POSITION",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_CENTER_X",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_CENTER_Y",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_MARGIN_X",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_MARGIN_Y",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_FONT_SIZE",
					"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_OPACITY",
					"HOME_MENU_SETTINGS_DEBUG",
					"HOME_MENU_SETTINGS_DEBUG_OVERLAY",
					"HOME_MENU_SETTINGS_DEBUG_INPUT_OVERLAY",
					"HOME_MENU_SETTINGS_DEBUG_DISABLE_VIDEO_OUTPUT",
					"HOME_MENU_SETTINGS_DEBUG_TEXTURE_LOD_BIAS",
					"HOME_MENU_SCREENSHOT",
					"HOME_MENU_SAVESTATE",
					"HOME_MENU_SAVESTATE_SAVE",
					"HOME_MENU_SAVESTATE_AND_EXIT",
					"HOME_MENU_RELOAD_SAVESTATE",
					"HOME_MENU_RECORDING",
					"HOME_MENU_TROPHIES",
					"HOME_MENU_TROPHY_LIST_TITLE",
					"HOME_MENU_TROPHY_LOCKED_TITLE",
					"HOME_MENU_TROPHY_HIDDEN_TITLE",
					"HOME_MENU_TROPHY_HIDDEN_DESCRIPTION",
					"HOME_MENU_TROPHY_PLATINUM_RELEVANT",
					"HOME_MENU_TROPHY_GRADE_BRONZE",
					"HOME_MENU_TROPHY_GRADE_SILVER",
					"HOME_MENU_TROPHY_GRADE_GOLD",
					"HOME_MENU_TROPHY_GRADE_PLATINUM",

					"PROGRESS_DIALOG_PROGRESS",
					"PROGRESS_DIALOG_PROGRESS_ANALYZING",
					"PROGRESS_DIALOG_REMAINING",
					"PROGRESS_DIALOG_DONE",
					"PROGRESS_DIALOG_FILE",
					"PROGRESS_DIALOG_MODULE",
					"PROGRESS_DIALOG_OF",
					"PROGRESS_DIALOG_PLEASE_WAIT",
					"PROGRESS_DIALOG_STOPPING_PLEASE_WAIT",
					"PROGRESS_DIALOG_SCANNING_PPU_EXECUTABLE",
					"PROGRESS_DIALOG_ANALYZING_PPU_EXECUTABLE",
					"PROGRESS_DIALOG_SCANNING_PPU_MODULES",
					"PROGRESS_DIALOG_LOADING_PPU_MODULES",
					"PROGRESS_DIALOG_COMPILING_PPU_MODULES",
					"PROGRESS_DIALOG_LINKING_PPU_MODULES",
					"PROGRESS_DIALOG_APPLYING_PPU_CODE",
					"PROGRESS_DIALOG_BUILDING_SPU_CACHE",
				"PROGRESS_DIALOG_PRELOADING_SHADER_CACHE",

					"EMULATION_PAUSED_RESUME_WITH_START",
					"EMULATION_RESUMING",
					"EMULATION_FROZEN",
					"SAVESTATE_FAILED_DUE_TO_VDEC",
					"SAVESTATE_FAILED_DUE_TO_SAVEDATA",
					"SAVESTATE_FAILED_DUE_TO_SPU",
					"SAVESTATE_FAILED_DUE_TO_MISSING_SPU_SETTING",

				"RSX_OVERLAYS_SPINNER_NO_TEXT",
				"CELL_NP_MESSAGE_INVITE_RECEIVED",
				"CELL_NP_MESSAGE_OTHER_RECEIVED",
				"RPCN_ERROR_BINDING",
				"HOME_MENU_SETTINGS_RESET_BUTTON",
				"HOME_MENU_SETTINGS_VIDEO_VSYNC",
				"HOME_MENU_SETTINGS_VIDEO_RESOLUTION_SCALE_PERCENT",
				"HOME_MENU_SETTINGS_VIDEO_RESOLUTION_SCALE_THRESHOLD",
				"HOME_MENU_SETTINGS_VIDEO_STEREO_MODE",
				"HOME_MENU_SETTINGS_ADVANCED_RSX_MEMORY_TILING",
				"HOME_MENU_SETTINGS_OVERLAYS_SHOW_FATAL_ERROR_HINTS",
				"HOME_MENU_SETTINGS_OVERLAYS_RECORD_WITH_OVERLAYS",
				"HOME_MENU_SETTINGS_OVERLAYS_PLAY_MUSIC_DURING_BOOT",
				"HOME_MENU_SETTINGS_PERFORMANCE_OVERLAY_USE_WINDOW_SPACE",
				"HOME_MENU_SETTINGS_MOUSE_DEBUG_INPUT_OVERLAY",
				"HOME_MENU_RELOAD_SECOND_SAVESTATE",
				"HOME_MENU_RELOAD_THIRD_SAVESTATE",
				"HOME_MENU_RELOAD_FOURTH_SAVESTATE",
				"HOME_MENU_TOGGLE_FULLSCREEN",
				"HOME_MENU_TROPHY_SHOW_HIDDEN_TROPHIES",
				"HOME_MENU_TROPHY_HIDE_HIDDEN_TROPHIES",
				"AUDIO_MUTED",
				"AUDIO_UNMUTED",
				"AUDIO_CHANGED",
				"PROGRESS_DIALOG_SAVESTATE_PLEASE_WAIT",
		};

		for(String k:localized_string_keys){
			try {
				int resId = app.getResources().getIdentifier(k, "string", app.getPackageName());
				if (resId != 0) {
					aenu.lang.System.setenv(k, app.getString(resId));
				} else {
					throw new RuntimeException("String resource not found: " + k);
				}
			} catch (Exception e) {
				throw new RuntimeException(e.getMessage());
			}
		}
	}

	public static Emulator get=null;

	public static void load_library(){
		if(get!=null)
			throw new RuntimeException("Library already loaded");
		get=new Emulator();
		System.loadLibrary("e");
	}
	public boolean support_custom_driver(){
		try {
			return new File("/dev/kgsl-3d0").exists();
		}catch(Exception e){
			return false;
		}
	}

	public void set_env(String k,String v){
		aenu.lang.System.setenv(k,v);
	}
	public native void setup_game_id(String id);

	 public native String[] get_support_llvm_cpu_list();


	public native String[] get_native_llvm_cpu_list();

	public native String[] get_vulkan_physical_dev_list();

	//public native boolean support_custom_driver();
	public native String generate_config_xml();
	public native String generate_strings_xml();
	public native String generate_java_string_arr();
	//public native MetaInfo meta_info_from_iso(String p) throws RuntimeException;
	
	public native MetaInfo meta_info_from_dir(String p) throws RuntimeException;
	public native MetaInfo meta_info_from_iso(int fd,int dec_key_fd,String iso_uri) throws RuntimeException;
	public native boolean install_firmware(int pup_file_fd);

	public boolean install_rap(int pfd,String rap_name){
		final String user_id="00000001";
		final String child=String.format("config/dev_hdd0/home/%s/exdata",user_id);
		final File rap_dir=new File(Application.get_app_data_dir(),child);
		ParcelFileDescriptor rap_file_in=ParcelFileDescriptor.adoptFd(pfd);
		if(!rap_dir.exists()) rap_dir.mkdirs();
		File rap_file_out=new File(rap_dir,rap_name);
        try {
			FileInputStream rap_file_in_s=new FileInputStream(rap_file_in.getFileDescriptor());
			if(rap_file_in_s.available()!=16){
				rap_file_in_s.close();
				rap_file_in.close();
				return false;
			}
            FileOutputStream rap_file_out_s=new FileOutputStream(rap_file_out);
			int n;
			while((n=rap_file_in_s.read())!=-1)
				rap_file_out_s.write(n);
			rap_file_in_s.close();
			rap_file_out_s.close();
			rap_file_in.close();
        } catch (Exception e) {
            return false;
        }
        return true;
	}

	public native boolean install_edat(int pfd);
	public native boolean install_pkg(int pkg_fd);

	//public native boolean inatall_iso(String iso_path,String game_dir);
	/*public native void setup_game_info(MetaInfo info);
	public native void setup_surface(Surface sf);
	public native void boot() throws BootException;

	public native void key_event(int key_code,boolean pressed,int value);

	public native void quit();

	public native boolean is_running();
	public native boolean is_paused();

	public native void pause();

	public native void resume();

    public native void change_surface(int w,int h);*/

    public void key_event(int key_code,boolean pressed){
        key_event(key_code,pressed,255);
    }

	public native String simple_device_info();

	public native int get_cpu_core_count();
	public native String get_cpu_name(int core_idx);
	public native int get_cpu_max_mhz(int core_idx);

	public native boolean precompile_ppu_cache(String path);
	public native boolean precompile_ppu_cache(int fd);

	private native GameTrophyInfo trophy_info_from_dir(String real_path,String vfs_path);

	public native CheatInfo[] search_memory(CheatInfo info);
	public native void set_cheat(CheatInfo info);
	public native void get_cheat(CheatInfo info);
	public native void camera_frame(byte[] data, int width, int height, int format);
}
