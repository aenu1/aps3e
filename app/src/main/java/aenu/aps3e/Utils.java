// SPDX-License-Identifier: WTFPL
package aenu.aps3e;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Paint;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.util.Log;
import android.view.Window;
import android.view.WindowManager;

import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.List;
import android.content.res.Configuration;

public class Utils {

    /*
     * targetSdk>=35 强制边到边(edge-to-edge)显示,状态栏/导航栏透明,
     * 必须显式设置系统栏图标颜色,否则图标可能与窗口背景同色而不可见:
     * 亮色主题(白色背景) -> 深色图标;暗色主题(深色背景) -> 浅色图标。
     */
    public static void setup_system_bars_appearance(Activity a){
        Window w=a.getWindow();
        WindowInsetsControllerCompat wic=WindowCompat.getInsetsController(w,w.getDecorView());
        boolean is_night=(a.getResources().getConfiguration().uiMode
                &Configuration.UI_MODE_NIGHT_MASK)==Configuration.UI_MODE_NIGHT_YES;
        wic.setAppearanceLightStatusBars(!is_night);
        wic.setAppearanceLightNavigationBars(!is_night);
    }

    public static String getCurrentProcessName(Context context) {
        int pid = android.os.Process.myPid();
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        List<ActivityManager.RunningAppProcessInfo> runningApps = am.getRunningAppProcesses();
        if (runningApps!= null) {
            for (ActivityManager.RunningAppProcessInfo procInfo : runningApps) {
                if (procInfo.pid == pid) {
                    return procInfo.processName;
                }
            }
        }
        return null;
    }

    public static void extractAssetsDir(Context context, String assertDir, File outputDir) {
        AssetManager assetManager = context.getAssets();
        try {
            if (!outputDir.exists()) {
                outputDir.mkdirs();
            }

            String[] filesToExtract = assetManager.list(assertDir);
            if (filesToExtract!= null) {
                for (String file : filesToExtract) {
                    File outputFile = new File(outputDir, file);
                    if(outputFile.exists())continue;

                    InputStream in = assetManager.open(assertDir + "/" + file);
                    FileOutputStream out = new FileOutputStream(outputFile);
                    byte[] buffer = new byte[16384];
                    int read;
                    while ((read = in.read(buffer))!= -1) {
                        out.write(buffer, 0, read);
                    }
                    in.close();
                    out.close();
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public  static byte[] load_assets_file(Context ctx,String asset_file_path) {
        try {
            InputStream in = ctx.getAssets().open(asset_file_path);
            int size = in.available();
            byte[] buffer = new byte[size];
            in.read(buffer);
            in.close();
            return buffer;
        } catch (IOException e) {
            e.printStackTrace();
            return null;
        }
    }

    static void copy_file(File src,File dst) {
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

    static String read_file_as_str(File f) {
        try {
            FileInputStream in=new FileInputStream(f);
            int size=in.available();
            byte[] buffer=new byte[size];
            in.read(buffer);
            in.close();
            return new String(buffer);
        } catch (IOException e) {
            e.printStackTrace();
            return null;
        }
    }

    public static void enable_fullscreen(Window w){
        WindowCompat.setDecorFitsSystemWindows(w,false);
        WindowInsetsControllerCompat wic=WindowCompat.getInsetsController(w,w.getDecorView());
        wic.hide(WindowInsetsCompat.Type.systemBars());
        wic.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        WindowManager.LayoutParams lp=w.getAttributes();
        lp.layoutInDisplayCutoutMode=WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        w.setAttributes(lp);
    }

    static String getFileNameFromUri(Uri uri) {
        String fileName = null;
        Cursor cursor = Application.ctx.getContentResolver().query(
                uri,
                new String[]{DocumentsContract.Document.COLUMN_DISPLAY_NAME},
                null, null, null
        );
        if (cursor != null) {
            if (cursor.moveToFirst()) {
                fileName = cursor.getString(cursor.getColumnIndexOrThrow(
                        DocumentsContract.Document.COLUMN_DISPLAY_NAME
                ));
            }
            cursor.close();
        }
        return fileName;
    }

    static Bitmap to_gray_bmp(Bitmap src){
        int w=src.getWidth();
        int h=src.getHeight();
        Bitmap bmp=Bitmap.createBitmap(w,h,src.getConfig());
        ColorMatrix cm=new ColorMatrix();
        cm.setSaturation(0);
        Canvas canvas=new Canvas(bmp);
        Paint paint=new Paint();
        paint.setColorFilter(new ColorMatrixColorFilter(cm));
        canvas.drawBitmap(src,0,0,paint);
        return bmp;
    }

    static int detach_open_uri(Context ctx,Uri uri){
        try {
            return ctx.getContentResolver().openFileDescriptor(uri,"r").detachFd();
        } catch (FileNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    static int try_open_key_fd(Context ctx, Uri isoUri){
        int fd = try_open_fd_with_ext(ctx, isoUri, ".dkey");
        if (fd != -1) return fd;
        return try_open_fd_with_ext(ctx, isoUri, ".key");
    }

    private static int try_open_fd_with_ext(Context ctx, Uri isoUri, String ext){
        try {
            String docId = DocumentsContract.getDocumentId(isoUri);
            int dotPos = docId.lastIndexOf('.');
            if (dotPos <= 0) return -1;
            String keyDocId = docId.substring(0, dotPos) + ext;
            int slashPos = docId.lastIndexOf('/');
            String treeId = docId.substring(0, slashPos);
            Uri treeUri = DocumentsContract.buildTreeDocumentUri(isoUri.getAuthority(), treeId);
            Uri keyUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, keyDocId);
            return ctx.getContentResolver().openFileDescriptor(keyUri, "r").detachFd();
        } catch (Exception e) {
            return -1;
        }
    }

    static long convert_hex_str_to_long(String hex){
        if(hex.startsWith("0x"))hex=hex.substring(2);
        return Long.parseLong(hex.toUpperCase(),16);
    }
    static void open_file_manager(Activity activity)
    {
        try{
            Intent it=new Intent(Intent.ACTION_VIEW);
            it.setClassName("com.android.documentsui", "com.android.documentsui.files.FilesActivity");
            it.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            activity.startActivity(it);
            return;
        }
        catch(Exception e){
        }
        try{
            Intent it=new Intent(Intent.ACTION_VIEW);
            it.setClassName("com.google.android.documentsui", "com.android.documentsui.files.FilesActivity");
            it.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            activity.startActivity(it);
            return;

        }
        catch(Exception e){
        }
    }
}
