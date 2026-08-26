// SPDX-License-Identifier: WTFPL

package aenu.aps3e;

import android.annotation.TargetApi;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ListActivity;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.database.Cursor;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;
import android.icu.text.SimpleDateFormat;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Message;
import android.os.ParcelFileDescriptor;
import android.preference.PreferenceManager;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.format.DateFormat;
import android.text.style.ForegroundColorSpan;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileFilter;
import java.lang.annotation.Target;
import java.text.Collator;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import android.os.Handler;
import java.io.FileOutputStream;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.IOException;
import android.app.Activity;
import android.widget.AdapterView;
import android.widget.Adapter;
import android.Manifest;
import java.util.*;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import java.io.*;
import android.view.*;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.documentfile.provider.DocumentFile;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

public class MainActivity extends AppCompatActivity {

	public static final int REQUEST_INSTALL_FIRMWARE=6001;
	private static final int REQUEST_INSTALL_GAME=6002;
	//private static final int REQUEST_INSTALL_GAME_PKG=6003;
	public static final int REQUEST_SELECT_ISO_DIR=6004;

	static final String PREF_KEY_ISO_DIR="iso_dir";

	static final String PREF_KEY_EXTRA_ISO_DIRS="extra_iso_dirs";

	public static File get_hdd0_game_dir(){
		return new File(Application.get_app_data_dir(),"config/dev_hdd0/game");
	}

	public static File get_game_data_dir(){
		return new File(Application.get_app_data_dir(),"config/dev_hdd0/home/00000001/savedata");
	}

	public static File get_game_trophy_dir(){
		return new File(Application.get_app_data_dir(),"config/dev_hdd0/home/00000001/trophy");
	}

	public static File[] get_ppu_cache_dirs(String serial){
		File cache_dir=new File(Application.get_app_data_dir(),"cache/cache/"+serial);
		if(!cache_dir.exists()) return null;
		File[] ppu_cache_dir=cache_dir.listFiles();
		return ppu_cache_dir;
	}

	public static File[] get_all_ppu_cache_dirs(){
		File cache_dir=new File(Application.get_app_data_dir(),"cache/cache");
		if(!cache_dir.exists()) return null;
		File[] ppu_cache_dir=cache_dir.listFiles(new FileFilter(){
			@Override
			public boolean accept(File f)
			{
				return f.isDirectory();
			}
		});
		return ppu_cache_dir;
	}
	public static File[] get_shader_cache_dirs(String serial){
		File[] ppu_cache_dirs=get_ppu_cache_dirs(serial);
		if( ppu_cache_dirs==null||ppu_cache_dirs.length==0) return null;
		File[] shader_cache_dirs=new File[ppu_cache_dirs.length];
		for(int i=0;i<ppu_cache_dirs.length;i++){
			shader_cache_dirs[i]=new File(ppu_cache_dirs[i],"shaders_cache");
		}
		return shader_cache_dirs;
	}

	public static File get_disc_game_dir(){
		return new File(Application.get_app_data_dir(),"config/games");
	}

	public static File get_game_list_file(){
		return new File(Application.get_app_data_dir(),"config/game_list.json");
	}

	public static File firmware_installed_file(){
		return new File(Application.get_app_data_dir(),"config/dev_flash/.installed");
	}

	static String get_config_resolution(String serial){
		Emulator.Config cfg=null;
		File cfg_file=Application.get_custom_cfg_file(serial);
		if(!cfg_file.exists())
			cfg_file=Application.get_global_config_file();
		if(!cfg_file.exists())
			return null;
		try{
			cfg=Emulator.Config.open_config_file(cfg_file.getAbsolutePath());
			String resolution=cfg.load_config_entry(EmulatorSettings.Video$Resolution);
			String aspect_ratio=cfg.load_config_entry(EmulatorSettings.Video$Aspect_ratio);
			cfg.close_config_file();

			switch (resolution){
				case "1920x1080":
				case "1920x1080i":
					return String.format("1920x1080 [%s]",aspect_ratio);
				case "1280x720":
				case "1440x1080":
				case "1280x1080":
					return String.format("1280x720 [%s]",aspect_ratio);
				case "960x1080":
				case "720x480":
				case "720x480i":
					return String.format("720x480 [%s]",aspect_ratio);
				case "720x576":
				case "720x576i":
					return String.format("720x576 [%s]",aspect_ratio);
				default:
					return null;
			}
		}catch(Exception e){
			return null;
		}
	}
	
	private final AdapterView.OnItemClickListener item_click_l=new AdapterView.OnItemClickListener(){
		@Override
		public void onItemClick(AdapterView<?> l, View v, int position,long id)
		{
			if(!firmware_installed_file().exists()){
				Toast.makeText(MainActivity.this,getString(R.string.firmware_not_install),Toast.LENGTH_LONG).show();
				return;
			}
			
			Emulator.MetaInfo meta_info=((GameMetaInfoAdapter)l.getAdapter()).getMetaInfo(position);

			if(!meta_info.decrypt) {
				show_hint_dialog(getString(R.string.undecrypted_game));
				return;
			}

			if(PPUCacheBuildService.isBuilding(meta_info)){
				Toast.makeText(MainActivity.this,R.string.creating_ppu_cache,Toast.LENGTH_LONG).show();
				return;
			}

			/*if(meta_info.resolution==0) {
				refresh_game_list();
				return;
			}

			if(!meta_info.get_support_resolution().contains(get_config_resolution(meta_info.serial))){
				String msg_fmt=getString(R.string.resolution_not_support);
				String support_resolution=meta_info.print_resolution();
				String setting_1=getString(R.string.emulator_settings_video)+"->"+getString(R.string.emulator_settings_video_resolution);
				String setting_2=getString(R.string.emulator_settings_video)+"->"+getString(R.string.emulator_settings_video_aspect_ratio);
				show_hint_dialog(String.format(msg_fmt,support_resolution,setting_1,setting_2));
				return;
			}*/

			Intent intent = new Intent("aenu.intent.action.APS3E");
			intent.setPackage(getPackageName());
			
			intent.putExtra(EmulatorActivity.EXTRA_META_INFO,meta_info);

			// 检测是否存在即时存档,存在则让用户选择开始游戏或载入存档
			List<File> savestates = find_instant_savestates(meta_info.serial);
			if (!savestates.isEmpty()) {
				show_savestate_menu(intent, savestates);
			} else {
				startActivity(intent);
			}
		}
	};

	private List<File> find_instant_savestates(String serial){
		List<File> savestates = new ArrayList<>();
		if (serial == null || serial.isEmpty())
			return savestates;

		File dir = Application.get_save_state_dir(serial);
		File[] files = dir.listFiles();
		if (files == null)
			return savestates;

		String prefix = serial + "_";
		for (File f : files) {
			String name = f.getName();
			if (f.isFile() && name.startsWith(prefix) && name.endsWith(".SAVESTAT.zst") && f.length() > 1024)
				savestates.add(f);
		}
		return savestates;
	}

	// 存在即时存档时弹出菜单,让用户选择开始游戏或载入存档
	private void show_savestate_menu(final Intent baseIntent,  List<File> savestates){
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setTitle(R.string.start_game_or_load_savestate);
		List<String> items = new ArrayList<>();
		items.add(getString(R.string.start_game));
		for(File f:savestates){
			String name=f.getName();
			name=name.substring(0,name.length()-".SAVESTAT.zst".length());
			String time=new SimpleDateFormat("yyyy-MM-dd HH:mm:ss").format(new Date(f.lastModified()));
			items.add(String.format("%s (%s)",name,time));
		}
		builder.setItems(items.toArray(new String[0]), new DialogInterface.OnClickListener(){
			@Override
			public void onClick(DialogInterface dialog, int which){
				Intent intent = new Intent(baseIntent);
				int savestate_index=which-1;
				if (which !=0 ) // 载入即时存档
					intent.putExtra(EmulatorActivity.EXTRA_SAVESTATE_PATH, savestates.get(savestate_index).getAbsolutePath());
				startActivity(intent);
			}
		});
		builder.setNegativeButton(android.R.string.cancel, null);
		builder.show();
	}

	GameMetaInfoAdapter adapter;

	ProgressTask progress_task;
	Toolbar toolbar;
    @Override
    public void onCreate(Bundle savedInstanceState){
        supportRequestWindowFeature(Window.FEATURE_NO_TITLE);

        super.onCreate(savedInstanceState);
		Utils.setup_system_bars_appearance(this);
        setContentView(R.layout.activity_main);

		// 设置 Toolbar
		toolbar = findViewById(R.id.toolbar);

		setSupportActionBar(toolbar);
		if(getSupportActionBar()!=null) {
			getSupportActionBar().setTitle(getString(R.string.select_game));//"选择游戏");
		}
		android.util.Log.i("aps3e_java","main");

		((ListView)findViewById(R.id.game_list)).setOnItemClickListener(item_click_l);
		((ListView)findViewById(R.id.game_list)).setEmptyView(findViewById(R.id.game_list_is_empty));
		//((ListView)findViewById(R.id.game_list)).setOnLongClickListener(item_long_click_l);
		registerForContextMenu(findViewById(R.id.game_list));

		/*boolean is_request_perms_ok;
		if(Build.VERSION.SDK_INT>=30) {
			is_request_perms_ok=check_and_request_perms_API30();
		}
		else{
			is_request_perms_ok=check_and_request_perms();
		}*/


		//if(Build.VERSION.SDK_INT>=30||is_request_perms_ok)
		initialize();

		//if(is_request_perms_ok)
		{
			show_game_list();
		}
    }

	@Override
    public boolean onCreateOptionsMenu(Menu menu){
        super.onCreateOptionsMenu(menu);
        getMenuInflater().inflate(R.menu.main,menu);
		/*if(!getResources().getConfiguration().getLocales().get(0).equals(Locale.SIMPLIFIED_CHINESE))
			menu.removeItem(R.id.menu_update_log);*/
		//menu.removeItem(R.id.menu_set_iso_dir);
		if(getPackageName().equals("aenu.aps3e.premium")){
			menu.removeItem(R.id.menu_buy_premium);
		}
		else{
			MenuItem buy_premium=menu.findItem(R.id.menu_buy_premium);
			SpannableString s=new SpannableString(buy_premium.getTitle());
			s.setSpan(new ForegroundColorSpan(Color.BLUE),0,s.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
			buy_premium.setTitle(s);
		}
        return true;
    }
	
	static void request_file_select(Activity activity,int request_code){

		Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT);
		intent.addCategory(Intent.CATEGORY_OPENABLE);
		intent.setType("*/*");
		activity.startActivityForResult(intent, request_code);
	}

	static void request_dir_select(Activity activity,int request_code){
		Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
		intent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
		activity.startActivityForResult(intent, request_code);
	}
	static void request_iso_dir_select(Activity activity){
		request_dir_select(activity, REQUEST_SELECT_ISO_DIR);
	}

	static void save_pref_iso_dir(Context ctx,Uri uri){
		try{
			SharedPreferences.Editor editor=PreferenceManager.getDefaultSharedPreferences(ctx).edit();
			if(uri==null)
				editor.remove(PREF_KEY_ISO_DIR);
			else {
				ctx.getContentResolver().takePersistableUriPermission(uri,Intent.FLAG_GRANT_READ_URI_PERMISSION);
				editor.putString(PREF_KEY_ISO_DIR, uri.toString());
			}
			editor.apply();
		}
		catch(Exception e){
			e.printStackTrace();
		}
	}

	static Uri load_pref_iso_dir(Context ctx){
		try{
			String uri_str=PreferenceManager.getDefaultSharedPreferences(ctx).getString(PREF_KEY_ISO_DIR,null);
			if(uri_str==null)
				return null;
			Uri uri= Uri.parse(uri_str);
			ctx.getContentResolver().takePersistableUriPermission(uri,Intent.FLAG_GRANT_READ_URI_PERMISSION);
			return uri;
		}
		catch(Exception e){
			e.printStackTrace();
			return null;
		}
	}

	static Set<Uri> load_pref_extra_iso_dirs(Context ctx){

		Set<Uri> uris=new HashSet<>();
		try{
			Set<String> uri_str=PreferenceManager.getDefaultSharedPreferences(ctx).getStringSet(PREF_KEY_EXTRA_ISO_DIRS,null);
			if(uri_str==null||uri_str.isEmpty())
				return uris;

			for(String _uri_str:uri_str){
				Uri uri= Uri.parse(_uri_str);
				ctx.getContentResolver().takePersistableUriPermission(uri,Intent.FLAG_GRANT_READ_URI_PERMISSION);
				uris.add(uri);
			}

			return uris;
		}
		catch(Exception e){
			e.printStackTrace();
			return uris;
		}
	}

	public static void save_pref_extra_iso_dirs(Context ctx,Set<Uri> uris){
		try{
			Set<String> uri_str=new HashSet<String>();
			for(Uri uri:uris){
				ctx.getContentResolver().takePersistableUriPermission(uri,Intent.FLAG_GRANT_READ_URI_PERMISSION);
				uri_str.add(uri.toString());
			}
			SharedPreferences.Editor editor=PreferenceManager.getDefaultSharedPreferences(ctx).edit();
			if(uri_str.isEmpty())
				editor.remove(PREF_KEY_EXTRA_ISO_DIRS);
			else
				editor.putStringSet(PREF_KEY_EXTRA_ISO_DIRS,uri_str);

			editor.apply();
		}
		catch(Exception e){
			e.printStackTrace();
		}
	}

    @Override
    public boolean onOptionsItemSelected(MenuItem item){

		int item_id=item.getItemId();
		if(item_id==R.id.menu_install_firmware){
			request_file_select(this,REQUEST_INSTALL_FIRMWARE);
			return true;
		}
		else if(item_id==R.id.menu_install_game){
			request_file_select(this,REQUEST_INSTALL_GAME);
			return true;
		}
		else if(item_id==R.id.menu_refresh_list){
			refresh_game_list();
			return true;
		}
		else if(item_id==R.id.menu_about){
			startActivity(new Intent(this,AboutActivity.class));
			return true;
		}
		/*else if(item_id==R.id.menu_update_log){
			startActivity(new Intent(this,UpdateLogActivity.class));
			return true;
		}*/
		else if(item_id==R.id.menu_key_mappers){
			startActivity(new Intent(this,KeyMapActivity.class));
			return true;
		}
		else if(item_id==R.id.menu_user_data){
			startActivity(new Intent(this,UserDataActivity.class));
			return true;
		}
		/*else if(item_id==R.id.menu_gratitude){
			startActivity(new Intent(this,GratitudeActivity.class));
			return true;
		}*/
		else if(item_id==R.id.menu_virtual_pad_edit){
			startActivity(new Intent(this,VirtualPadEdit.class));
			return true;
		}
		else if(item_id==R.id.menu_settings){
			startActivity(new Intent(this,EmulatorSettings.class));
			return true;
		}
		else if(item_id==R.id.menu_open_file_mgr){
			/*Intent it=new Intent(this,TextActivity.class);
			it.putExtra("text",Emulator.get.generate_java_string_arr());
			startActivity(it);*/
			Utils.open_file_manager(this);
			return true;
		}
		else if(item_id==R.id.menu_set_iso_dir){
			request_iso_dir_select(this);
			return true;
		}
		else if(item_id==R.id.menu_quick_start_page){
			Intent it=new Intent(this,QuickStartActivity.class);
			it.setAction(QuickStartActivity.ACTION_REENTRY);
			startActivity(it);
			return true;
		}
		else if(item_id==R.id.menu_buy_premium){

			try{
				startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("market://details?id=aenu.aps3e.premium")));
			}catch (Exception e){
				startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("https://play.google.com/store/apps/details?id=aenu.aps3e.premium")));
			}

			return true;
		}
		return super.onOptionsItemSelected(item);

        /*super.onOptionsItemSelected(item);
        switch(item.getItemId()){
			case R.id.menu_install_firmware:
                request_file_select(FileChooser.REQUEST_INSTALL_FIRMWARE);
                break;    
			case R.id.menu_install_game:
                request_file_select(FileChooser.REQUEST_INSTALL_GAME);
                break;    
			case R.id.menu_refresh_list:
                refresh_game_list();
                break;    
            case R.id.menu_about:
                startActivity(new Intent(this,HelloJni.class));
                break;       
			case R.id.menu_update_log:
				startActivity(new Intent(this,UpdateLogActivity.class));
				break;
			case R.id.menu_key_mappers:
				startActivity(new Intent(this,KeyMapActivity.class));
				break;
			case R.id.menu_gratitude:
				startActivity(new Intent(this,GratitudeActivity.class));
				break;
			case R.id.menu_virtual_pad_edit:
				startActivity(new Intent(this,VirtualPadEdit.class));
				break;
        }
        return true;*/
    }
/*
	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data)
	{
		super.onActivityResult(requestCode, resultCode, data);
		if(resultCode!=RESULT_OK) return;
		String path=data.getStringExtra("path");
		switch(requestCode){
			case REQUEST_INSTALL_GAME:{
				if(path.endsWith(".iso")){
				Emulator.MetaInfo mi=Emulator.get.meta_info_from_iso(path);
				if(mi.icon!=null&&mi.serial!=null){
					InstallGame.install_iso(this,path,mi.serial);
				}
				return;}
					else if(path.endsWith(".pkg")){
						InstallGame.install_pkg(this,path,null);
				}
					else if(path.endsWith(".rap")){
						Emulator.get.install_rap(path);
				}
				return ;
			}
			case REQUEST_INSTALL_FIRMWARE:{
				InstallFrimware.start_install_firmware(this,path);
				return;
			}
			default:
			return;
		}
	}*/

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		super.onActivityResult(requestCode, resultCode, data);
		if (resultCode != RESULT_OK || data == null) return;

		Uri uri = data.getData();

		if(requestCode == REQUEST_SELECT_ISO_DIR){
			save_pref_iso_dir(this,uri);
			refresh_game_list();
			return;
		}

		if (uri != null) {
			String fileName = Utils.getFileNameFromUri(uri);
			int pfd;
            try {
                ParcelFileDescriptor pfd_= getContentResolver().openFileDescriptor(uri, "r");
				pfd=pfd_.detachFd();
				pfd_.close();
            } catch (Exception e) {
				Toast.makeText(this, e.getMessage(), Toast.LENGTH_SHORT).show();
				return;
            }
            switch(requestCode){
				case REQUEST_INSTALL_FIRMWARE:
					//InstallFrimware.start_install_firmware(this,pfd);
					(progress_task=new ProgressTask( this)
							.set_progress_message(getString( R.string.installing_firmware))
							.set_failed_task(new ProgressTask.UI_Task() {
								@Override
								public void run() {
									Toast.makeText(MainActivity.this, getString(R.string.msg_failed), Toast.LENGTH_LONG).show();
								}
							}))
							.call( new ProgressTask.Task() {
								@Override
								public void run(ProgressTask task) {
									if(firmware_installed_file().exists())
										firmware_installed_file().delete();

									if(Emulator.get.install_firmware(pfd)){

                                        try {
                                            firmware_installed_file() .createNewFile();
                                        } catch (IOException e) {
                                            throw new RuntimeException(e);
                                        }

                                        task.task_handler.sendEmptyMessage(ProgressTask.TASK_DONE);
										progress_task=null;
										return;
									}
									task.task_handler.sendEmptyMessage(ProgressTask.TASK_FAILED);
									progress_task=null;
								}
							});
					return;
				case REQUEST_INSTALL_GAME:
					if (fileName.endsWith(".pkg")) {
						/*Log.i("aps3e_java", "installing pkg: "+fileName);
						InstallGame.install_pkg(this, pfd);
						refresh_game_list();*/
						(progress_task=new  ProgressTask(this)
								.set_progress_message(getString(R.string.installing_game))
								.set_failed_task(new ProgressTask.UI_Task() {
									@Override
									public void run() {
										Toast.makeText(MainActivity.this, getString(R.string.msg_failed), Toast.LENGTH_LONG).show();
									}
								}))
								.call(new ProgressTask.Task() {
									@Override
									public void run(ProgressTask task) {
										if (Emulator.get.install_pkg(pfd)) {
											task.task_handler.sendEmptyMessage(ProgressTask.TASK_DONE);
											progress_task = null;
											return;
										}
										task.task_handler.sendEmptyMessage(ProgressTask.TASK_FAILED);
										progress_task = null;
									}
								});
					} else if (fileName.endsWith(".rap")) {
						if(!Emulator.get.install_rap(pfd, fileName)){
							Toast.makeText(this, "Failed to install RAP", Toast.LENGTH_SHORT).show();
						}
					}
					else if (fileName.endsWith(".edat")) {
						if(!Emulator.get.install_edat(pfd)){
							Toast.makeText(this, "Failed to install EDAT", Toast.LENGTH_SHORT).show();
						}
					}
					break;
			}
		}
	}

	@Override
    public boolean onKeyDown(int keyCode,KeyEvent event){
        if(keyCode==KeyEvent.KEYCODE_BACK){
            finish();
            return true;
        }
		if (keyCode == KeyEvent.KEYCODE_MENU) {
			openOptionsMenu();
			return true;
		}
		if (keyCode == KeyEvent.KEYCODE_DPAD_UP) {
			ListView listView = findViewById(R.id.game_list);
			if (listView != null && listView.getSelectedItemPosition() <= 0) {
				if (toolbar != null && toolbar.getChildCount() > 0) {
					View lastChild = toolbar.getChildAt(toolbar.getChildCount() - 1);
					if (lastChild instanceof ViewGroup) {
						ViewGroup group = (ViewGroup) lastChild;
						if (group.getChildCount() > 0) {
							View lastMenuItem = group.getChildAt(group.getChildCount() - 1);
							lastMenuItem.setFocusable(true);
							lastMenuItem.setFocusableInTouchMode(true);
							lastMenuItem.requestFocus();
							return true;
						}
					}
				}
			}
		}
		if (keyCode == KeyEvent.KEYCODE_DPAD_DOWN) {
			if (toolbar != null && toolbar.hasFocus()) {
				ListView listView = findViewById(R.id.game_list);
				if (listView != null) {
					listView.requestFocus();
					listView.setSelection(0);
					return true;
				}
			}
		}

        return super.onKeyDown(keyCode,event);
    }
	
	@Override
	public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
		super.onCreateContextMenu(menu, v, menuInfo);
		AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) menuInfo;
		if(adapter.is_disc_game(info.position)){
			getMenuInflater().inflate(R.menu.iso_game_options, menu);
		}
		else{
			getMenuInflater().inflate(R.menu.hdd0_game_options, menu);
		}
	}
	
	@Override
	public boolean onContextItemSelected(MenuItem item) {
		AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
		
		int position = info.position;
		int item_id = item.getItemId();
		if(item_id==R.id.delete_hdd0_install_data){
			show_verify_dialog(R.string.delete_hdd0_install_data, new DialogInterface.OnClickListener() {
				@Override
				public void onClick(DialogInterface dialog, int which) {
					(progress_task=new ProgressTask(MainActivity.this)).call(new ProgressTask.Task() {
						@Override
						public void run(ProgressTask task) {
							adapter.del_hdd0_install_data(position);
							task.task_handler.sendEmptyMessage(ProgressTask.TASK_DONE);
							progress_task=null;
						}
					});
				}
			});
			return true;
		}
		else if(item_id==R.id.delete_game_data){
			show_verify_dialog(R.string.delete_game_data, new DialogInterface.OnClickListener() {
				@Override
				public void onClick(DialogInterface dialog, int which) {
					(progress_task=new ProgressTask(MainActivity.this)).call(new ProgressTask.Task() {
						@Override
						public void run(ProgressTask task) {
							adapter.del_game_data(position);
							task.task_handler.sendEmptyMessage(ProgressTask.TASK_DONE);
							progress_task=null;
						}
					});
				}
			});
		}
		else if(item_id==R.id.delete_game_and_data){
			show_verify_dialog(R.string.delete_game_and_data, new DialogInterface.OnClickListener() {
				@Override
				public void onClick(DialogInterface dialog, int which) {
					(progress_task=new ProgressTask(MainActivity.this)
							.set_done_task(new ProgressTask.UI_Task(){
								@Override
								public void run() {
									refresh_game_list();
								}
							}))
							.call(new ProgressTask.Task() {
						@Override
						public void run(ProgressTask task) {
							adapter.del_hdd0_game_and_data(position);
							task.task_handler.sendEmptyMessage(ProgressTask.TASK_DONE);
							progress_task=null;
						}
					});
				}
			});
		}
		else if(item_id==R.id.delete_shaders_cache){
			(progress_task=new ProgressTask(MainActivity.this)).call(new ProgressTask.Task() {
				@Override
				public void run(ProgressTask task) {
					adapter.del_shaders_cache(position);
					task.task_handler.sendEmptyMessage(ProgressTask.TASK_DONE);
					progress_task=null;
				}
			});
		}
		else if(item_id==R.id.edit_custom_config){
			Intent intent=new Intent(this,EmulatorSettings.class);
			File cfg_file=Application.get_custom_cfg_file(adapter.getMetaInfo(position).serial);
			if(!cfg_file.exists()){
				copy_file(Application.get_global_config_file(),cfg_file);
			}
			intent.putExtra(EmulatorSettings.EXTRA_CONFIG_PATH, cfg_file.getAbsolutePath());
			startActivity(intent);
		}
		else if(item_id==R.id.create_shortcut){
			ShortcutManager shortcutManager=getSystemService(ShortcutManager.class);
			Emulator.MetaInfo meta_info=adapter.getMetaInfo(position);

			Bitmap icon=BitmapFactory.decodeByteArray(meta_info.icon,0,meta_info.icon.length);
			meta_info.icon=null;

			Intent intent=new Intent(this,EmulatorActivity.class);
			{
				intent.setAction(Intent.ACTION_VIEW);
				if(meta_info.iso_uri!=null) {
					intent.putExtra(EmulatorActivity.EXTRA_ISO_URI, meta_info.iso_uri);
				}
				else if(!adapter.is_disc_game(position)){
					intent.putExtra(EmulatorActivity.EXTRA_GAME_DIR
							,new File(MainActivity.get_hdd0_game_dir(),meta_info.serial).getAbsolutePath());
				}
				else{
					return false;
				}
			}
			shortcutManager.requestPinShortcut(new ShortcutInfo.Builder(this, meta_info.serial)
					.setShortLabel(meta_info.name)
					.setIcon(Icon.createWithBitmap( icon))
					.setIntent(intent)
					.build(), null);
		}
		else if(item_id==R.id.show_game_info){
			show_hint_dialog(adapter.getMetaInfo(position).toString());
		}
		else if(item_id==R.id.show_trophy_info){
			Emulator.MetaInfo meta_info=adapter.getMetaInfo(position);
			Emulator.GameTrophyInfo trophy_info;
			if((trophy_info=Emulator.GameTrophyManager.get_or_init().find(meta_info.name))!=null)
				show_trophy_dialog(trophy_info);
			else
				Toast.makeText(this, R.string.no_found_trophy_info, Toast.LENGTH_SHORT).show();
		}
		/*else if(item_id==R.id.create_ppu_cache){
			Emulator.MetaInfo meta_info=adapter.getMetaInfo(position);
			Intent intent=new Intent(this,PPUCacheBuildService.class);
			intent.putExtra(PPUCacheBuildService.EXTRA_META_INFO, meta_info);

			if(!PPUCacheBuildService.isBuilding(meta_info)){
				startService(intent);
			}
		}*/
		else if(item_id==R.id.delete_ppu_cache){
			show_verify_dialog(R.string.delete_ppu_cache, new DialogInterface.OnClickListener() {
				@Override
				public void onClick(DialogInterface dialog, int which) {
					(progress_task=new ProgressTask(MainActivity.this)
							)
							.call(new ProgressTask.Task() {
								@Override
								public void run(ProgressTask task) {
									adapter.del_ppu_cache(position);
									task.task_handler.sendEmptyMessage(ProgressTask.TASK_DONE);
									progress_task=null;
								}
							});
				}
			});
		}
		else if(item_id==R.id.delete_spu_cache){
			show_verify_dialog(R.string.delete_spu_cache, new DialogInterface.OnClickListener() {
				@Override
				public void onClick(DialogInterface dialog, int which) {
					(progress_task=new ProgressTask(MainActivity.this)
					)
							.call(new ProgressTask.Task() {
								@Override
								public void run(ProgressTask task) {
									adapter.del_spu_cache(position);
									task.task_handler.sendEmptyMessage(ProgressTask.TASK_DONE);
									progress_task=null;
								}
							});
				}
			});
		}
		else if(item_id==R.id.patch) {
			Emulator.MetaInfo meta_info=adapter.getMetaInfo(position);
			show_patch_dialog(meta_info.serial);
		}
		return super.onContextItemSelected(item);
	}

	static void copy_file(File src,File dst){
		try {
			FileInputStream in=new FileInputStream(src);
			FileOutputStream out=new FileOutputStream(dst);
			byte buf[]=new byte[16384];
			int n;
			while((n=in.read(buf))!=-1)
				out.write(buf,0,n);
			in.close();
			out.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	@Override
	protected void onDestroy() {
		if(progress_task!=null){
			progress_task.force_close();
			progress_task=null;
		}
		super.onDestroy();
	}



	void show_verify_dialog(int res_id, DialogInterface.OnClickListener listener){
		new AlertDialog.Builder(this)
				.setMessage(getString(res_id)+"?")
				.setPositiveButton(android.R.string.ok, listener)
				.setNegativeButton(android.R.string.cancel, null)
				.create().show();
	}

	void show_hint_dialog(String hint){
		new AlertDialog.Builder(this)
				.setMessage(hint)
				.setNegativeButton(android.R.string.cancel, null)
				.create().show();
	}
	void show_trophy_dialog(Emulator.GameTrophyInfo game_trophy_info) {
		ListView layout = new ListView(this);
		layout.setAdapter(new TrophyDialogAdapter(this, game_trophy_info));
		androidx.appcompat.app.AlertDialog.Builder builder = new androidx.appcompat.app.AlertDialog.Builder(this);
		builder.setView(layout);
		builder.create().show();
	}
	void show_patch_dialog(String serial){
	    List<Emulator.PatchManager.PatchInfo> allPatches = Emulator.PatchManager.getAllLocalPatches();

	    Map<String, Boolean> enabledPatches = Emulator.PatchManager.getEnabledLocalPatches();
	    List<Emulator.PatchManager.PatchInfo> gamePatches = Emulator.PatchManager.getPatchesForGame(serial, allPatches, enabledPatches);

	    if (gamePatches.isEmpty()) {
	        new AlertDialog.Builder(this)
	            .setTitle(serial)
	            .setMessage(getString(R.string.no_patches_available))
	            .setPositiveButton(android.R.string.ok, null)
	            .create().show();
	        return;
	    }

	    String[] patchNames = new String[gamePatches.size()];
	    boolean[] checkedStates = new boolean[gamePatches.size()];
	    
	    for (int i = 0; i < gamePatches.size(); i++) {
	        Emulator.PatchManager.PatchInfo patch = gamePatches.get(i);
	        patchNames[i] = patch.name;
	        checkedStates[i] = patch.isEnabled;
	    }

	    AlertDialog.Builder builder = new AlertDialog.Builder(this);
	    builder.setTitle(serial)
	           .setMultiChoiceItems(patchNames, checkedStates, new DialogInterface.OnMultiChoiceClickListener() {
	               @Override
	               public void onClick(DialogInterface dialog, int which, boolean isChecked) {
	                   checkedStates[which] = isChecked;
	               }
	           })
	           .setPositiveButton(getString(R.string.apply), new DialogInterface.OnClickListener() {
	               @Override
	               public void onClick(DialogInterface dialog, int which) {
                       try {
						   if(!Application.get_patch_config_yml_file().exists()) Application.get_patch_config_yml_file().createNewFile();
                           Emulator.Config config = Emulator.Config.open_config_file(Application.get_patch_config_yml_file().getAbsolutePath());
						   for (int i = 0; i < gamePatches.size(); i++) {
							   Emulator.PatchManager.PatchInfo patch = gamePatches.get(i);
							   boolean newState = checkedStates[i];

							   if (patch.isEnabled != newState) {
								   Emulator.PatchManager.updatePatchStatus(patch, newState, config);
							   }
						   }
						   config.close_config_file();
                       } catch (Exception e) {
                           throw new RuntimeException(e);
                       }


	                   Toast.makeText(MainActivity.this, getString(R.string.patch_settings_saved), Toast.LENGTH_SHORT).show();
	               }
	           })
	           .setNegativeButton(android.R.string.cancel, null)
	           .create().show();
	}
	
	static void mk_dirs(){
		String[] sub_dir_paths={
				"aps3e",
				"aps3e/config",
				"aps3e/config/dev_flash",
				"aps3e/config/dev_flash2",
				"aps3e/config/dev_flash3",
				"aps3e/config/dev_bdvd",
				"aps3e/config/dev_hdd0",
				"aps3e/config/dev_hdd1",
				"aps3e/config/dev_hdd0/game",
				"aps3e/config/dev_hdd0/home",
				"aps3e/config/dev_hdd0/home/00000001",
				"aps3e/config/dev_hdd0/home/00000001/exdata",
				"aps3e/config/games",
				"aps3e/config/Icons",
				"aps3e/config/Icons/ui",
				"aps3e/config/patches",
				"aps3e/config/custom_cfg",
				"aps3e/logs",
				"aps3e/font",
		};

		for(String sp:sub_dir_paths){
			File dir=new File(Application.get_app_data_dir().getParent(),sp);
			if(!dir.exists()){
				dir.mkdir();
			}
		}
	}
	private void initialize(){
		//private final File APP_DIR=new File(Environment.getExternalStorageDirectory(),"aps3e");

		mk_dirs();

		File icons_ui_output_dir=new File(Application.get_app_data_dir().getParent(),"aps3e/config/Icons/ui");
		Utils.extractAssetsDir(this,"Icons/ui",icons_ui_output_dir);

		/*File xxxx_txt=new File(Application.get_app_data_dir(),"xxxx.txt");
		if(!xxxx_txt.exists()) {
			String xxxx_txt_str="";
			xxxx_txt_str+=Emulator.get.generate_config_xml();
			xxxx_txt_str+="\n\n";
			xxxx_txt_str+=Emulator.get.generate_strings_xml();
			xxxx_txt_str+="\n\n";
			xxxx_txt_str+=Emulator.get.generate_java_string_arr();

            FileOutputStream fos= null;
            try {
                fos = new FileOutputStream(xxxx_txt);
				fos.write(xxxx_txt_str.getBytes());
            } catch (Exception e) {
				Log.w("aps3e_java","xxxx msg:"+e.getMessage());
            } finally {
				try {
					if (fos != null) fos.close();
				} catch (IOException e) {
				}
			}
		}*/

		File config_yml=Application.get_global_config_file();;
		if(!config_yml.exists()) {
			if(Application.get_default_config_file().exists())
				Utils.copy_file(Application.get_default_config_file(),config_yml);
		}

		File patch_yml=Application.get_patch_yml_file();
		if(!Application.get_patch_yml_file().exists()){
			Utils.copy_file(Application.get_patch_yml_file(),patch_yml);
		}
		
		/*File nomedia=new File(Environment.getExternalStorageDirectory(),"aps3e/.nomedia");
		try{
			if(!nomedia.exists())
				nomedia.createNewFile();
		}
		catch (IOException e)
		{}*/

        /*if(!APP_DIR.exists())
		 APP_DIR.mkdirs();*/
    }

    private void refresh_game_list(){
		if(!get_hdd0_game_dir().exists()){
			get_hdd0_game_dir().mkdirs();
		}
		if(!get_disc_game_dir().exists()){
			get_disc_game_dir().mkdirs();
		}
		(progress_task=new ProgressTask( this)
				.set_progress_message(getString( R.string.game_list_loading))
				.set_done_task(new ProgressTask.UI_Task() {
					@Override
					public void run() {
						show_game_list();
						progress_task=null;
					}
				}))
				.call( new ProgressTask.Task() {
					@Override
					public void run(ProgressTask task) {

						File json_file=get_game_list_file();
						if(json_file.exists())
							json_file.delete();
						GameMetaInfoAdapter.save_game_list_to_json_file(json_file,GameMetaInfoAdapter.refresh_game_list(MainActivity.this));
							task.task_handler.sendEmptyMessage(ProgressTask.TASK_DONE);
					}
				});
    }

	private void show_game_list(){
		if(!get_hdd0_game_dir().exists()){
			get_hdd0_game_dir().mkdirs();
		}
		if(!get_disc_game_dir().exists()){
			get_disc_game_dir().mkdirs();
		}

        try {
            ArrayList<Emulator.MetaInfo> metas=GameMetaInfoAdapter.load_game_list_from_json_file(get_game_list_file());
			adapter=new GameMetaInfoAdapter(this, metas);
			((ListView)findViewById(R.id.game_list)).setAdapter(adapter);
        } catch (Exception e) {
			Log.w("show_game_list",e.getMessage());
			refresh_game_list();
        }

	}

	static class GameMetaInfoAdapter extends BaseAdapter {
		
		private static final FileFilter dir_filter_=new FileFilter(){
            
            @Override
            public boolean accept(File f){
				if(f.isFile()) return false;
				for(String entry:new String[]{"PARAM.SFO","PS3_GAME/PARAM.SFO"}){
					File param=new File(f,entry);
					if(param.exists()){
						return true;
					}
				}
				return false;

            }         
        };
		
		static byte[] read_game_icon_from_dir(File game_dir) {
			File icon=new File(game_dir,"ICON0.PNG");
			if (!icon.exists()) {
				icon = new File(game_dir, "PS3_GAME/ICON0.PNG");
			}
			if (!icon.exists()) {
				return null;
			}
			try {
				FileInputStream fis = new FileInputStream(icon);
				byte[] buffer = new byte[(int) icon.length()];
				fis.read(buffer);
				return buffer;
			} catch (IOException e) {
				e.printStackTrace();
				return null;
			}
		}
		
		/*static void deleteDirectory(File directory) {
			if (directory.isDirectory()) {
				File[] files = directory.listFiles();
				if (files!= null) {
					for (File file : files) {
						if (file.isDirectory()) {
							deleteDirectory(file);
						} else {
							file.delete();
						}
					}
				}
			}
			directory.delete();
		}*/

		static void recursive_delete_sub_files(File dir) {
			File[] files = dir.listFiles();
			if(files == null)
				return;
			for(File file : files){
				if(file.isDirectory()) {
					recursive_delete_sub_files(file);
				}
				file.delete();
			}
		}

		ArrayList<Emulator.MetaInfo> metas;
		private final MainActivity context_;
		private GameMetaInfoAdapter(MainActivity context,ArrayList<Emulator.MetaInfo> metas){
			context_=context;
			this.metas=metas;
		}
		static ArrayList<Emulator.MetaInfo> load_game_list_from_json_file(File json) throws JSONException, IOException {
			FileInputStream fis=new FileInputStream(json);
			ByteArrayOutputStream baos=new ByteArrayOutputStream();
			byte[] buffer=new byte[16384];
			int n;
			while ((n=fis.read(buffer))!=-1){
				baos.write(buffer,0,n);
			}
			fis.close();
			String json_str=baos.toString();
			return load_game_list_from_json(json_str);
		}

			static ArrayList<Emulator.MetaInfo> load_game_list_from_json(String json_str) throws JSONException {
			ArrayList<Emulator.MetaInfo> metas=new ArrayList<Emulator.MetaInfo>();
			JSONArray game_list=new JSONArray(json_str);
			for(int i=0;i<game_list.length();i++){
				JSONObject game_info=game_list.getJSONObject(i);
				Emulator.MetaInfo meta=Emulator.MetaInfo.from_json(game_info);
				metas.add(meta);
			}
			return metas;
		}

		static void save_game_list_to_json_file(File json,ArrayList<Emulator.MetaInfo> metas){
			try {
				FileOutputStream fos=new FileOutputStream(json);
				fos.write(save_game_list_to_json(metas).getBytes());
				fos.close();
			} catch (IOException | JSONException e) {
				e.printStackTrace();
			}
		}

		static String save_game_list_to_json(ArrayList<Emulator.MetaInfo> metas) throws JSONException {
			JSONArray game_list=new JSONArray();
			for(Emulator.MetaInfo meta:metas){
				game_list.put(Emulator.MetaInfo.to_json(meta));
			}
			return game_list.toString();
		}

		static ArrayList<Emulator.MetaInfo> refresh_game_list(MainActivity context){
			return gen_metas(context,get_game_dir_list(),get_game_iso_list(context));
		}

		private static ArrayList<File> get_game_dir_list(){
			ArrayList<File> game_dir_list=new ArrayList<File>();
			File[] game_dirs;
			game_dirs=get_hdd0_game_dir().listFiles(dir_filter_);
			if(game_dirs!=null)
				for(File f:game_dirs)
					game_dir_list.add(f);

			game_dirs=get_disc_game_dir().listFiles(dir_filter_);
			if(game_dirs!=null)
				for(File f:game_dirs)
					game_dir_list.add(f);

			return game_dir_list;
        }

		private static ArrayList<DocumentFile> get_game_iso_list(MainActivity context_){
			ArrayList<DocumentFile> iso_list=new ArrayList<DocumentFile>();
			Uri main_uri=context_.load_pref_iso_dir(context_);
			Set<Uri> uris=context_.load_pref_extra_iso_dirs(context_);
			if(main_uri!=null)
				uris.add(main_uri);
			for(Uri uri:uris){
				DocumentFile iso_dir=DocumentFile.fromTreeUri(context_, uri);
				if(iso_dir==null||!iso_dir.exists())
					continue;
				DocumentFile [] iso_files=iso_dir.listFiles();
				if(iso_files==null)
					continue;
				for(DocumentFile f:iso_files){
					if(f.isFile()&&f.getName().endsWith(".iso"))
						iso_list.add(f);
				}
			}

			return iso_list;
		}
		
		private static ArrayList<Emulator.MetaInfo> gen_metas(MainActivity context_,ArrayList<File> dirs,ArrayList<DocumentFile> isos){
			ArrayList<Emulator.MetaInfo> metas=new ArrayList<Emulator.MetaInfo>();
			for(File dir:dirs){
				Emulator.MetaInfo  meta=Emulator.get.meta_info_from_dir(dir.getAbsolutePath());
				if(meta.category.equals("GD")) continue;
				meta.icon=read_game_icon_from_dir(dir);
				metas.add(meta);
			}

			for(DocumentFile iso:isos){
                try {
                    ParcelFileDescriptor pfd_=context_.getContentResolver().openFileDescriptor(iso.getUri(),"r");
					int pfd=pfd_.detachFd();
					pfd_.close();
					int dec_key_fd = Utils.try_open_key_fd(context_, iso.getUri());
					Emulator.MetaInfo meta=Emulator.get.meta_info_from_iso(pfd,dec_key_fd,iso.getUri().toString());
					if(meta!=null) {
						meta.dec_key_fd = dec_key_fd;
						metas.add(meta);
					}
                } catch (Exception e) {

                }
			}

			return metas;
		}
		
		public Emulator.MetaInfo getMetaInfo(int pos){
            return metas.get(pos);
        }

		public boolean is_disc_game(int pos){
			Emulator.MetaInfo info=metas.get(pos);
			if(info.iso_uri!=null)
				return true;
			if(info.eboot_path.startsWith(get_disc_game_dir().getAbsolutePath()))
				return true;
			return false;
		}

		public Emulator.MetaInfo fetch_dlc_update_info(String serial){
			File dir=new File(get_hdd0_game_dir(),serial);
			if(!dir.exists()) return null;
			Emulator.MetaInfo info=Emulator.get.meta_info_from_dir(dir.getAbsolutePath());
			return info;
		}

		void _del_trophy_data(String serial){
			File[] trophy_dirs=get_game_trophy_dir().listFiles();
			if(trophy_dirs!=null){
				for(File f:trophy_dirs){
					if(f.getName().startsWith(serial)){
						if(f.isDirectory())
							recursive_delete_sub_files(f);
						f.delete();
					}
				}
			}
		}
		void _del_game_data(String serial){
			File[] data_dirs=get_game_data_dir().listFiles();
			if(data_dirs!=null){
				for(File f:data_dirs){
					if(f.getName().startsWith(serial)){
						if(f.isDirectory())
							recursive_delete_sub_files(f);
						f.delete();
					}
				}
			}
		}

		public void _del_hdd0_data(String serial){
			File[] game_dirs=get_hdd0_game_dir().listFiles();
			if(game_dirs!=null){
				for(File f:game_dirs){
					if(f.getName().startsWith(serial)){
						if(f.isDirectory())
							recursive_delete_sub_files(f);
						f.delete();
					}
				}
			}
		}
		public void del_hdd0_game_and_data(int pos){
			Emulator.MetaInfo info=metas.get(pos);
			_del_trophy_data(info.serial);
			_del_game_data(info.serial);

			File game_dir=new File(get_hdd0_game_dir(),info.serial);
			if(game_dir.exists()){
				recursive_delete_sub_files(game_dir);
				game_dir.delete();
			}
		}

		public void del_hdd0_install_data(int pos){
			Emulator.MetaInfo info=metas.get(pos);
			_del_hdd0_data(info.serial);
		}

		public void del_game_data(int pos){
			Emulator.MetaInfo info=metas.get(pos);
			_del_trophy_data(info.serial);
			_del_game_data(info.serial);
		}

		public  void del_shaders_cache(int pos){
			Emulator.MetaInfo info=metas.get(pos);
			File[] shader_cache_dirs=get_shader_cache_dirs(info.serial);
			if(shader_cache_dirs!=null)
				for(File dir:shader_cache_dirs){
					recursive_delete_sub_files( dir);
					dir.delete();
				}
		}

		public  void del_ppu_cache(int pos){
			Emulator.MetaInfo info=metas.get(pos);
			File[] ppu_cache_dirs=get_ppu_cache_dirs(info.serial);
			if(ppu_cache_dirs!=null)
				for(File dir:ppu_cache_dirs){
					File[] ppu_cache_files=dir.listFiles();
					if(ppu_cache_files!=null)
						for(File f:ppu_cache_files){
							if(f.isFile()&&f.getName().endsWith(".gz"))
								f.delete();
						}
				}
		}

		public  void del_spu_cache(int pos){
			Emulator.MetaInfo info=metas.get(pos);
			File[] ppu_cache_dirs=get_ppu_cache_dirs(info.serial);
			if(ppu_cache_dirs!=null)
				for(File dir:ppu_cache_dirs){
					File[] ppu_cache_files=dir.listFiles();
					if(ppu_cache_files!=null)
						for(File f:ppu_cache_files){
							if(f.isFile()&&f.getName().endsWith(".dat"))
								f.delete();
						}
				}
		}

        @Override
        public int getCount(){
            return metas.size();
        }

        @Override
        public Object getItem(int p1){
            return null;
        }

        @Override
        public long getItemId(int p1){
            return p1;
        }

        private LayoutInflater getLayoutInflater(){
            return (LayoutInflater)context_.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        }

        @Override
        public View getView(int pos,View curView,ViewGroup p3){

            if(curView==null){
                curView=getLayoutInflater().inflate(R.layout.game_item,null);
            }
			
			Emulator.MetaInfo mi=metas.get(pos);

            ImageView icon=(ImageView)curView.findViewById(R.id.game_icon);
			if(mi.icon==null)
				icon.setImageResource(R.drawable.unknown);
			else {
				Bitmap icon_bmp=BitmapFactory.decodeByteArray(mi.icon,0,mi.icon.length);
				if(icon_bmp!=null){
					if(!mi.decrypt)
						icon_bmp=Utils.to_gray_bmp(icon_bmp);
					icon.setImageBitmap(icon_bmp);
				}
				else
					icon.setImageResource(R.drawable.unknown);
			}
			
			TextView name=(TextView)curView.findViewById(R.id.game_name);
			name.setText(mi.name);

			TextView version=(TextView)curView.findViewById(R.id.game_version);
			version.setText(mi.version);
			Emulator.MetaInfo update_info=null;
			if(mi.category.equals("DG")&&(update_info=fetch_dlc_update_info(mi.serial))!=null){
				version.setTextColor(Color.MAGENTA);
				version.setText(update_info.version);
			}
			TextView config_type=(TextView)curView.findViewById(R.id.config_type);
			if(Application.get_custom_cfg_file(mi.serial).exists()){
				config_type.setText("Custom");
				config_type.setTextColor(Color.MAGENTA);
			}else{
				config_type.setText("Default");
			}

            return curView;
        } 
    }//!FileAdapter

	private static class TrophyDialogAdapter extends BaseAdapter {
		private final Context context;
		private final LayoutInflater inflater;
		private Emulator.GameTrophyInfo trophy_info;


		public TrophyDialogAdapter(Context context, Emulator.GameTrophyInfo trophy_info) {
			this.context = context;
			inflater = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
			this.trophy_info = trophy_info;
		}

		@Override
		public int getCount() {
			return trophy_info.trophies.length;
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
				convertView=inflater.inflate(R.layout.game_item,null);
			}

			TextView name=(TextView)convertView.findViewById(R.id.game_name);
			name.setText(trophy_info.trophies[position].name);
			TextView version=(TextView)convertView.findViewById(R.id.game_version);
			version.setText(trophy_info.trophies[position].unlocked?"Unlocked":"Locked");

			ImageView icon=(ImageView)convertView.findViewById(R.id.game_icon);
			Bitmap icon_bmp=BitmapFactory.decodeFile(trophy_info.trophies[position].icon_path);
			if(icon_bmp!=null){
				if(!trophy_info.trophies[position].unlocked) icon_bmp=Utils.to_gray_bmp(icon_bmp);
				icon.setImageBitmap(icon_bmp);
			}

			return convertView;
		}
	}

}
