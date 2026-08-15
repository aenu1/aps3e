// SPDX-License-Identifier: WTFPL
#include <android/log.h>
#include <jni.h>
#include <filesystem>
#include <fstream>
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>
#include "emulator_aps3e.h"
#include "cpuinfo.h"
#include "Emu/system_config.h"
#include "Emu/Cell/Modules/sceNpTrophy.h"
#include "Loader/PSF.h"
#include "Loader/TROPUSR.h"


#define LOG_TAG "aps3e_native"

#define LOGW(...) {      \
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,"%s : %d",__func__,__LINE__);\
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__);\
}

#define LOGE(...) {      \
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,"%s : %d",__func__,__LINE__);\
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__);\
}

static jstring make_jstring(JNIEnv* env, std::string_view sv){
    //NewStringUTF要求合法Modified UTF-8,而SFO字段可能包含非UTF-8字节(如Shift-JIS标题),
    //直接传入会触发 JNI DETECTED ERROR 导致SIGABRT,这里把非法字节替换为'?'
    std::string s{sv};
    for(size_t i=0;i<s.size();){
        const unsigned char c=s[i];
        int len=0;
        if(c<0x80) len=1;
        else if((c&0xE0)==0xC0&&c>=0xC2) len=2;
        else if((c&0xF0)==0xE0) len=3;
        else if((c&0xF8)==0xF0) len=4;

        if(len==0||len>3||i+len>s.size()){
            s[i]='?';
            i++;
            continue;
        }
        bool ok=true;
        for(int k=1;k<len;k++){
            if((static_cast<unsigned char>(s[i+k])&0xC0)!=0x80){
                ok=false;
                break;
            }
        }
        if(!ok){
            s[i]='?';
            i++;
            continue;
        }
        i+=len;
    }
    return env->NewStringUTF(s.c_str());
}

static jboolean j_install_firmware(JNIEnv* env,jobject self,jint pup_fd){
    //jboolean is_copy=false;
    //const char* path=env->GetStringUTFChars(pup_path,&is_copy);
    jboolean result= ae::install_firmware(pup_fd);
    //env->ReleaseStringUTFChars(pup_path,path);
    return result;
}

/*
static jboolean j_install_pkg(JNIEnv* env,jobject self,jstring pkg_path){
	jboolean is_copy=false;
	const char* path=env->GetStringUTFChars(pkg_path,&is_copy);
    jboolean result= aps3e_util::install_pkg(path);
    env->ReleaseStringUTFChars(pkg_path,path);
	return result;
}*/

static jboolean j_install_pkg(JNIEnv* env,jobject self,jint pkg_fd){
    //jboolean is_copy=false;
    //const char* path=env->GetStringUTFChars(pkg_path,&is_copy);
    jboolean result= ae::install_pkg(pkg_fd);
    //env->ReleaseStringUTFChars(pkg_path,path);
    return result;
}

static jboolean j_install_edat(JNIEnv* env,jobject self,jint edat_fd){
    fs::file edat_f=fs::file::from_native_handle(edat_fd);
    return ae::install_edat(edat_f);
}

static jobject j_meta_info_from_dir(JNIEnv* env,jobject self,jstring jdir_path){

    auto fetch_psf_path=[](const std::string& dir_path){
        std::string sub_paths[]={
                "/PARAM.SFO","/PS3_GAME/PARAM.SFO"
        };
        for(std::string& sub_path:sub_paths){
            std::string psf_path=dir_path+sub_path;
            if(std::filesystem::exists(psf_path))
                return psf_path;
        }
        return std::string{};
    };

    auto fetch_eboot_path=[](const std::string& dir_path){
        std::string sub_paths[]=
                {
                        "/EBOOT.BIN",
                        "/USRDIR/EBOOT.BIN",
                        "/USRDIR/ISO.BIN.EDAT",
                        "/PS3_GAME/USRDIR/EBOOT.BIN",
                };
        for(std::string& sub_path:sub_paths){
            std::string eboot_path=dir_path+sub_path;
            if(std::filesystem::exists(eboot_path))
                return eboot_path;
        }
        return std::string{};
    };

    jboolean is_copy=false;
    const char* _dir_path=env->GetStringUTFChars(jdir_path,&is_copy);
    const std::string dir_path=_dir_path;
    env->ReleaseStringUTFChars(jdir_path, _dir_path);

    psf::registry psf=psf::load_object(fetch_psf_path(dir_path));
#if 0
    for(const auto& [key,value]:psf){
        switch (value.type()) {
            case psf::format::array:

                LOGW("key %s is array!",key.c_str());
                break;
            case psf::format::string:
                LOGW("key %s svalue %s",key.c_str(),value.as_string().c_str());
                break;
            case psf::format::integer:
                LOGW("key %s ivalue %d",key.c_str(),value.as_integer());
                break;
        }
    }
#endif
    jclass cls_MetaInfo=env->FindClass("aenu/aps3e/Emulator$MetaInfo");
    jmethodID mid_MetaInfo_ctor=env->GetMethodID(cls_MetaInfo,"<init>","()V");
    jfieldID fid_MetaInfo_eboot_path=env->GetFieldID(cls_MetaInfo,"eboot_path","Ljava/lang/String;");
    jfieldID fid_MetaInfo_name=env->GetFieldID(cls_MetaInfo,"name","Ljava/lang/String;");
    jfieldID fid_MetaInfo_serial=env->GetFieldID(cls_MetaInfo,"serial","Ljava/lang/String;");
    jfieldID fid_MetaInfo_category=env->GetFieldID(cls_MetaInfo,"category","Ljava/lang/String;");
    jfieldID fid_MetaInfo_version=env->GetFieldID(cls_MetaInfo,"version","Ljava/lang/String;");
    jfieldID fid_MetaInfo_decrypt=env->GetFieldID(cls_MetaInfo,"decrypt","Z");
    jfieldID  fid_MetaInfo_resolution=env->GetFieldID(cls_MetaInfo,"resolution","I");
    jfieldID  fid_MetaInfo_sound_format=env->GetFieldID(cls_MetaInfo,"sound_format","I");

    jobject meta_info=env->NewObject(cls_MetaInfo,mid_MetaInfo_ctor);

    std::string v;
    env->SetObjectField(meta_info,fid_MetaInfo_eboot_path,env->NewStringUTF((v=fetch_eboot_path(dir_path)).c_str()));
    env->SetBooleanField(meta_info,fid_MetaInfo_decrypt,true);
    env->SetObjectField(meta_info,fid_MetaInfo_name,make_jstring(env,psf::get_string(psf,"TITLE","")));
    env->SetObjectField(meta_info,fid_MetaInfo_serial,make_jstring(env,psf::get_string(psf,"TITLE_ID","")));
    env->SetObjectField(meta_info,fid_MetaInfo_category,make_jstring(env,psf::get_string(psf,"CATEGORY","??")));
    env->SetObjectField(meta_info,fid_MetaInfo_version,make_jstring(env,psf::get_string(psf,"APP_VER","")));
    env->SetIntField(meta_info,fid_MetaInfo_resolution,psf::get_integer(psf,"RESOLUTION",0));
    env->SetIntField(meta_info,fid_MetaInfo_sound_format,psf::get_integer(psf,"SOUND_FORMAT",0));
    return meta_info;
}


static jobject j_meta_info_from_iso(JNIEnv* env,jobject self,jint fd,jint dec_key_fd,jstring jiso_uri_path){

    jclass cls_MetaInfo=env->FindClass("aenu/aps3e/Emulator$MetaInfo");
    jmethodID mid_MetaInfo_ctor=env->GetMethodID(cls_MetaInfo,"<init>","()V");
    jfieldID fid_MetaInfo_iso_uri=env->GetFieldID(cls_MetaInfo,"iso_uri","Ljava/lang/String;");
    jfieldID fid_MetaInfo_name=env->GetFieldID(cls_MetaInfo,"name","Ljava/lang/String;");
    jfieldID fid_MetaInfo_serial=env->GetFieldID(cls_MetaInfo,"serial","Ljava/lang/String;");
    jfieldID fid_MetaInfo_category=env->GetFieldID(cls_MetaInfo,"category","Ljava/lang/String;");
    jfieldID fid_MetaInfo_version=env->GetFieldID(cls_MetaInfo,"version","Ljava/lang/String;");
    jfieldID fid_MetaInfo_icon=env->GetFieldID(cls_MetaInfo,"icon","[B");
    jfieldID fid_MetaInfo_decrypt=env->GetFieldID(cls_MetaInfo,"decrypt","Z");
    jfieldID  fid_MetaInfo_resolution=env->GetFieldID(cls_MetaInfo,"resolution","I");
    jfieldID  fid_MetaInfo_sound_format=env->GetFieldID(cls_MetaInfo,"sound_format","I");

    iso_archive iso(fd,dec_key_fd);

    if(!iso.exists("PS3_GAME/USRDIR/EBOOT.BIN")) {
        LOGW("EBOOT.BIN not found");
        return NULL;
    }

    psf::registry psf=iso.open_psf("PS3_GAME/PARAM.SFO");

    jobject meta_info=env->NewObject(cls_MetaInfo,mid_MetaInfo_ctor);
    env->SetObjectField(meta_info,fid_MetaInfo_iso_uri,jiso_uri_path);
    env->SetObjectField(meta_info,fid_MetaInfo_name,make_jstring(env,psf::get_string(psf,"TITLE","")));
    env->SetObjectField(meta_info,fid_MetaInfo_serial,make_jstring(env,psf::get_string(psf,"TITLE_ID","")));
    env->SetObjectField(meta_info,fid_MetaInfo_category,make_jstring(env,psf::get_string(psf,"CATEGORY","??")));
    env->SetObjectField(meta_info,fid_MetaInfo_version,make_jstring(env,psf::get_string(psf,"APP_VER","")));
    env->SetIntField(meta_info,fid_MetaInfo_resolution,psf::get_integer(psf,"RESOLUTION",0));
    env->SetIntField(meta_info,fid_MetaInfo_sound_format,psf::get_integer(psf,"SOUND_FORMAT",0));

    if(iso.exists("PS3_GAME/ICON0.PNG")) {

        iso_file icon_file=iso.open("PS3_GAME/ICON0.PNG");
        std::vector<u8> icon_data(icon_file.size());
        icon_file.read(icon_data.data(),icon_data.size());
        jbyteArray icon_array=env->NewByteArray(icon_data.size());
        env->SetByteArrayRegion(icon_array,0,icon_data.size(),reinterpret_cast<const jbyte*>(icon_data.data()));
        env->SetObjectField(meta_info,fid_MetaInfo_icon,icon_array);
    }

    //env->SetBooleanField(meta_info,fid_MetaInfo_decrypt,ae::allow_eboot_decrypt(fs::file(*iso,":PS3_GAME/USRDIR/EBOOT.BIN")));
    env->SetBooleanField(meta_info,fid_MetaInfo_decrypt,true);
    return meta_info;
}
static void j_setup_game_id(JNIEnv* env,jobject self,jstring id){
    const char* _id=env->GetStringUTFChars(id,NULL);
    ae::game_id=_id;
    env->ReleaseStringUTFChars(id,_id);
}
/*public native int get_cpu_core_count();
public native String get_cpu_name(int core_idx);
public native int get_cpu_max_mhz(int core_idx);*/
static jint j_get_cpu_core_count(JNIEnv* env,jobject self){
    return cpu_get_core_count();
}
static jstring j_get_cpu_name(JNIEnv* env,jobject self,jint core_idx){
    const std::string& name=cpu_get_processor_name(core_idx);
    jstring r= env->NewStringUTF(name.c_str());
    return r;
}
static jint j_get_cpu_max_mhz(JNIEnv* env,jobject self,jint core_idx){
    return cpu_get_max_mhz(core_idx);
}

static jstring j_simple_device_info(JNIEnv* env, jobject thiz)
{

    std::string vv;//=get_auther_info();

    vv+=ae::get_cpu_info();
    vv+="\n"+ae::get_gpu_info();

    return env->NewStringUTF(vv.c_str());
}

//public native boolean precompile_ppu_cache(String path);
//public native boolean precompile_ppu_cache(int fd);
static jboolean j_precompile_ppu_cache(JNIEnv* env,jobject self,jstring jpath){
    const char* _path=env->GetStringUTFChars(jpath,NULL);
    const std::string path=_path;
    env->ReleaseStringUTFChars(jpath, _path);
    return ae::precompile_ppu_cache(path,std::nullopt);
}
static jboolean j_precompile_ppu_cache_with_fd(JNIEnv* env,jobject self,jint fd){
    return ae::precompile_ppu_cache(":PS3_GAME/USRDIR/EBOOT.BIN",fd);
}

//public native GameTrophyInfo trophy_info_from_dir(String path);
namespace ae{
    void init();
}
static jobject j_trophy_info_from_dir(JNIEnv* env,jobject self,jstring jreal_path,jstring jvfs_path){
    const char* real_path=env->GetStringUTFChars(jreal_path,NULL);
    const std::string trophy_real_dir = real_path;
    env->ReleaseStringUTFChars(jreal_path, real_path);

    const char* vfs_path=env->GetStringUTFChars(jvfs_path,NULL);
    const std::string trophy_vfs_dir = vfs_path;
    env->ReleaseStringUTFChars(jvfs_path, vfs_path);

    ae::init();

    std::unique_ptr<TROPUSRLoader> trophy_data = std::make_unique<TROPUSRLoader>();

    const std::string tropusr_path = trophy_vfs_dir + "/TROPUSR.DAT";
    const std::string tropconf_path = trophy_vfs_dir + "/TROPCONF.SFM";

    const bool success = trophy_data->Load(tropusr_path, tropconf_path).success;

    if(!success) {
        LOGW("Failed to load TROPUSR.DAT");
        return NULL;
    }

    const u32 trophy_count = trophy_data->GetTrophiesCount();
    if(trophy_count==0){
        return NULL;
    }

    trophy_xml_document trop_config;

    auto read_file=[](const std::string& path)->std::string{
        std::string r;
        std::ifstream f(path);
        if(f.is_open()){
            std::string line;
            while(std::getline(f,line)){
                r+=line;
            }
        }
        return r;
    };

    pugi::xml_parse_result res = trop_config.Read(read_file(trophy_real_dir+"/TROPCONF.SFM"));
    if (!res){
        LOGE("Failed to read trophy xml: %s", tropconf_path.c_str());
        return NULL;
    }

    std::shared_ptr<rXmlNode> trophy_base = trop_config.GetRoot();
    if (!trophy_base)
    {
        LOGE("Failed to read trophy xml (root is null): %s", tropconf_path.c_str());
        return NULL;
    }

    jclass cls_GameTrophyInfo=env->FindClass("aenu/aps3e/Emulator$GameTrophyInfo");
    jmethodID mid_GameTrophyInfo_ctor=env->GetMethodID(cls_GameTrophyInfo,"<init>","()V");
    jfieldID fid_GameTrophyInfo_trophies=env->GetFieldID(cls_GameTrophyInfo,"trophies","[Laenu/aps3e/Emulator$GameTrophyInfo$TrophyInfo;");
    jfieldID fid_GameTrophyInfo_game_name=env->GetFieldID(cls_GameTrophyInfo,"game_name","Ljava/lang/String;");

    jobject result=env->NewObject(cls_GameTrophyInfo,mid_GameTrophyInfo_ctor);
    jstring game_name;

    for (std::shared_ptr<rXmlNode> n = trophy_base->GetChildren(); n; n = n->GetNext())
    {
        if (n->GetName() == "title-name")
        {
            std::string v;
            game_name = env->NewStringUTF((v=n->GetNodeContent()).c_str());
            env->SetObjectField(result,fid_GameTrophyInfo_game_name,game_name);
            break;
        }
    }

    jclass cls_TrophyInfo=env->FindClass("aenu/aps3e/Emulator$GameTrophyInfo$TrophyInfo");
    jmethodID mid_TrophyInfo_ctor=env->GetMethodID(cls_TrophyInfo,"<init>","()V");

    jfieldID fid_TrophyInfo_icon_path=env->GetFieldID(cls_TrophyInfo,"icon_path","Ljava/lang/String;");
    jfieldID fid_TrophyInfo_name=env->GetFieldID(cls_TrophyInfo,"name","Ljava/lang/String;");
    jfieldID fid_TrophyInfo_description=env->GetFieldID(cls_TrophyInfo,"description","Ljava/lang/String;");
    jfieldID fid_TrophyInfo_type=env->GetFieldID(cls_TrophyInfo,"type","B");
    jfieldID fid_TrophyInfo_timestamp=env->GetFieldID(cls_TrophyInfo,"timestamp","J");
    jfieldID fid_TrophyInfo_unlocked=env->GetFieldID(cls_TrophyInfo,"unlocked","Z");
    jobjectArray trophies=env->NewObjectArray(trophy_count,cls_TrophyInfo,NULL);
    env->SetObjectField(result,fid_GameTrophyInfo_trophies,trophies);

    for(u32 trophy_id=0;trophy_id<trophy_count;trophy_id++){
        jobject trophy=env->NewObject(cls_TrophyInfo,mid_TrophyInfo_ctor);
        std::string v;
        env->SetObjectField(trophy,fid_TrophyInfo_icon_path,env->NewStringUTF((v=fmt::format("%s/TROP%03d.PNG", trophy_real_dir,trophy_id)).c_str()));
        env->SetBooleanField(trophy,fid_TrophyInfo_unlocked,(jboolean)trophy_data->GetTrophyUnlockState(trophy_id));
        env->SetLongField(trophy,fid_TrophyInfo_timestamp,trophy_data->GetTrophyTimestamp(trophy_id));

        env->SetObjectArrayElement(trophies,trophy_id,trophy);
    }

    //int i = 0;
    for (std::shared_ptr<rXmlNode> n = trophy_base ? trophy_base->GetChildren() : nullptr; n; n = n->GetNext())
    {
        // Only show trophies.
        if (n->GetName() != "trophy")
        {
            continue;
        }

        // Get data (stolen graciously from sceNpTrophy.cpp)
        SceNpTrophyDetails details{};

        // Get trophy id
        const s32 trophy_id = atoi(n->GetAttribute("id").c_str());
        jobject trophy=env->GetObjectArrayElement(trophies,trophy_id);
        details.trophyId = trophy_id;

        // Get platinum link id (we assume there only exists one platinum trophy per game for now)
        //const s32 platinum_link_id = atoi(n->GetAttribute("pid").c_str());
        //const QString platinum_relevant = platinum_link_id < 0 ? tr("No") : tr("Yes");

        // Get trophy type
        /*QString trophy_type;

        switch (n->GetAttribute("ttype")[0])
        {
            case 'B': details.trophyGrade = SCE_NP_TROPHY_GRADE_BRONZE;   trophy_type = tr("Bronze", "Trophy type");   break;
            case 'S': details.trophyGrade = SCE_NP_TROPHY_GRADE_SILVER;   trophy_type = tr("Silver", "Trophy type");   break;
            case 'G': details.trophyGrade = SCE_NP_TROPHY_GRADE_GOLD;     trophy_type = tr("Gold", "Trophy type");     break;
            case 'P': details.trophyGrade = SCE_NP_TROPHY_GRADE_PLATINUM; trophy_type = tr("Platinum", "Trophy type"); break;
            default: gui_log.warning("Unknown trophy grade %s", n->GetAttribute("ttype")); break;
        }*/

        // Get hidden state
        const bool hidden = n->GetAttribute("hidden")[0] == 'y';
        details.hidden = hidden;

        // Get name and detail
        for (std::shared_ptr<rXmlNode> n2 = n->GetChildren(); n2; n2 = n2->GetNext())
        {
            const std::string name = n2->GetName();
            if (name == "name")
            {
                strcpy_trunc(details.name, n2->GetNodeContent());
            }
            else if (name == "detail")
            {
                strcpy_trunc(details.description, n2->GetNodeContent());
            }
        }

        env->SetObjectField(trophy,fid_TrophyInfo_name,env->NewStringUTF(details.name));
        env->SetObjectField(trophy,fid_TrophyInfo_description,env->NewStringUTF(details.description));

        switch(details.trophyGrade){
            case SCE_NP_TROPHY_GRADE_BRONZE:
                env->SetByteField(trophy,fid_TrophyInfo_type,'B');
                break;
            case SCE_NP_TROPHY_GRADE_SILVER:
                env->SetByteField(trophy,fid_TrophyInfo_type,'S');
                break;
            case SCE_NP_TROPHY_GRADE_GOLD:
                env->SetByteField(trophy,fid_TrophyInfo_type,'G');
                break;
            case SCE_NP_TROPHY_GRADE_PLATINUM:
                env->SetByteField(trophy,fid_TrophyInfo_type,'P');
                break;
        }

        // Get timestamp
        /*const u64 tick = data->trop_usr->GetTrophyTimestamp(trophy_id);
        const QString datetime = tick ? gui::utils::format_datetime(TickToDateTime(tick), gui::persistent::last_played_date_with_time_of_day_format) : tr("Unknown");

        const QString unlockstate = data->trop_usr->GetTrophyUnlockState(trophy_id) ? tr("Earned") : tr("Not Earned");

        custom_table_widget_item* icon_item = new custom_table_widget_item();
        icon_item->setData(Qt::UserRole, hidden, true);
        icon_item->setData(Qt::DecorationRole, placeholder);

        custom_table_widget_item* type_item = new custom_table_widget_item(trophy_type);
        type_item->setData(Qt::UserRole, static_cast<uint>(details.trophyGrade), true);

        m_trophy_table->setItem(i, static_cast<int>(gui::trophy_list_columns::icon), icon_item);
        m_trophy_table->setItem(i, static_cast<int>(gui::trophy_list_columns::name), new custom_table_widget_item(QString::fromStdString(details.name)));
        m_trophy_table->setItem(i, static_cast<int>(gui::trophy_list_columns::description), new custom_table_widget_item(QString::fromStdString(details.description)));
        m_trophy_table->setItem(i, static_cast<int>(gui::trophy_list_columns::type), type_item);
        m_trophy_table->setItem(i, static_cast<int>(gui::trophy_list_columns::is_unlocked), new custom_table_widget_item(unlockstate));
        m_trophy_table->setItem(i, static_cast<int>(gui::trophy_list_columns::id), new custom_table_widget_item(QString::number(trophy_id), Qt::UserRole, trophy_id));
        m_trophy_table->setItem(i, static_cast<int>(gui::trophy_list_columns::platinum_link), new custom_table_widget_item(platinum_relevant, Qt::UserRole, platinum_link_id));
        m_trophy_table->setItem(i, static_cast<int>(gui::trophy_list_columns::time_unlocked), new custom_table_widget_item(datetime, Qt::UserRole, QVariant::fromValue<qulonglong>(tick)));

        ++i;*/
    }

    return result;
}

static jobjectArray j_search_memory(JNIEnv* env,jobject self,jobject search_info){

    jclass CheatInfo=env->GetObjectClass(search_info);
    jmethodID mth_constructor=env->GetMethodID(CheatInfo,"<init>","()V");
    jfieldID fid_CheatInfo_addr=env->GetFieldID(CheatInfo,"addr","J");
    jfieldID fid_CheatInfo_value=env->GetFieldID(CheatInfo,"value","J");
    jfieldID fid_CheatInfo_type=env->GetFieldID(CheatInfo,"type","I");
    jint type=env->GetIntField(search_info,fid_CheatInfo_type);
    jlong search_value=env->GetLongField(search_info,fid_CheatInfo_value);

    std::vector<u32> search_result;
    switch (type) {
        case 1://TYPE_U8
        {
            search_result=ae::mem_search<u8>(static_cast<u8>(search_value));
        }
        break;
        case 2://TYPE_U16
        {
            search_result=ae::mem_search<u16>(static_cast<u16>(search_value));
        }
        break;
        case 3://TYPE_U32
        {
            search_result=ae::mem_search<u32>(static_cast<u32>(search_value));
        }
        break;
        case 4://TYPE_U64
        {
            search_result=ae::mem_search<u64>(static_cast<u64>(search_value));
        }
        break;
    }

    if(search_result.empty()) return nullptr;

    jobjectArray result=env->NewObjectArray(search_result.size(),CheatInfo,nullptr);
    for(size_t i=0;i<search_result.size();++i){
        jobject obj=env->NewObject(CheatInfo,mth_constructor);
        env->SetLongField(obj,fid_CheatInfo_addr,search_result[i]);
        env->SetLongField(obj,fid_CheatInfo_value,search_value);
        env->SetIntField(obj,fid_CheatInfo_type,type);
        env->SetObjectArrayElement(result,i,obj);
    }
    return result;
}

static void j_set_cheat(JNIEnv* env,jobject self,jobject cheat_info){
    jclass CheatInfo=env->GetObjectClass(cheat_info);
    jfieldID fid_CheatInfo_addr=env->GetFieldID(CheatInfo,"addr","J");
    jfieldID fid_CheatInfo_value=env->GetFieldID(CheatInfo,"value","J");
    jfieldID fid_CheatInfo_type=env->GetFieldID(CheatInfo,"type","I");
    jlong addr=env->GetLongField(cheat_info,fid_CheatInfo_addr);
    jlong value=env->GetLongField(cheat_info,fid_CheatInfo_value);
    jint type=env->GetIntField(cheat_info,fid_CheatInfo_type);
    switch (type) {
        case 1://TYPE_U8
        {
            ae::mem_set_value<u8>(static_cast<u32>(addr),static_cast<u8>(value));
        }
        break;
        case 2://TYPE_U16
        {
            ae::mem_set_value<u16>(static_cast<u32>(addr),static_cast<u16>(value));
        }
        break;
        case 3://TYPE_U32
        {
            ae::mem_set_value<u32>(static_cast<u32>(addr),static_cast<u32>(value));
        }
        break;
        case 4://TYPE_U64
        {
            ae::mem_set_value<u64>(static_cast<u32>(addr),static_cast<u64>(value));
        }
        break;
    }
}

static void j_get_cheat(JNIEnv* env,jobject self,jobject cheat_info){
    jclass CheatInfo=env->GetObjectClass(cheat_info);
    jfieldID fid_CheatInfo_addr=env->GetFieldID(CheatInfo,"addr","J");
    jfieldID fid_CheatInfo_value=env->GetFieldID(CheatInfo,"value","J");
    jfieldID fid_CheatInfo_type=env->GetFieldID(CheatInfo,"type","I");
    jlong addr=env->GetLongField(cheat_info,fid_CheatInfo_addr);
    jint type=env->GetIntField(cheat_info,fid_CheatInfo_type);

    bool success;
    switch (type) {
        case 1://TYPE_U8
        {
            env->SetLongField(cheat_info,fid_CheatInfo_value,ae::mem_get_value<u8>(static_cast<u32>(addr), success));
        }
            break;
        case 2://TYPE_U16
        {
            env->SetLongField(cheat_info,fid_CheatInfo_value,ae::mem_get_value<u16>(static_cast<u32>(addr), success));
        }
            break;
        case 3://TYPE_U32
        {
            env->SetLongField(cheat_info,fid_CheatInfo_value,ae::mem_get_value<u32>(static_cast<u32>(addr), success));
        }
            break;
        case 4://TYPE_U64
        {
            env->SetLongField(cheat_info,fid_CheatInfo_value,ae::mem_get_value<u64>(static_cast<u32>(addr), success));
        }
            break;
    }
}

//public native void camera_frame(byte[] data, int width, int height, int format);
static void j_camera_frame(JNIEnv* env,jobject self,jbyteArray data,jint width,jint height,jint format){

}

static auto gen_key=[](const std::string& name)->std::string{
    std::string k=name;
    if(size_t p=k.find("(");p!=std::string::npos){
        k=k.substr(0,p);
    }

    //replace(' ','_');
    //replace('/','_')
    for(auto& c:k){
        if(c==' '){
            c='_';
        }
        else if(c=='/'){
            c='_';
        }
        else if(c=='|'){
            c='_';
        }

    }

    //replace("-","");
    size_t p=k.find("-");
    while(p!=std::string::npos){
        k.erase(p,1);
        p=k.find("-");
    }

    //移除尾部的_
    while (k.at(k.size()-1)=='_'){
        k.erase(k.size()-1,1);
    }

    std::transform(k.begin(),k.end(),k.begin(),[](char c){
        return std::tolower(c);
    });

    return k;
};

static const std::string gen_skips[]={
        "Core|SPU LLVM Lower Bound",
        "Core|SPU LLVM Upper Bound",
        "Audio|Audio Device",
        "Audio|Microphone Devices",
        "Input/Output|Camera ID",
        "Input/Output|Emulated Midi devices",
        "System|System Name",
        "System|PSID high",
        "System|PSID low",
        "System|HDD Model Name",
        "System|HDD Serial Number",
        "Miscellaneous|GDB Server",
        "Miscellaneous|Window Title Format",

        "Video|Force Convert Texture"

        "Video|Performance Overlay|Font",
        "Video|Performance Overlay|Body Color (hex)",
        "Video|Performance Overlay|Body Background (hex)",
        "Video|Performance Overlay|Title Color (hex)",
        "Video|Performance Overlay|Title Background (hex)",
};

static bool gen_is_parent(const std::string& parent_name){
    if(parent_name=="Core"||parent_name=="Video"||parent_name=="Audio"||parent_name=="Input/Output"
       ||parent_name=="System"||parent_name=="Savestate"||parent_name=="Miscellaneous")
        return true;
    return false;
}

#define SEEKBAR_PREF_TAG "aenu.preference.SeekBarPreference"
#define CHECKBOX_PREF_TAG "aenu.preference.CheckBoxPreference"
#define LIST_PREF_TAG "aenu.preference.ListPreference"

static jstring generate_config_xml(JNIEnv* env,jobject self){
#if 1
    auto gen_one_preference=[&](const std::string parent_name,cfg::_base* node)->std::string{
        std::stringstream out;

        std::string parent_name_l=gen_key(parent_name);

        const std::string& name=node->get_name();
        const std::string key=gen_key(name);

        switch (node->get_type()) {
            case cfg::type::_bool:
                out<<"<" CHECKBOX_PREF_TAG " app:title=\"@string/emulator_settings_"<<parent_name_l<<"_"<<key<<"\" \n";
                out<<"app:iconSpaceReserved=\"false\" \n";
                out<<"app:key=\""<<parent_name<<"|"<<name<<"\" />\n";
                break;
            case cfg::type::_int:
            case cfg::type::uint:
                out<<"<" SEEKBAR_PREF_TAG " app:title=\"@string/emulator_settings_"<<parent_name_l<<"_"<<key<<"\" \n";
                out<<"app:iconSpaceReserved=\"false\" \n";
                out<<"app:min=\""<<node->get_min()<<"\"\n";
                if(node->get_max()!=-1)
                    out<<"android:max=\""<<node->get_max()<<"\"\n";
                else
                    out<<"android:max=\"0x7fffffff\"\n";
                out<<"app:showSeekBarValue=\"true\"\n";
                out<<"app:key=\""<<parent_name<<"|"<<name<<"\" />\n";
                break;

            case cfg::type::_enum:
                out<<"<" LIST_PREF_TAG " app:title=\"@string/emulator_settings_"<<parent_name_l<<"_"<<key<<"\" \n";
                out<<"app:iconSpaceReserved=\"false\" \n";
                out<<"app:entries=\""<<"@array/"<<parent_name_l<<"_"<<key<<"_entries\"\n";
                out<<"app:entryValues=\""<<"@array/"<<parent_name_l<<"_"<<key<<"_values\"\n";
                out<<"app:key=\""<<parent_name<<"|"<<name<<"\" />\n";
                break;
            default:

                out<<"<PreferenceScreen app:title=\"@string/emulator_settings_"<<parent_name_l<<"_"<<key<<"\"\n";
                out<<"app:iconSpaceReserved=\"false\" \n";
                out<<"app:key=\""<<parent_name<<"|"<<name<<"\" />\n";
                break;
        }
        return out.str();
    };

    auto gen_vk_debug_preferences=[&](const std::string parent_name,cfg::_base* node)->std::string {
        std::stringstream out;

        std::string parent_name_l=gen_key(parent_name);
        const std::string &name = node->get_name();
        const std::string key = gen_key(name);

        if(node->get_type()==cfg::type::node){
            out<<"<PreferenceScreen app:title=\"@string/emulator_settings_"<<parent_name_l<<"_"<<key<<"\" \n";
            out<<"app:iconSpaceReserved=\"false\" \n";
            out<<"app:key=\""<<parent_name<<"|"<<name<<"\" >\n";
            std::string node_key=parent_name+"|"+name;
            for(auto n3:reinterpret_cast<cfg::node*>(node)->get_nodes()) {
                if(n3->get_type()!=cfg::type::_bool){
                    LOGE("unexpected type");
                    continue;
                }
                out<<"<" CHECKBOX_PREF_TAG " app:title=\""<<n3->get_name()<<"\" \n";
                out<<"app:key=\""<<node_key<<"|"<<n3->get_name()<<"\" />\n";
            }
            out<<"</PreferenceScreen>\n";
        }

        return out.str();
    };

    std::ostringstream out;
    out<<R"(
<?xml version="1.0" encoding="utf-8"?>
<PreferenceScreen
    xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:app="http://schemas.android.com/apk/res-auto">
    )";

    for(auto n:g_cfg.get_nodes()){
        const std::string& name=n->get_name();
        if(gen_is_parent(name)){
            out<<" <PreferenceScreen app:title=\"@string/emulator_settings_"<<gen_key(name)<<"\" \n";
            out<<"app:iconSpaceReserved=\"false\" \n";
            out<<"app:key=\""<<name<<"\" >\n";

            for(auto n2:reinterpret_cast<cfg::node*>(n)->get_nodes()){

                if(std::find(std::begin(gen_skips),std::end(gen_skips),name+"|"+n2->get_name())!=std::end(gen_skips))
                    continue;

                if(n2->get_type()==cfg::type::node){
                    out<<"<PreferenceScreen app:title=\"@string/emulator_settings_"<<gen_key(name)<<"_"<<gen_key(n2->get_name())<<"\" \n";
                    out<<"app:iconSpaceReserved=\"false\" \n";
                    out<<"app:key=\""<<name+"|"+n2->get_name()<<"\" >\n";
                    if((name+"|"+n2->get_name())=="Core|Thread Affinity Mask"){
                        out<<"</PreferenceScreen>\n";
                        continue;
                    }
                    for(auto n3:reinterpret_cast<cfg::node*>(n2)->get_nodes()) {
                        if(std::find(std::begin(gen_skips),std::end(gen_skips),name+"|"+n2->get_name()+"|"+n3->get_name())!=std::end(gen_skips))
                            continue;
                        //
                        if(n3->get_type()==cfg::type::node){
                            out << "\n" <<gen_vk_debug_preferences(name+"|"+n2->get_name(),n3)<< "\n";
                            continue;
                        }

                        out << "\n" << gen_one_preference(name+"|"+n2->get_name(), n3) << "\n";
                    }
                    out<<"</PreferenceScreen>\n";
                }
                else{
                    out<<"\n"<< gen_one_preference(name,n2)<<"\n";
                }
            }
            out<<"</PreferenceScreen>\n";
        }
    }

    out<<"</PreferenceScreen>\n";

    return env->NewStringUTF(out.str().c_str());
#endif
    return env->NewStringUTF("");
}
#undef SEEKBAR_PREF_TAG
#undef CHECKBOX_PREF_TAG
#undef LIST_PREF_TAG

//public native String generate_strings_xml();
static jstring generate_strings_xml(JNIEnv* env,jobject self){

    auto gen_one_string=[&](const std::string parent_name,cfg::_base* node)->std::string{
        std::stringstream out;

        std::string parent_name_l=gen_key(parent_name);

        const std::string& name=node->get_name();
        const std::string key=gen_key(name);

        out<<"<string name=\"emulator_settings_"<<parent_name_l<<"_"<<key<<"\">"<<name<<"</string>\n";
        if(node->get_type()==cfg::type::_enum){
            out<<"<string-array name=\""<<parent_name_l<<"_"<<key<<"_entries\">\n";
            std::vector<std::string> list=node->to_list();
            for(const auto& e:list){
                out<<"<item>"<<e<<"</item>\n";
            }
            out<<"</string-array>\n";
        }

        return out.str();
    };

    std::ostringstream out;

    for(auto n:g_cfg.get_nodes()){
        const std::string& name=n->get_name();
        if(gen_is_parent(name)){

            out<<"<string name=\"emulator_settings_"<<gen_key(name)<<"\">"<<name<<"</string>\n";

            for(auto n2:reinterpret_cast<cfg::node*>(n)->get_nodes()){

                //Video下的3个子项
                if(n2->get_type()==cfg::type::node){

                    out<<"<string name=\"emulator_settings_"<<gen_key(name+"|"+n2->get_name())<<"\">"<<n2->get_name()<<"</string>\n";
                    for(auto n3:reinterpret_cast<cfg::node*>(n2)->get_nodes()) {
                        out << gen_one_string(name+"|"+n2->get_name(), n3);
                    }
                }
                else{
                    out<< gen_one_string(name,n2);
                }
            }
        }
    }

    //array
    for(auto n:g_cfg.get_nodes()){
        const std::string& name=n->get_name();
        if(gen_is_parent(name)){

            for(auto n2:reinterpret_cast<cfg::node*>(n)->get_nodes()){

                switch (n2->get_type()) {
                    case cfg::type::_enum: {

                        out << "<string-array name=\"" << gen_key(name + "|" + n2->get_name())
                            << "_values\">\n";
                        std::vector<std::string> list = n2->to_list();
                        for (const auto &e: list) {
                            out << "<item>" << e << "</item>\n";
                        }
                        out << "</string-array>\n";
                        break;
                    }
                        //Video下的3个子项
                    case cfg::type::node:{
                        for(auto n3:reinterpret_cast<cfg::node*>(n2)->get_nodes()) {
                            out << "<string-array name=\""<<gen_key(name+"|"+n2->get_name()+"|"+n3->get_name())<<"_values\">\n";
                            std::vector<std::string> list=n3->to_list();
                            for(const auto& e:list){
                                out<<"<item>"<<e<<"</item>\n";
                            }
                            out<<"</string-array>\n";
                        }
                        break;
                    }
                    default:
                        break;
                }

            }
        }
    }

    return env->NewStringUTF(out.str().c_str());
}

static jstring generate_java_string_arr(JNIEnv* env,jobject self){

    auto gen_one_key_string=[&](const std::string parent_name,cfg::_base* node,cfg::type test_ty)->std::string{
        std::string r="";
        if(node->get_type()==test_ty){
            return r+"\""+parent_name+"|"+node->get_name()+"\",\n";
        }
        return r;
    };

    auto gen_one_key_array=[&](const std::string prefix,cfg::type test_ty)->std::string{

        std::ostringstream out;
        out<<prefix<<"\n";
        for(auto n:g_cfg.get_nodes()){
            const std::string& name=n->get_name();
            if(gen_is_parent(name)){

                for(auto n2:reinterpret_cast<cfg::node*>(n)->get_nodes()){

                    if(std::find(std::begin(gen_skips),std::end(gen_skips),name+"|"+n2->get_name())!=std::end(gen_skips))
                        continue;


                    if(n2->get_type()!=cfg::type::node) {
                        out<< gen_one_key_string(name,n2,test_ty);
                        continue;
                    }

                    //node
                    for(auto n3:reinterpret_cast<cfg::node*>(n2)->get_nodes()) {
                        if(std::find(std::begin(gen_skips),std::end(gen_skips),name+"|"+n2->get_name()+"|"+n3->get_name())!=std::end(gen_skips))
                            continue;
                        if(n3->get_type()!=cfg::type::node){
                            out << gen_one_key_string(name+"|"+n2->get_name(), n3,test_ty);
                            continue;
                        }

                        for(auto n4:reinterpret_cast<cfg::node*>(n3)->get_nodes()){
                            out << gen_one_key_string(name+"|"+n2->get_name()+"|"+n3->get_name(), n4,test_ty);
                        }
                    }
                }
            }
        }
        out<<"};\n";
        return out.str();
    };

    auto gen_node_key_array=[&]()->std::string{

        std::ostringstream out;
        out<<"final String[] NODE_KEYS={\n";
        for(auto n:g_cfg.get_nodes()){
            const std::string& name=n->get_name();
            if(gen_is_parent(name)){

                out<<"\""<<name<<"\",\n";

                for(auto n2:reinterpret_cast<cfg::node*>(n)->get_nodes()){
                    if(std::find(std::begin(gen_skips),std::end(gen_skips),name+"|"+n2->get_name())!=std::end(gen_skips))
                        continue;

                    if(n2->get_type()==cfg::type::node){
                        out<<"\""<<name+"|"+n2->get_name()<<"\",\n";

                        for(auto n3:reinterpret_cast<cfg::node*>(n2)->get_nodes()) {

                            if(std::find(std::begin(gen_skips),std::end(gen_skips),name+"|"+n2->get_name()+"|"+n3->get_name()))
                                continue;

                            if(n3->get_type()==cfg::type::node){
                                out<<"\""<<name+"|"+n2->get_name()+"|"+n3->get_name()<<"\",\n";
                            }
                        }
                    }
                }
            }
        }
        out<<"};\n";
        return out.str();
    };

    std::ostringstream out;

    out<<gen_one_key_array("final String[] BOOL_KEYS={",cfg::type::_bool);
    out<<gen_one_key_array("final String[] INT_KEYS={",cfg::type::_int);
    out<<gen_one_key_array("final String[] INT_KEYS={",cfg::type::uint);
    out<<gen_one_key_array("final String[] STRING_ARR_KEYS={",cfg::type::_enum);

    //
    out<<gen_node_key_array();

    return env->NewStringUTF(out.str().c_str());
}

static jobjectArray j_get_native_llvm_cpu_list(JNIEnv* env,jobject self){

    const std::vector<core_info_t> core_info_list = cpu_get_core_info();
    const std::set<std::string,std::greater<>> llvm_cpu_list=get_processor_name_set(core_info_list);

    int count=llvm_cpu_list.size();

    jobjectArray ret=env->NewObjectArray(count,env->FindClass("java/lang/String"),nullptr);
    int n=0;
    for(const std::string& cpu_name:llvm_cpu_list){
        env->SetObjectArrayElement(ret,n++,env->NewStringUTF(cpu_name.c_str()));
    }
    return ret;
}

static jobjectArray j_get_support_llvm_cpu_list(JNIEnv* env,jobject self){

    const std::vector<core_info_t> core_info_list = cpu_get_core_info();
    const std::set<std::string,std::greater<>> llvm_cpu_list=get_processor_name_set(core_info_list);
    const std::string isa=cpu_get_processor_isa(core_info_list[0]);
    std::vector<std::string> append_llvm_cpu_list;
    int count=llvm_cpu_list.size();
    if(isa=="armv9-a"||isa=="armv9.2-a"){
        count+=4;
        append_llvm_cpu_list={"cortex-x1","cortex-a55","cortex-a73","cortex-a53"};
    }
    else if(isa=="armv8.2-a"){
        count+=2;
        append_llvm_cpu_list={"cortex-a73","cortex-a53"};
    }

    jobjectArray ret=env->NewObjectArray(count,env->FindClass("java/lang/String"),nullptr);
    int n=0;
    for(const std::string& cpu_name:llvm_cpu_list){
        env->SetObjectArrayElement(ret,n++,env->NewStringUTF(cpu_name.c_str()));
    }
    for(const std::string& cpu_name:append_llvm_cpu_list){
        env->SetObjectArrayElement(ret,n++,env->NewStringUTF(cpu_name.c_str()));
    }
    return ret;
}

static jobjectArray j_get_vulkan_physical_dev_list(JNIEnv* env,jobject self){

    std::vector<VkPhysicalDeviceProperties> physical_device_prop_list;
    {
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
        }

        // 获取物理设备数量
        uint32_t physicalDeviceCount = 0;
        vkEnumeratePhysicalDevices(inst, &physicalDeviceCount, nullptr);

        std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
        vkEnumeratePhysicalDevices(inst, &physicalDeviceCount, physicalDevices.data());

        for (const auto& physicalDevice : physicalDevices) {
            VkPhysicalDeviceProperties physicalDeviceProperties;
            vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
            physical_device_prop_list.push_back(physicalDeviceProperties);
        }

        vkDestroyInstance(inst, nullptr);
    }
    int count=physical_device_prop_list.size();
    jobjectArray ret=env->NewObjectArray(count,env->FindClass("java/lang/String"),nullptr);

    int n=0;
    for(const VkPhysicalDeviceProperties& prop:physical_device_prop_list){
        env->SetObjectArrayElement(ret,n++,env->NewStringUTF(prop.deviceName));
    }
    return ret;
}

/*
static jboolean support_custom_driver(JNIEnv* env,jobject self){
    return access("/dev/kgsl-3d0",F_OK)==0;
}*/

int register_aps3e_Emulator(JNIEnv* env){

    static const JNINativeMethod methods[] = {

            {"get_cpu_core_count", "()I",(void *)j_get_cpu_core_count},
            {"get_cpu_name", "(I)Ljava/lang/String;",(void *)j_get_cpu_name},
            {"get_cpu_max_mhz", "(I)I",(void *)j_get_cpu_max_mhz},
            {"simple_device_info", "()Ljava/lang/String;",(void *)j_simple_device_info},
            {"get_native_llvm_cpu_list", "()[Ljava/lang/String;",(void *)j_get_native_llvm_cpu_list},
            {"get_support_llvm_cpu_list", "()[Ljava/lang/String;",(void *)j_get_support_llvm_cpu_list},
            {"get_vulkan_physical_dev_list", "()[Ljava/lang/String;",(void *)j_get_vulkan_physical_dev_list},

            //{"support_custom_driver","()Z",(void *)support_custom_driver},

            { "generate_config_xml", "()Ljava/lang/String;", (void *) generate_config_xml },
            {"generate_strings_xml","()Ljava/lang/String;", (void *)generate_strings_xml},
            {"generate_java_string_arr","()Ljava/lang/String;", (void *)generate_java_string_arr},

            { "install_firmware", "(I)Z", (void *) j_install_firmware },
            //{ "meta_info_from_iso","(Ljava/lang/String;)Laenu/aps3e/Emulator$MetaInfo;",(void*)MetaInfo_from_iso},
            { "meta_info_from_dir","(Ljava/lang/String;)Laenu/aps3e/Emulator$MetaInfo;",(void*)j_meta_info_from_dir},

            { "meta_info_from_iso","(IILjava/lang/String;)Laenu/aps3e/Emulator$MetaInfo;",(void*)j_meta_info_from_iso},

            { "setup_game_id", "(Ljava/lang/String;)V", (void *) j_setup_game_id },
            {"install_edat", "(I)Z", (void *) j_install_edat},
            { "install_pkg", "(I)Z", (void *) j_install_pkg },
            {"precompile_ppu_cache", "(Ljava/lang/String;)Z", (void *) j_precompile_ppu_cache},

            {"precompile_ppu_cache", "(I)Z", (void *) j_precompile_ppu_cache_with_fd},
            { "trophy_info_from_dir", "(Ljava/lang/String;Ljava/lang/String;)Laenu/aps3e/Emulator$GameTrophyInfo;", (void *) j_trophy_info_from_dir},
            { "search_memory", "(Laenu/aps3e/Emulator$CheatInfo;)[Laenu/aps3e/Emulator$CheatInfo;", (void *) j_search_memory},
            { "set_cheat", "(Laenu/aps3e/Emulator$CheatInfo;)V", (void *) j_set_cheat},
            { "get_cheat", "(Laenu/aps3e/Emulator$CheatInfo;)V", (void *) j_get_cheat},

            {"camera_frame", "([BIII)V", (void *) j_camera_frame},
    };

    jclass clazz = env->FindClass("aenu/aps3e/Emulator");
    return env->RegisterNatives(clazz,methods, sizeof(methods)/sizeof(methods[0]));

}