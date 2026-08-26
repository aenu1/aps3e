// SPDX-License-Identifier: WTFPL
package aenu.aps3e;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.ContentValues;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.preference.PreferenceManager;
import android.provider.MediaStore;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ListAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.activity.OnBackPressedCallback;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.sql.Array;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

public class UserDataActivity extends AppCompatActivity {
    private Toolbar toolbar;

    private HashMap<Integer,View> layout_list;

    public static final int PAGE_TYPE_MAIN = 0;

    public static final int PAGE_TYPE_PPU_CACHE_MANAGER = 1;
    public static final int PAGE_TYPE_ISO_DIR_MANAGER = 2;
    public static final int PAGE_TYPE_COMPATIBILITY_TABLE = 3;
    public static final int PAGE_TYPE_DRIVER_MANAGER = 4;
    public static final int PAGE_TYPE_GAME_DATA_MANAGER = 5;
    public static final int PAGE_TYPE_PATCH_MANAGER = 6;

    private static final int REQUEST_EXPORT_PPU_CACHE = 6201;
    private static final int REQUEST_IMPORT_PPU_CACHE = 6202;
    private static final int REQUEST_SELECT_ISO_DIR = 6203;
    private static final int REQUEST_IMPORT_GAME_DATA = 6204;

    private int current_page_type = PAGE_TYPE_MAIN;

    static class PageInfo{
        final int title_id;
        final int type;
        PageInfo(int title_id, int type) {
            this.title_id = title_id;
            this.type = type;
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {

        supportRequestWindowFeature(Window.FEATURE_NO_TITLE);

        super.onCreate(savedInstanceState);
        Utils.setup_system_bars_appearance(this);
        setContentView(R.layout.activity_user_data);

        getOnBackPressedDispatcher().addCallback(this, new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                handle_back_pressed();
            }
        });

        toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        toolbar.setNavigationOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                handle_back_pressed();
            }
        });

        init_layout();
        show_main_menu();
    }

    void init_layout() {
        layout_list=new HashMap<>();
        layout_list.put(PAGE_TYPE_MAIN, findViewById(R.id.list_main));
        layout_list.put(PAGE_TYPE_PPU_CACHE_MANAGER, findViewById(R.id.ppu_cache_view));
        layout_list.put(PAGE_TYPE_ISO_DIR_MANAGER, findViewById(R.id.iso_dir_view));
        layout_list.put(PAGE_TYPE_COMPATIBILITY_TABLE, findViewById(R.id.compatibility_table_view));
        layout_list.put(PAGE_TYPE_DRIVER_MANAGER, findViewById(R.id.list_driver));
        layout_list.put(PAGE_TYPE_GAME_DATA_MANAGER, findViewById(R.id.game_data_view));
        layout_list.put(PAGE_TYPE_PATCH_MANAGER, findViewById(R.id.patch_manager_view));
    }

    private void handle_back_pressed() {
        if (current_page_type != PAGE_TYPE_MAIN) {
            show_main_menu();
        } else {
            finish();
        }
    }

    private void handle_item_click(ListAdapter adapter, int position) {
        if (current_page_type == PAGE_TYPE_MAIN) {
            PageInfo info = (PageInfo) adapter.getItem(position);
            switch (info.type) {
                case PAGE_TYPE_PPU_CACHE_MANAGER:
                    show_ppu_cache_page();
                    break;
                case PAGE_TYPE_ISO_DIR_MANAGER:
                    show_iso_dir_page();
                    break;
                case PAGE_TYPE_COMPATIBILITY_TABLE:
                    show_compatibility_table_page();
                    break;
                case PAGE_TYPE_DRIVER_MANAGER:
                    show_driver_manager_page();
                    break;
                case PAGE_TYPE_GAME_DATA_MANAGER:
                    show_game_data_page();
                    break;
                case PAGE_TYPE_PATCH_MANAGER:
                    show_patch_table_page();
                    break;
            }
        } else if (current_page_type == PAGE_TYPE_PPU_CACHE_MANAGER) {
            handle_ppu_cache_item_click(adapter, position);
        } else if (current_page_type == PAGE_TYPE_ISO_DIR_MANAGER) {
            handle_iso_dir_item_click(adapter, position);
        } else if (current_page_type == PAGE_TYPE_COMPATIBILITY_TABLE) {
            //
        } else if (current_page_type == PAGE_TYPE_DRIVER_MANAGER) {
            handle_driver_item_click(adapter, position);
        }
    }

    private void show_main_menu() {
        ListView listMain=(ListView)select_layout(PAGE_TYPE_MAIN);

        final int titleResId = R.string.user_data_manager;
        if(getSupportActionBar()!=null) {
            getSupportActionBar().setTitle(getString(titleResId));
        }

        ArrayList<PageInfo> titles = new ArrayList<>();
        titles.add(new PageInfo(R.string.ppu_cache_manager, PAGE_TYPE_PPU_CACHE_MANAGER));
        titles.add(new PageInfo(R.string.iso_dir_manager, PAGE_TYPE_ISO_DIR_MANAGER));
        titles.add(new PageInfo(R.string.compatibility_table, PAGE_TYPE_COMPATIBILITY_TABLE));
        titles.add(new PageInfo(R.string.patch_manager, PAGE_TYPE_PATCH_MANAGER));
        titles.add(new PageInfo(R.string.game_data_manager, PAGE_TYPE_GAME_DATA_MANAGER));

        if(Emulator.get.support_custom_driver()) {
            titles.add(new PageInfo(R.string.driver_manager, PAGE_TYPE_DRIVER_MANAGER));
        }

        listMain.setAdapter(new ArrayAdapter<PageInfo>(this, android.R.layout.simple_list_item_1, titles){
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                TextView textView = (TextView)convertView;
                if(textView==null)
                    textView = (TextView)getLayoutInflater().inflate(android.R.layout.simple_list_item_1, parent, false);

                textView.setText(getItem(position).title_id);
                return textView;
            }
        });
        listMain.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                handle_item_click((ListAdapter) parent.getAdapter(), position);
            }
        });

        current_page_type = PAGE_TYPE_MAIN;
    }


    private void show_ppu_cache_page() {
        View layout=select_layout(PAGE_TYPE_PPU_CACHE_MANAGER);
        ListView listPpuCache = (ListView) layout.findViewById(R.id.list_ppu_cache);

        final int titleResId = R.string.ppu_cache_manager;
        if(getSupportActionBar()!=null) {
            getSupportActionBar().setTitle(getString(titleResId));
        }

        Button btn_import = (Button) layout.findViewById(R.id.btn_ppu_import);
        btn_import.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                MainActivity.request_file_select(UserDataActivity.this, REQUEST_IMPORT_PPU_CACHE);
            }
        });

        final File[] _all_ppu_cache_dirs = MainActivity.get_all_ppu_cache_dirs();
        final ArrayList<File> list = new ArrayList<>();
        if(_all_ppu_cache_dirs!=null)
        for(File dir:_all_ppu_cache_dirs) {
            if(dir.getName().length()==9)
                list.add(dir);
        }

        listPpuCache.setAdapter(new ArrayAdapter<File>(this, android.R.layout.simple_list_item_2, list){
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                if(convertView==null)
                    convertView = getLayoutInflater().inflate(android.R.layout.simple_list_item_2, parent, false);

                TextView text1 = convertView.findViewById(android.R.id.text1);
                TextView text2 = convertView.findViewById(android.R.id.text2);

                String serial = getItem(position).getName();
                String gameName = getGameName(serial);
                text1.setText(gameName);
                text2.setText(serial);

                return convertView;
            }
        });
        listPpuCache.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                handle_item_click((ListAdapter) parent.getAdapter(), position);
            }
        });

        current_page_type = PAGE_TYPE_PPU_CACHE_MANAGER;
    }

    private void handle_ppu_cache_item_click(ListAdapter adapter, int position) {
        File item = (File) adapter.getItem(position);

        String[] options = {getString(R.string.export_ppu_cache)};
        new AlertDialog.Builder(this)
                .setTitle(getGameName(item.getName()))
                .setItems(options, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        switch (which) {
                            case 0:
                                export_ppu_cache(item);
                                break;
                        }
                    }
                })
                .show();
    }

    void export_ppu_cache(File dir){
        File save_to_dir = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), "aps3e");
        if (!save_to_dir.exists()) {
            save_to_dir.mkdirs();
        }
        final String save_name = dir.getName() + ".zip";
        final File save_to_file = new File(save_to_dir, save_name);

        ProgressTask pt=new ProgressTask(this);
        pt.set_done_task(new ProgressTask.UI_Task(){
            @Override
            public void run() {
                Toast.makeText(UserDataActivity.this,String.format(getString(R.string.save_to),save_to_file.getAbsolutePath()),Toast.LENGTH_LONG).show();
            }
        }).call(new ProgressTask.Task(){
            @Override
            public void run(ProgressTask task) {
                try {
                    zip_ppu_cache(dir, save_to_file);
                    task.task_handler.sendEmptyMessage(ProgressTask.TASK_DONE);
                } catch (Exception e) {
                    task.task_handler.sendEmptyMessage(ProgressTask.TASK_FAILED);
                }
            }
        });
    }

    private static boolean zip_ppu_cache(File dir, File out_f) throws Exception {
        FileOutputStream fos=null;
        ZipOutputStream zos=null;
        try{
            fos=new FileOutputStream(out_f);
            zos=new java.util.zip.ZipOutputStream(fos);
            zip_ppu_cache_file(dir, zos, "");
        } catch (Exception e) {
            throw e;
        }
        finally {
            if(zos!=null) zos.close();
            if(fos!=null) fos.close();
        }
        return true;
    }

    private static void zip_ppu_cache_file(File file, ZipOutputStream zos, String path) throws Exception {
        String entryName = path + file.getName();

        if (file.isDirectory()) {
            File[] files = file.listFiles();
            if (files != null) {
                for (File f : files) {
                    zip_ppu_cache_file(f, zos, entryName + "/");
                }
            }
        } else if(file.getName().endsWith(".obj.gz")){
            ZipEntry zip_entry = new ZipEntry(entryName);
            zos.putNextEntry(zip_entry);
            try (FileInputStream fis = new FileInputStream(file)) {
                byte[] buffer = new byte[16384];
                int len;
                while ((len = fis.read(buffer)) > 0) {
                    zos.write(buffer, 0, len);
                }
            }
            zos.closeEntry();
        }
    }
    private static boolean zip_user_data(File dir, File out_f) throws Exception {
        FileOutputStream fos=null;
        ZipOutputStream zos=null;
        try{
            fos=new FileOutputStream(out_f);
            zos=new java.util.zip.ZipOutputStream(fos);
            zip_user_data_file(dir, zos, "");
        } catch (Exception e) {
            throw e;
        }
        finally {
            if(zos!=null) zos.close();
            if(fos!=null) fos.close();
        }
        return true;
    }

    private static void zip_user_data_file(File file, ZipOutputStream zos, String path) throws Exception {
        String entryName = path + file.getName();

        if (file.isDirectory()) {
            File[] files = file.listFiles();
            if (files != null) {
                for (File f : files) {
                    zip_user_data_file(f, zos, entryName + "/");
                }
            }
        } else{
            ZipEntry zip_entry = new ZipEntry(entryName);
            zos.putNextEntry(zip_entry);
            try (FileInputStream fis = new FileInputStream(file)) {
                byte[] buffer = new byte[16384];
                int len;
                while ((len = fis.read(buffer)) > 0) {
                    zos.write(buffer, 0, len);
                }
            }
            zos.closeEntry();
        }
    }

    void handle_import_ppu_cache(Uri uri) {
        ProgressTask pt = new ProgressTask(this);
        pt.set_done_task(new ProgressTask.UI_Task() {
            @Override
            public void run() {
                Toast.makeText(UserDataActivity.this, R.string.import_ppu_cache_done, Toast.LENGTH_SHORT).show();
                show_ppu_cache_page(); // 刷新列表
            }
        }).set_failed_task(new ProgressTask.UI_Task() {
            @Override
            public void run() {
                Toast.makeText(UserDataActivity.this, R.string.import_ppu_cache_failed, Toast.LENGTH_SHORT).show();
            }
        }).call(new ProgressTask.Task() {
            @Override
            public void run(ProgressTask task) {
                try {
                    import_ppu_cache_from_uri(uri);
                    task.task_handler.sendEmptyMessage(ProgressTask.TASK_DONE);
                } catch (Exception e) {
                    Log.e("APS3E", e.getMessage());
                    e.printStackTrace();
                    task.task_handler.sendEmptyMessage(ProgressTask.TASK_FAILED);
                }
            }
        });
    }

    static File get_ppu_cache_base_dir() {
        return new File(Application.get_app_data_dir(), "cache/cache");
    }

    private void import_ppu_cache_from_uri(Uri uri) throws Exception {
        ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "r");
        if (pfd == null) {
            throw new RuntimeException("xx");
        }

        FileInputStream fis = new FileInputStream(pfd.getFileDescriptor());
        ZipInputStream zis = new ZipInputStream(fis);

        File cache_base_dir = get_ppu_cache_base_dir();
        if (!cache_base_dir.exists()) {
            cache_base_dir.mkdirs();
        }

        ZipEntry entry;
        int extracted_count = 0;

        while ((entry = zis.getNextEntry()) != null) {
            if (entry.isDirectory()) {
                continue;
            }

            String entry_name = entry.getName();

            if (!entry_name.endsWith(".obj.gz")) {
                zis.closeEntry();
                continue;
            }

            String sub_path=entry_name.substring(0, entry_name.lastIndexOf( "/"));
            File targetDir = new File(cache_base_dir, sub_path);

            if (!targetDir.exists()) {
                targetDir.mkdirs();
            }

            File out_f = new File(targetDir, entry_name.substring(entry_name.lastIndexOf("/") + 1));

            FileOutputStream fos = new FileOutputStream(out_f);
            byte[] buffer = new byte[16384];
            int len;
            while ((len = zis.read(buffer)) > 0) {
                fos.write(buffer, 0, len);
            }
            fos.close();
            extracted_count++;

            zis.closeEntry();
        }

        zis.close();
        fis.close();
        pfd.close();

        if (extracted_count == 0) {
            throw new Exception("未找到有效的 PPU 缓存文件");
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (resultCode != RESULT_OK || data == null) {
            return;
        }

        Uri uri = data.getData();
        if (uri == null) {
            return;
        }

        switch (requestCode) {
            case REQUEST_EXPORT_PPU_CACHE:
                break;
            case REQUEST_IMPORT_PPU_CACHE:
                handle_import_ppu_cache(uri);
                break;
            case REQUEST_SELECT_ISO_DIR:
                handle_add_iso_dir(uri);
                break;
            case REQUEST_IMPORT_GAME_DATA:
                handle_import_game_data(uri);
                break;
        }
    }

    private void show_iso_dir_page() {
        View layout=select_layout(PAGE_TYPE_ISO_DIR_MANAGER);

        final int titleResId = R.string.iso_dir_manager;
        if(getSupportActionBar()!=null) {
            getSupportActionBar().setTitle(getString(titleResId));
        }

        Button btnAddIsoDir=(Button)layout.findViewById(R.id.btn_iso_dir_add);
        btnAddIsoDir.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                MainActivity.request_dir_select(UserDataActivity.this,REQUEST_SELECT_ISO_DIR);
            }
        });

        ListView listIsoDir=(ListView)layout.findViewById(R.id.list_iso_dir);

        Set<Uri> dirs = MainActivity.load_pref_extra_iso_dirs(this);

        Uri mainIsoUri = MainActivity.load_pref_iso_dir(this);

        if (mainIsoUri != null) {
            dirs.add(mainIsoUri);
        }

        listIsoDir.setAdapter(new ArrayAdapter<Uri>(this, android.R.layout.simple_list_item_1, new ArrayList<>(dirs)){
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                if(convertView==null)
                    convertView=getLayoutInflater().inflate(android.R.layout.simple_list_item_1,parent, false);
                TextView textView = (TextView) convertView.findViewById(android.R.id.text1);

                textView.setText(getItem(position).toString());
                return convertView;
            }
        });
        listIsoDir.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                handle_item_click((ListAdapter) parent.getAdapter(), position);
            }
        });

        current_page_type = PAGE_TYPE_ISO_DIR_MANAGER;
    }

    private void handle_iso_dir_item_click(ListAdapter adapter, int position) {
        Uri item = (Uri) adapter.getItem(position);

        String[] options = {getString(R.string.remove_directory)};
        new AlertDialog.Builder(this)
                .setTitle(R.string.iso_dir_manager)
                .setItems(options, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        switch (which) {
                            case 0: // 移除
                                remove_iso_dir(item);
                                break;
                        }
                    }
                })
                .show();
    }

    private void remove_iso_dir(Uri uri) {
        try {
            Uri mainIsoUri = MainActivity.load_pref_iso_dir(this);
            if (mainIsoUri != null && mainIsoUri.equals(uri)) {
                MainActivity.save_pref_iso_dir(this, null);
                show_iso_dir_page();
                return;
            }

            Set<Uri> extraIsoDirs = MainActivity.load_pref_extra_iso_dirs(this);
            extraIsoDirs.remove(uri);
            MainActivity.save_pref_extra_iso_dirs(this, extraIsoDirs);

            show_iso_dir_page();
        } catch (Exception e) {
            e.printStackTrace();
            Toast.makeText(this, "Failed：" + e.getMessage(), Toast.LENGTH_SHORT).show();
        }
    }

    private void handle_add_iso_dir(Uri uri) {
        Uri mainIsoUri = MainActivity.load_pref_iso_dir(this);
        if (mainIsoUri != null && mainIsoUri.equals(uri)) {
            return;
        }
        Set<Uri> extraIsoDirs = MainActivity.load_pref_extra_iso_dirs(this);
        if(extraIsoDirs.contains(uri)){
            return;
        }
        if(mainIsoUri== null){
            MainActivity.save_pref_iso_dir(this, uri);
        }
        else{
            extraIsoDirs.add(uri);
            MainActivity.save_pref_extra_iso_dirs(this, extraIsoDirs);
        }
        show_iso_dir_page();
    }

    private void show_compatibility_table_page() {
        View layout =select_layout(PAGE_TYPE_COMPATIBILITY_TABLE);

        final int titleResId = R.string.compatibility_table;
        if(getSupportActionBar()!=null) {
            getSupportActionBar().setTitle(getString(titleResId));
        }

        TextView text_compat_content=(TextView)layout.findViewById(R.id.text_compat_content);

        try {
            ArrayList<Emulator.MetaInfo> metas= MainActivity.GameMetaInfoAdapter.load_game_list_from_json_file(
                    MainActivity.get_game_list_file());
            byte[] compatData = Utils.load_assets_file(this, "compatibility_table.json");
            JSONObject json = new JSONObject(new String(compatData));
            StringBuilder sb = new StringBuilder();
            for(Emulator.MetaInfo meta:metas){
                JSONObject compat_info = json.optJSONObject(meta.serial);
                if (compat_info != null) {
                    sb.append(meta.name).append("\n");
                    sb.append(compat_info.getString("status")).append("\n");
                }
            }
            String text=sb.toString();
            SpannableString spannableString = new SpannableString(text);
                            
            String[] lines = text.split("\n");
            int lineStartIndex = 0;
            for (int i = 0; i < lines.length; i += 2) {
                if (i + 1 < lines.length) {
                    String gameName = lines[i];
                    String status = lines[i + 1];
                                    
                    int statusStart = lineStartIndex + gameName.length() + 1;
                    int statusEnd = statusStart + status.length();
                                    
                    int color;
                    if (status.equals("Playable")) {
                        color = Color.GREEN;
                    } else if (status.equals("Ingame")) {
                        color = Color.rgb(255, 165, 0);
                    } else if (status.equals("Intro")) {
                        color = Color.RED;
                    } else if (status.equals("Loadable")) {
                        color = Color.GRAY;
                    } else {
                        color = Color.GRAY;
                    }
                                    
                    spannableString.setSpan(new ForegroundColorSpan(color), 
                            statusStart, statusEnd, SpannableString.SPAN_EXCLUSIVE_EXCLUSIVE);
                                    
                    lineStartIndex += gameName.length() + status.length() + 2;
                }
            }
                            
            text_compat_content.setText(spannableString);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

        current_page_type = PAGE_TYPE_COMPATIBILITY_TABLE;
    }

    private void show_patch_table_page() {
        View layout = select_layout(PAGE_TYPE_PATCH_MANAGER);

        final int titleResId = R.string.patch_manager;
        if(getSupportActionBar()!=null) {
            getSupportActionBar().setTitle(getString(titleResId));
        }

        TextView text_patch_content = (TextView) layout.findViewById(R.id.text_patch_content);

        try {
            byte[] patchData = Utils.load_assets_file(this, "patch.yml");
            String content = new String(patchData, "UTF-8");
            String fileSize = format_file_size(patchData.length);
            int previewLen = Math.min(content.length(), 3000);
            String preview = content.substring(0, previewLen);
            if(content.length() > 3000) preview += "\n...";
            text_patch_content.setText(getString(R.string.patch_downloaded_info, fileSize) + "\n\n" + preview);
        } catch (Exception e) {
            text_patch_content.setText(e.getMessage());
        }

        current_page_type = PAGE_TYPE_PATCH_MANAGER;
    }

    private static String format_file_size(long bytes) {
        if(bytes < 1024) return bytes + " B";
        if(bytes < 1024*1024) return String.format("%.1f KB", bytes/1024.0);
        return String.format("%.1f MB", bytes/(1024.0*1024));
    }
    private void show_driver_manager_page() {
        ListView listDriver=(ListView)select_layout(PAGE_TYPE_DRIVER_MANAGER);

        final int titleResId = R.string.driver_manager;
        if(getSupportActionBar()!=null) {
            getSupportActionBar().setTitle(getString(titleResId));
        }

        ArrayList<File> items = new ArrayList<>();

        File driverDir = Application.get_custom_driver_dir();
        File[] driverFiles = driverDir.listFiles();
        if(driverFiles == null) driverFiles = new File[0];

        listDriver.setAdapter(new ArrayAdapter<File>(this, android.R.layout.simple_list_item_1, driverFiles){
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                if(convertView==null)
                    convertView=getLayoutInflater().inflate(android.R.layout.simple_list_item_1, parent, false);
                TextView textView=(TextView) convertView.findViewById(android.R.id.text1);
                if(getItem( position).isDirectory()){
                    textView.setText(getItem( position).getName()+"/");
                }
                else{
                    textView.setText(getItem( position).getName());
                }
                return convertView;
            }
        });
        listDriver.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                handle_item_click((ListAdapter) parent.getAdapter(), position);
            }
        });

        current_page_type = PAGE_TYPE_DRIVER_MANAGER;
    }

    private void handle_driver_item_click(ListAdapter adapter, int position) {
        File item = (File) adapter.getItem(position);

        String[] options = {getString(R.string.information), getString(R.string.delete)};
        new AlertDialog.Builder(this)
                .setTitle(item.getName())
                .setItems(options, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        switch (which) {
                            case 0: // 查看信息
                                show_driver_info(item);
                                break;
                            case 1: // 删除驱动
                                if(item.isDirectory()){
                                    try {
                                        DocumentsProvider.recursive_delete_sub_files(item);
                                    } catch (FileNotFoundException e) {
                                    }
                                }
                                item.delete();
                                break;
                        }
                    }
                })
                .show();
    }

    private void show_driver_info(File item) {
        String info=null;
        if(item.isFile()){
            info=item.getName();
        }
        else if(item.isDirectory()){
            File meta_json=new File(item, "meta.json");
            if(meta_json.exists()){
                try {
                    JSONObject meta = new JSONObject(Utils.read_file_as_str(meta_json));
                    info=meta.getString("name")+"\n"+
                            meta.getString("driverVersion");
                } catch (JSONException e) {
                    info=e.getMessage();
                }
            }
        }

        new AlertDialog.Builder(this)
            .setTitle(getString(R.string.information))
            .setMessage(info)
            .setPositiveButton(android.R.string.ok, null)
            .show();
    }

    private void show_game_data_page() {
        View layout = select_layout(PAGE_TYPE_GAME_DATA_MANAGER);

        final int titleResId = R.string.game_data_manager;
        if(getSupportActionBar()!=null) {
            getSupportActionBar().setTitle(getString(titleResId));
        }

        Button btn_import = (Button) layout.findViewById(R.id.btn_game_data_import);
        btn_import.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                MainActivity.request_file_select(UserDataActivity.this, REQUEST_IMPORT_GAME_DATA);
            }
        });

        Button btn_export = (Button) layout.findViewById(R.id.btn_game_data_export);
        btn_export.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                export_game_data();
            }
        });

        current_page_type = PAGE_TYPE_GAME_DATA_MANAGER;
    }

    void export_game_data(){
        File save_to_dir = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), "aps3e");
        if (!save_to_dir.exists()) {
            save_to_dir.mkdirs();
        }
        final String save_name = "user_data_00000001.zip";
        final File save_to_file = new File(save_to_dir, save_name);

        final String user_id = "00000001";
        final String child = String.format("config/dev_hdd0/home/%s", user_id);
        final File user_dir = new File(Application.get_app_data_dir(), child);

        ProgressTask pt=new ProgressTask(this);
        pt.set_done_task(new ProgressTask.UI_Task(){
            @Override
            public void run() {
                Toast.makeText(UserDataActivity.this,String.format(getString(R.string.save_to),save_to_file.getAbsolutePath()),Toast.LENGTH_LONG).show();
            }
        }).call(new ProgressTask.Task(){
            @Override
            public void run(ProgressTask task) {
                try {
                    if (user_dir.exists()) {
                        zip_user_data(user_dir, save_to_file);
                    }
                    task.task_handler.sendEmptyMessage(ProgressTask.TASK_DONE);
                } catch (Exception e) {
                    Log.e("APS3E", "Export game data failed: " + e.getMessage());
                    e.printStackTrace();
                    task.task_handler.sendEmptyMessage(ProgressTask.TASK_FAILED);
                }
            }
        });
    }

    void handle_import_game_data(Uri uri) {
        ProgressTask pt = new ProgressTask(this);
        pt.set_done_task(new ProgressTask.UI_Task() {
            @Override
            public void run() {
                Toast.makeText(UserDataActivity.this, R.string.import_game_data_done, Toast.LENGTH_SHORT).show();
            }
        }).set_failed_task(new ProgressTask.UI_Task() {
            @Override
            public void run() {
                Toast.makeText(UserDataActivity.this, R.string.import_game_data_failed, Toast.LENGTH_SHORT).show();
            }
        }).call(new ProgressTask.Task() {
            @Override
            public void run(ProgressTask task) {
                try {
                    import_game_data_from_uri(uri);
                    task.task_handler.sendEmptyMessage(ProgressTask.TASK_DONE);
                } catch (Exception e) {
                    Log.e("APS3E", "Import game data failed: " + e.getMessage());
                    e.printStackTrace();
                    task.task_handler.sendEmptyMessage(ProgressTask.TASK_FAILED);
                }
            }
        });
    }

    private void import_game_data_from_uri(Uri uri) throws Exception {
        ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "r");
        if (pfd == null) {
            throw new RuntimeException("Failed to open file");
        }

        FileInputStream fis = new FileInputStream(pfd.getFileDescriptor());
        ZipInputStream zis = new ZipInputStream(fis);

        //final String user_id = "00000001";
        final String child = String.format("config/dev_hdd0/home");
        final File home_dir = new File(Application.get_app_data_dir(), child);
        
        if (!home_dir.exists()) {
            home_dir.mkdirs();
        }

        ZipEntry entry;
        int extracted_count = 0;

        while ((entry = zis.getNextEntry()) != null) {
            if (entry.isDirectory()) {
                zis.closeEntry();
                continue;
            }

            String entry_name = entry.getName();
            File out_f = new File(home_dir, entry_name);

            File parentDir = out_f.getParentFile();
            if (parentDir != null && !parentDir.exists()) {
                parentDir.mkdirs();
            }

            FileOutputStream fos = new FileOutputStream(out_f);
            byte[] buffer = new byte[16384];
            int len;
            while ((len = zis.read(buffer)) > 0) {
                fos.write(buffer, 0, len);
            }
            fos.close();
            extracted_count++;

            zis.closeEntry();
        }

        zis.close();
        fis.close();
        pfd.close();

        if (extracted_count == 0) {
            throw new Exception("Invalid file");
        }
    }

    private String getGameName(String serial) {
        try {
            ArrayList<Emulator.MetaInfo> metas = MainActivity.GameMetaInfoAdapter.load_game_list_from_json_file(MainActivity.get_game_list_file());
            for (Emulator.MetaInfo meta : metas) {
                if (meta.serial.equals(serial)) {
                    return meta.name;
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return serial;
    }

    private View select_layout(int page_ty) {
        for(View v:layout_list.values()){
            v.setVisibility(View.GONE);
        }
        View v=layout_list.get(page_ty);
        v.setVisibility(View.VISIBLE);
        return v;
    }

}
