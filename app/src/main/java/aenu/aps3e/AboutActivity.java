// SPDX-License-Identifier: WTFPL
package aenu.aps3e;

import android.app.Activity;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.webkit.WebView;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;

public class AboutActivity extends AppCompatActivity {


    public static String get_update_log(){
        final String log="\n"
                +"0.1(2025-01-06)\n"
                + " *首个版本\n"
                +"0.2(2025-01-13)\n"
                + " *修正socket无法创建的bug\n"
                + " *添加更新日志\n"
                + " *新的用户界面\n"
                + " *修正了一个iso安装的bug\n"
                + " *修正了cpu架构检测错误的bug\n"
                + " *添加.nomedia禁止媒体存储扫描\n"
                +"0.3(2025-01-14)\n"
                + " *游戏界面设置为全屏\n"
                + " *日志行为修改\n"
                + " *初步完善虚拟键盘\n"
                + " *移除sharedUserId属性用于兼容安卓13+\n"
                + " *修正了压缩纹理卡死的bug\n"
                + " *虚拟键盘增加L2,L3,R2,R3\n"
                +"0.4(2025-01-17)\n"
                + " *修正了统一缓冲区更新卡死的BUG\n"
                + " *修正了多线程按键资源访问冲突导致的闪退\n"
                + " *同步部分代码以解决卡奖杯的问题\n"
                + " *BC纹理格式支持\n"
                + " *虚拟键盘位置调整\n"
                + " *加入了图标(Icons/ui/*)\n"
                + " *游戏准备阶段消息变更\n"
                +"0.5(2025-02-19)\n"
                + " *右摇杆控制修复\n"
                + " *站点搭建完成，欢迎访问😄\n"
                + " *支持pkg格式安装\n"
                + " *初步的英语本地化支持\n"
                + " *启动失败显示错误信息(目前效果很烂)\n"
                + " *PS键追加\n"
                +"0.6(2025-02-23)\n"
                + " *LR键位修正\n"
                + " *加入按键映射(目前还无法正常使用)\n"
                + " *加入虚拟键盘一定时长自动隐藏\n"
                + " *加入鸣谢列表\n"
                + " *加入虚拟键盘编辑\n"
                + " *新的图标，由Ruban提供\n"
                +"0.7(2025-03-02)\n"
                + " *修正了部分着色器缓存相关的BUG，目前可以启用了\n"
                + " *更新LLVM至19.1\n"
                + " *修正了设备不支持depthClamp闪退的问题\n"
                + " *修正按键映射\n"
                + " *安装PKG/ISO时自定义路径\n"
                +"0.8(2025-03-06)\n"
                + " *支持的最低版本更新为Android 8.1(API27)\n"
                + " *音频后端更改为AAudio\n"
                + " *修正按键映射(LR)\n"
                + " *载入着色器缓存界面修正\n"
                + " *同步部分代码以解决压缩纹理问题\n"
                + " *降低编译PPU模块线程数量为4，防止部分设备爆内存\n"
                +"0.9(2025-03-24)\n"
                + " *3D纹理修正(未测试)\n"
                + " *同步部分代码以解决GCM事件问题\n"
                + " *同步部分代码(多个修正)\n"
                + " *增加了设置界面(不能用，还没做完)\n"
                + " *完善了设置界面\n"
                + " *修复了退出死锁的问题\n"
                +"0.10(2025-04-06)\n"
                + " *调整默认色彩格式为RGBA\n"
                + " *后端结构调整，修正了顶点更新的问题\n"
                + " *同步部分代码(0328)\n"
                + " *修正了PS键界面无字的问题\n"
                + " *修正了消息框无字的问题\n"
                + " *颜色修正\n"
                + " *返回键弹出退出框\n"
                + " *默认配置调整\n"
                +"0.11(2025-04-24)\n"
                + " *更新glslang至15.2.0\n"
                + " *数据目录调整\n"
                + " *支持自定义驱动\n"
                + " *设置界面完善\n"
                + " *修复了应用后台崩溃\n"
                + " *实现了SaveDialog\n"
                + " *实现了奖杯通知\n"
                + " *游戏安装行为变更\n"
                +"1.12(2025-04-27)\n"
                + " *修正了奖杯通知和存档界面\n"
                + " *顶点buffer初始值修正\n"
                +"1.13(2025-05-01)\n"
                + " *添加顶点buffer更新模式选择\n"
                + " *添加了文件夹格式支持\n"
                +"1.14(2025-05-08)\n"
                + " *调整默认字体为来自固件，防止Android15崩溃\n"
                + " *添加了字体选择\n"
                + " *关于界面调整，获取cpu/gpu信息变更\n"
                +"1.15(2025-05-12)\n"
                + " *虚拟键盘UI更新\n"
                + " *虚拟键盘编辑更新（可调整大小，重置）\n"
                +"1.16(2025-05-16)\n"
                + " *修复全屏模式左侧黑边\n"
                + " *自定义驱动|字体路径，以列表形式显示\n"
                + " *添加设置选项 重置为默认\n"
                +"1.17(2025-05-20)\n"
                + " *ISO格式支持\n"
                + " *菜单调整\n"
                + " *修复文件管理无法删除目录的问题\n"
                +"1.18(2025-05-24)\n"
                + " *完善ISO支持\n"
                + " *追加了MsgDialog,OskDialog\n"
                +"1.19(2025-05-28)\n"
                + " *载入时加载背景图(ISO)\n"
                + " *修正了ISO支持\n"
                + " *修正和完善了主界面的上下文菜单\n"
                //+ " *支持了16kb页\n"
                + "1.20(2025-06-03)\n"
                + " *修复了自定义驱动不生效的问题\n"
                + " *设置清理与调整\n"
                + " *修正了删除游戏数据不生效的问题，添加删除着色器缓存选项\n"
                + "1.21(2025-06-11)\n"
                + " *更改纹理格式为RGBA\n"
                + " *设置完善\n"
                + "1.22(2025-06-16)\n"
                + " *默认使用cpu处理纹理(用于修复Adreno 7xx默认驱动崩溃的问题)\n"
                + " *检测并阻止启动无法解密的游戏\n"
                + " *清理无效的主菜单（PS3）选项\n"
                + " *修复游戏数量较多时刷新列表会产生ANR的问题\n"
                + " *增加选项（纹理更新模式）\n"
                + " *增加选项（字体大小）\n"
                //+ " *修复UB\n"
                //+ " *协议追加\n"
                + "1.23(2025-06-19)\n"
                + " *优化了主菜单界面\n"
                + " *修复了按键映射无效的问题\n"
                + " *增加了选项使用BGRA格式并默认启用\n"
                + " *设置优化（库控制排序，进度条可编辑）\n"
                + "1.24(2025-06-24)\n"
                + " *统一界面风格\n"
                + " *修复了方向键和摇杆\n"
                + " *可为游戏创建单独的配置\n"
                + " *增加些调试功能\n"
                + "1.25(2025-06-30)\n"
                + " *虚拟键盘更新，增加动态摇杆，可设置组缩放\n"
                + " *增加了按键震动\n"
                + " *更改ffmpeg版本为5.1.6\n"
                + "1.26(2025-07-09)\n"
                + " *调整风格为跟随系统\n"
                + " *加入了快速开始页面\n"
                + " *修正了强制转换纹理选项\n"
                //+ " *自定义配置可重置为默认\n"
                //+ " *适配目标版本为安卓15\n"
                + " *修正删除着色器缓存无效的问题\n"
                //+ " *添加自定义驱动(*.zip)时自动处理命名\n"
                + "1.27(2025-07-19)\n"
                + " *修复未开启 使用BGRA格式 选项时产生的颜色错位\n"
                + " *设置和主界面更新\n"
                + " *修复部分ISO不识别的问题\n"
                + " *可禁用虚拟键盘\n"
                + " *加入日语，韩语和繁体中文的支持\n"
                + "1.28(2025-07-29)\n"
                + " *在Adreno 5xx/6xx设备上延迟0.5秒加载动态库和主界面，以修正其无法启动的问题\n"
                + " *增加添加快捷方式选项\n"
                + " *修正游戏重入后按键输入无效的问题\n"
                + " *增加线程亲和力掩码选项\n"
                + " *新增法语，俄语等共18种语言支持\n"
                + "1.29(2025-08-19)\n"
                //+ " *游戏退出项优化\n"
                + " *启动时检测设备是否支持Vulkan\n"
                + " *使用高精度的计时器\n"
                + " *同步部分RPCS3更新\n"
                + "1.30(2025-08-20)\n"
                //+ " *同步部分RPCS3更新（0820）\n"
                + " *回退掉RPCS3更新\n"
                + "1.31(2025-10-17)\n"
                + " *修复iso文件内日期不解析的bug\n"
                + " *修复sys_fs解析iso目录的问题\n"
                + " *打开DG类游戏时，自动安装PKG（需要删除config/dev_hdd0/game/$locks目录一次）\n"
                + " *可安装edat\n"
                + "1.32(2025-11-06)\n"
                + " *修复多个bug\n"
                + " *界面更新\n"
                + "1.33(2025-12-01)\n"
                + " *修复每次更新时按键错位的问题\n"
                + " *添加显示奖杯信息选项\n"
                + " *添加创建/删除PPU缓存，删除SPU缓存选项\n"
                + "1.34(2025-12-26)\n"
                + " *优化创建PPU缓存\n"
                + " *内存搜索/写入（未测试）\n"
                + "1.35(2025-12-26)\n"
                + " *移除创建PPU缓存选项（谷歌play没通过）\n"
                + "1.36(2026-02-24)\n"
                + " *修复方向键\n"
                + " *修复DLC不生效（.iso)\n"
                + " *修复设置界面旋转崩溃\n"
                + " *部分调整与优化\n"
                + "1.37(2026-03-21)\n"
                + " *适配不支持触屏的设备\n"
                + " *增加用户数据管理页面\n"
                + " *优化了内存搜索\n"
                + "1.38(2026-04-13)\n"
                + " *部分修正与完善\n"
                + "2.39(2026-05-29)\n"
                + " *更新RPCS3版本\n"
                + " *加密版ISO支持（需要对应的.dkey或.key文件）\n"
                + "2.40(2026-06-30)\n"
                + " *修复Mali设备的闪烁问题（感谢o-bin)\n"
                + " *补丁支持\n"
                + "2.41(2026-07-30)\n"
                + " *更新RPCS3版本\n"
                + " *部分修正与完善\n"
                + " \n";

        return log;
    }
    public static String gratitude_list(){
        //gratitude_content
        final String list="\n"

                + " callmerabbitz\n"//
                + " collazof\n"//
                + " devyprasetyo33\n"
                + " 再见某人\n"
                + " 同人小说\n"
                + " 糖ωσ心の爱\n"
                + " 不表态不经手不参与\n"
                + " VM GAMEDROID\n"
                + " brothason\n"
                + " gamerpro\n" //
                + " geovanem5\n"
                + " edjeffher33\n"
                + " Chakiel Zero Android\n"//
                + " Darwinp\n"//
                + " geovanem5\n"
                + " sarahi\n"
                + " melkygt0\n"
                + " Gratitud e\n"
                + " mediafire40\n"
                + " klekot\n"
                + " agustocastillo101\n"
                + " Paul\n"
                + " bakerrichard69\n"
                + " Bardok84\n"
                + " wingcom007\n"
                + " Max\n"
                + " kelve.p\n"
                + " Sophia\n"
                + " gonzaloinversionista\n"
                + " josekelvin482\n"
                + " superfuffa87\n"
                + " kim81austin\n"
                + " jblanm005\n"
                + " dlt31795\n"
                + " XZeusZX\n"
                + " neucorazaocleon98\n"
                + " Kyujj17\n"
                + " Ryan.p\n"
                + " 妖妖\n"
                + " 太空飞瓜\n"
                + " 明\n"
                + " 萌酱的小可爱\n"
                + " edjeffher33\n"
                + " fernandez21\n"
                + " yamil\n"
                + " matschilui2\n"
                + " 超玩游戏盒\n"
                + " 冰糖\n"
                + " christopher\n"
                + " dalelace\n"
                + " sandroloez\n"
                + " 石头\n"
                + " 鑫晓宇\n"
                ;

        String l= list.replace("\n","\n    *");
        l.substring(0,l.length()-1);
        return l+"\n\n";
    }
    TextView text;
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        supportRequestWindowFeature(Window.FEATURE_NO_TITLE);
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_about);
        
        // 设置 Toolbar
        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        if(getSupportActionBar()!=null) {
            getSupportActionBar().setTitle(R.string.about);
        }
        
        text=findViewById(R.id.about_text);
        findViewById(R.id.gratitude).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                text.setText(getString(R.string.gratitude_content)+gratitude_list()+getString(R.string.gratitude_content2));
            }
        });
        findViewById(R.id.open_source_licenses).setOnClickListener(new View.OnClickListener() {
            void show_licenses_dialog(){
                AlertDialog.Builder ab=new AlertDialog.Builder(AboutActivity.this);
                ab.setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface p1, int p2) {
                        p1.cancel();
                    }
                });
                WebView wv=new WebView(AboutActivity.this);
                wv.loadUrl("file:///android_asset/licenses.html");
                ab.setView(wv);
                ab.create().show();
            }
            @Override
            public void onClick(View v) {
                show_licenses_dialog();
            }
        });
        findViewById(R.id.update_log).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                text.setText(get_update_log());
            }
        });
        ((CheckBox)findViewById(R.id.enable_log)).setChecked(getSharedPreferences("debug",MODE_PRIVATE).getBoolean("enable_log",false));
        ((CheckBox)findViewById(R.id.enable_log)).setOnCheckedChangeListener(
                new CheckBox.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                        SharedPreferences pref=getSharedPreferences("debug",MODE_PRIVATE);
                        pref.edit().putBoolean("enable_log",isChecked).apply();
                    }
                }
        );

        text.setText(Emulator.get.simple_device_info());
        text.setTextIsSelectable(true);
        text.setLongClickable(true);
    }
}
