// SPDX-License-Identifier: WTFPL

#include "emulator.h"
#include <android/log.h>
#include <fstream>
#include <jni.h>
#include <thread>

#define LOG_TAG "Emulator_Config"
#define LOGE(...) {      \
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,"%s : %d",__FILE__,__LINE__);\
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__);\
}

#define CFG_TYPE_TOML 0
#define CFG_TYPE_YAML 1

#if CFG_TYPE_YAML
#include "yaml-cpp/yaml.h"
#include <sstream>
#include <iostream>
#include <fstream>

static_assert(sizeof(YAML::Node*)==8,"");

static YAML::Node* j_open_config_str(JNIEnv* env,jobject self,jstring jconfig_str){
    jboolean is_copy=false;
    const char* config_str=env->GetStringUTFChars(jconfig_str,&is_copy);
    std::istringstream config_stream(config_str);
    YAML::Node* config_node = new YAML::Node(YAML::Load(config_stream));
    env->ReleaseStringUTFChars(jconfig_str,config_str);
    return config_node;
}

static jstring j_close_config_str(JNIEnv* env,jobject self,YAML::Node* config_node){
    YAML::Emitter out;
    out << *config_node;
    jboolean is_copy=false;

    jstring result=env->NewStringUTF(out.c_str());
    delete config_node;
    return result;
}

static YAML::Node* j_open_config_file(JNIEnv* env,jobject self,jstring config_path){
    jboolean is_copy=false;
    const char* path=env->GetStringUTFChars(config_path,&is_copy);

    YAML::Node* config_node = new YAML::Node(YAML::LoadFile(path));
    env->ReleaseStringUTFChars(config_path,path);
    return config_node;
}

static jstring j_load_config_entry(JNIEnv* env,jobject self,YAML::Node* config_node,jstring tag){
    jboolean is_copy=false;
    const char* tag_cstr=env->GetStringUTFChars(tag,&is_copy);
    std::string  tag_str(tag_cstr);
    env->ReleaseStringUTFChars(tag,tag_cstr);
    size_t pos=tag_str.find('|');
    std::string  parent=tag_str.substr(0,pos);
    std::string  child=tag_str.substr(pos+1);

    switch(std::count(child.begin(),child.end(),'|')){
        case 0: {
            YAML::Node node=(*config_node)[parent][child];
            if(node.IsDefined())
                return env->NewStringUTF(node.as<std::string>().c_str());
        }
            break;
        case 1:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);

            YAML::Node node=(*config_node)[parent][child][child2];
            if(node.IsDefined())
                return env->NewStringUTF(node.as<std::string>().c_str());
        }
            break;

        case 2:{

            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);

            YAML::Node node=(*config_node)[parent][child][child2][child3];
            if(node.IsDefined())
                return env->NewStringUTF(node.as<std::string>().c_str());
        }
            break;

        case 3:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0,pos);
            YAML::Node node=(*config_node)[parent][child][child2][child3][child4];
            if(node.IsDefined())
                return env->NewStringUTF(node.as<std::string>().c_str());
        }
            break;

        case 4:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            YAML::Node node=(*config_node)[parent][child][child2][child3][child4][child5];
            if(node.IsDefined())
                return env->NewStringUTF(node.as<std::string>().c_str());
        }
            break;

        case 5:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            pos=child5.find('|');
            std::string child6=child5.substr(pos+1);
            child5=child5.substr(0,pos);
            YAML::Node node=(*config_node)[parent][child][child2][child3][child4][child5][child6];
            if(node.IsDefined())
                return env->NewStringUTF(node.as<std::string>().c_str());
        }
        break;

        case 6:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            pos=child5.find('|');
            std::string child6=child5.substr(pos+1);
            child5=child5.substr(0,pos);
            pos=child6.find('|');
            std::string child7=child6.substr(pos+1);
            child6=child6.substr(0,pos);
            YAML::Node node=(*config_node)[parent][child][child2][child3][child4][child5][child6][child7];
            if(node.IsDefined())
                return env->NewStringUTF(node.as<std::string>().c_str());
        }
        break;

        case 7:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            pos=child5.find('|');
            std::string child6=child5.substr(pos+1);
            child5=child5.substr(0,pos);
            pos=child6.find('|');
            std::string child7=child6.substr(pos+1);
            child6=child6.substr(0,pos);
            std::string child8=child7.substr(pos+1);
            child7=child7.substr(0,pos);
            YAML::Node node=(*config_node)[parent][child][child2][child3][child4][child5][child6][child7][child8];
            if(node.IsDefined())
                return env->NewStringUTF(node.as<std::string>().c_str());
        }
        break;

        default:
            LOGE("load_config_entry fail %s,level too deep",tag_str.c_str());
            break;
    }
    return NULL;
}


static jobjectArray j_load_config_entry_ty_arr(JNIEnv* env,jobject self,YAML::Node* config_node,jstring tag){
    jboolean is_copy=false;
    const char* tag_cstr=env->GetStringUTFChars(tag,&is_copy);
    std::string  tag_str(tag_cstr);
    env->ReleaseStringUTFChars(tag,tag_cstr);
    size_t pos=tag_str.find('|');
    std::string  parent=tag_str.substr(0,pos);
    std::string  child=tag_str.substr(pos+1);

    switch(std::count(child.begin(),child.end(),'|')){
        case 0: {
            YAML::Node node=(*config_node)[parent][child];
            if(node.IsDefined()) {
                std::vector<std::string> v=node.as<std::vector<std::string>>();
                jobjectArray arr=env->NewObjectArray(v.size(),env->FindClass("java/lang/String"),NULL);
                for(size_t i=0;i<v.size();i++){
                    env->SetObjectArrayElement(arr,i,env->NewStringUTF(v[i].c_str()));
                }
                return arr;
            }
        }
            break;
        case 1:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);

            YAML::Node node=(*config_node)[parent][child][child2];
            if(node.IsDefined()) {
                std::vector<std::string> v=node.as<std::vector<std::string>>();
                jobjectArray arr=env->NewObjectArray(v.size(),env->FindClass("java/lang/String"),NULL);
                for(size_t i=0;i<v.size();i++){
                    env->SetObjectArrayElement(arr,i,env->NewStringUTF(v[i].c_str()));
                }
                return arr;
            }
        }
            break;

        case 2:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);

            YAML::Node node=(*config_node)[parent][child][child2][child3];
            if(node.IsDefined()) {
                std::vector<std::string> v=node.as<std::vector<std::string>>();
                jobjectArray arr=env->NewObjectArray(v.size(),env->FindClass("java/lang/String"),NULL);
                for(size_t i=0;i<v.size();i++){
                    env->SetObjectArrayElement(arr,i,env->NewStringUTF(v[i].c_str()));
                }
                return arr;
            }
        }
            break;
        
        case 3:{ 
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0,pos);

            YAML::Node node=(*config_node)[parent][child][child2][child3][child4];
            if(node.IsDefined()) {
                std::vector<std::string> v=node.as<std::vector<std::string>>();
                jobjectArray arr=env->NewObjectArray(v.size(),env->FindClass("java/lang/String"),NULL);
                for(size_t i=0;i<v.size();i++){
                    env->SetObjectArrayElement(arr,i,env->NewStringUTF(v[i].c_str()));
                }
                return arr;
            }
        }
            break;
        
        case 4:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            YAML::Node node=(*config_node)[parent][child][child2][child3][child4][child5];
            if(node.IsDefined()) {
                std::vector<std::string> v=node.as<std::vector<std::string>>();
                jobjectArray arr=env->NewObjectArray(v.size(),env->FindClass("java/lang/String"),NULL);
                for(size_t i=0;i<v.size();i++){
                    env->SetObjectArrayElement(arr,i,env->NewStringUTF(v[i].c_str()));
                }
                return arr;
            }
        }
            break;
        
        case 5:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            pos=child5.find('|');
            std::string child6=child5.substr(pos+1);
            child5=child5.substr(0,pos);
            YAML::Node node=(*config_node)[parent][child][child2][child3][child4][child5][child6];
            if(node.IsDefined()) {
                std::vector<std::string> v=node.as<std::vector<std::string>>();
                jobjectArray arr=env->NewObjectArray(v.size(),env->FindClass("java/lang/String"),NULL);
                for(size_t i=0;i<v.size();i++){
                    env->SetObjectArrayElement(arr,i,env->NewStringUTF(v[i].c_str()));
                }
                return arr;
            }
        }
        break;
        
        case 6:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            pos=child5.find('|');
            std::string child6=child5.substr(pos+1);
            child5=child5.substr(0,pos);
            pos=child6.find('|');
            std::string child7=child6.substr(pos+1);
            child6=child6.substr(0,pos);
            YAML::Node node=(*config_node)[parent][child][child2][child3][child4][child5][child6][child7];
            if(node.IsDefined()) {
                std::vector<std::string> v=node.as<std::vector<std::string>>();
                jobjectArray arr=env->NewObjectArray(v.size(),env->FindClass("java/lang/String"),NULL);
                for(size_t i=0;i<v.size();i++){
                    env->SetObjectArrayElement(arr,i,env->NewStringUTF(v[i].c_str()));
                }
                return arr;
            }
        }
        break;
        
        case 7:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            pos=child5.find('|');
            std::string child6=child5.substr(pos+1);
            child5=child5.substr(0,pos);
            pos=child6.find('|');
            std::string child7=child6.substr(pos+1);
            child6=child6.substr(0,pos);
            std::string child8=child7.substr(pos+1);
            child7=child7.substr(0,pos);
            YAML::Node node=(*config_node)[parent][child][child2][child3][child4][child5][child6][child7][child8];
            if(node.IsDefined()) {
                std::vector<std::string> v=node.as<std::vector<std::string>>();
                jobjectArray arr=env->NewObjectArray(v.size(),env->FindClass("java/lang/String"),NULL);
                for(size_t i=0;i<v.size();i++){
                    env->SetObjectArrayElement(arr,i,env->NewStringUTF(v[i].c_str()));
                }
                return arr;
            }
        }
        break;

        default:
            LOGE("load_config_entry fail %s,level too deep",tag_str.c_str());
            break;
    }
    return NULL;
}

static void j_save_config_entry(JNIEnv* env,jobject self,YAML::Node* config_node,jstring tag,jstring val){

    jboolean is_copy=false;
    const char* tag_cstr=env->GetStringUTFChars(tag,&is_copy);
    std::string  tag_str(tag_cstr);
    env->ReleaseStringUTFChars(tag,tag_cstr);
    size_t pos=tag_str.find('|');
    std::string  parent=tag_str.substr(0,pos);
    std::string  child=tag_str.substr(pos+1);
    const char* val_cstr=env->GetStringUTFChars(val,&is_copy);

    switch(std::count(child.begin(),child.end(),'|')){
        case 0: {
            (*config_node)[parent][child] = std::string(val_cstr);
        }
            break;
        case 1:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            (*config_node)[parent][child][child2]=std::string(val_cstr);
        }
            break;

        case 2:{

            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            (*config_node)[parent][child][child2][child3]=std::string(val_cstr);
        }
            break;
        
        case 3:{ 
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0,pos);
            (*config_node)[parent][child][child2][child3][child4]=std::string(val_cstr);
        }
            break;
        
        case 4:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            (*config_node)[parent][child][child2][child3][child4][child5]=std::string(val_cstr);
        }
            break;
        
        case 5:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            pos=child5.find('|');
            std::string child6=child5.substr(pos+1);
            child5=child5.substr(0,pos);
            (*config_node)[parent][child][child2][child3][child4][child5][child6]=std::string(val_cstr);
        }
        break;
        
        case 6:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            pos=child5.find('|');
            std::string child6=child5.substr(pos+1);
            child5=child5.substr(0,pos);
            pos=child6.find('|');
            std::string child7=child6.substr(pos+1);
            child6=child6.substr(0,pos);
            (*config_node)[parent][child][child2][child3][child4][child5][child6][child7]=std::string(val_cstr);
        }
        break;
        
        case 7:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            pos=child5.find('|');
            std::string child6=child5.substr(pos+1);
            child5=child5.substr(0,pos);
            pos=child6.find('|');
            std::string child7=child6.substr(pos+1);
            child6=child6.substr(0,pos);
            std::string child8=child7.substr(pos+1);
            child7=child7.substr(0,pos);
            (*config_node)[parent][child][child2][child3][child4][child5][child6][child7][child8]=std::string(val_cstr);
        }
        break;

        default:
            LOGE("save_config_entry fail %s,level too deep",tag_str.c_str());
            break;
    }

    env->ReleaseStringUTFChars(val,val_cstr);
}


static void j_save_config_entry_ty_arr(JNIEnv* env,jobject self,YAML::Node* config_node,jstring tag,jobjectArray val) {

    jboolean is_copy = false;
    const char *tag_cstr = env->GetStringUTFChars(tag, &is_copy);
    std::string tag_str(tag_cstr);
    env->ReleaseStringUTFChars(tag, tag_cstr);
    size_t pos = tag_str.find('|');
    std::string parent = tag_str.substr(0, pos);
    std::string child = tag_str.substr(pos + 1);

    switch (std::count(child.begin(), child.end(), '|')) {
        case 0: {
            std::vector<std::string> v;
            for (int i = 0; i < env->GetArrayLength(val); i++) {
                jstring jstr = (jstring) env->GetObjectArrayElement(val, i);
                const char *str = env->GetStringUTFChars(jstr, &is_copy);
                v.push_back(std::string(str));
                env->ReleaseStringUTFChars(jstr, str);
            }
            (*config_node)[parent][child] = v;
        }
            break;
        case 1:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            std::vector<std::string> v;
            for (int i = 0; i < env->GetArrayLength(val); i++) {
                jstring jstr = (jstring) env->GetObjectArrayElement(val, i);
                const char *str = env->GetStringUTFChars(jstr, &is_copy);
                v.push_back(std::string(str));
                env->ReleaseStringUTFChars(jstr, str);
            }
            (*config_node)[parent][child][child2] = v;
        }
            break;

        case 2:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            std::vector<std::string> v;
            for (int i = 0; i < env->GetArrayLength(val); i++) {
                jstring jstr = (jstring) env->GetObjectArrayElement(val, i);
                const char *str = env->GetStringUTFChars(jstr, &is_copy);
                v.push_back(std::string(str));
                env->ReleaseStringUTFChars(jstr, str);
            }
            (*config_node)[parent][child][child2][child3] = v;
        }
            break;
        
        case 3:{ 
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0,pos);
            std::vector<std::string> v;
            for (int i = 0; i < env->GetArrayLength(val); i++) {
                jstring jstr = (jstring) env->GetObjectArrayElement(val, i);
                const char *str = env->GetStringUTFChars(jstr, &is_copy);
                v.push_back(std::string(str));
                env->ReleaseStringUTFChars(jstr, str);
            }
            (*config_node)[parent][child][child2][child3][child4] = v;
        }
            break;
        
        case 4:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            std::vector<std::string> v;
            for (int i = 0; i < env->GetArrayLength(val); i++) {
                jstring jstr = (jstring) env->GetObjectArrayElement(val, i);
                const char *str = env->GetStringUTFChars(jstr, &is_copy);
                v.push_back(std::string(str));
                env->ReleaseStringUTFChars(jstr, str);
            }
            (*config_node)[parent][child][child2][child3][child4][child5] = v;
        }
            break;
        
        case 5:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            pos=child5.find('|');
            std::string child6=child5.substr(pos+1);
            child5=child5.substr(0,pos);
            std::vector<std::string> v;
            for (int i = 0; i < env->GetArrayLength(val); i++) {
                jstring jstr = (jstring) env->GetObjectArrayElement(val, i);
                const char *str = env->GetStringUTFChars(jstr, &is_copy);
                v.push_back(std::string(str));
                env->ReleaseStringUTFChars(jstr, str);
            }
            (*config_node)[parent][child][child2][child3][child4][child5][child6] = v;
        }
        break;
        
        case 6:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            pos=child5.find('|');
            std::string child6=child5.substr(pos+1);
            child5=child5.substr(0,pos);
            pos=child6.find('|');
            std::string child7=child6.substr(pos+1);
            child6=child6.substr(0,pos);
            std::vector<std::string> v;
            for (int i = 0; i < env->GetArrayLength(val); i++) {
                jstring jstr = (jstring) env->GetObjectArrayElement(val, i);
                const char *str = env->GetStringUTFChars(jstr, &is_copy);
                v.push_back(std::string(str));
                env->ReleaseStringUTFChars(jstr, str);
            }
            (*config_node)[parent][child][child2][child3][child4][child5][child6][child7] = v;
        }
        break;
        
        case 7:{
            pos=child.find('|');
            std::string child2=child.substr(pos+1);
            child=child.substr(0,pos);
            pos=child2.find('|');
            std::string child3=child2.substr(pos+1);
            child2=child2.substr(0,pos);
            pos=child3.find('|');
            std::string child4=child3.substr(pos+1);
            child3=child3.substr(0, pos);
            pos=child4.find('|');
            std::string child5=child4.substr(pos+1);
            child4=child4.substr(0,pos);
            pos=child5.find('|');
            std::string child6=child5.substr(pos+1);
            child5=child5.substr(0,pos);
            pos=child6.find('|');
            std::string child7=child6.substr(pos+1);
            child6=child6.substr(0,pos);
            std::string child8=child7.substr(pos+1);
            child7=child7.substr(0,pos);
            std::vector<std::string> v;
            for (int i = 0; i < env->GetArrayLength(val); i++) {
                jstring jstr = (jstring) env->GetObjectArrayElement(val, i);
                const char *str = env->GetStringUTFChars(jstr, &is_copy);
                v.push_back(std::string(str));
                env->ReleaseStringUTFChars(jstr, str);
            }
            (*config_node)[parent][child][child2][child3][child4][child5][child6][child7][child8] = v;
        }
        break;

        default:
        LOGE("save_config_entry fail %s,level too deep",tag_str.c_str());
            break;
    }
}
static void j_close_config_file(JNIEnv* env,jobject self,YAML::Node* config_node,jstring config_path){
    YAML::Emitter out;
    out << *config_node;
    jboolean is_copy=false;
    const char* path=env->GetStringUTFChars(config_path,&is_copy);
    std::ofstream fout(path);
    fout << out.c_str();
    fout.close();
    env->ReleaseStringUTFChars(config_path,path);
    delete config_node;
}

static jobjectArray j_list_children(JNIEnv* env, jobject self, YAML::Node* config_node, jstring tag) {

    YAML::Node node;
    if (tag==NULL) {
        node = *config_node;
    } else {

        jboolean is_copy = false;
        const char* tag_cstr = env->GetStringUTFChars(tag, &is_copy);
        std::string tag_str(tag_cstr);
        env->ReleaseStringUTFChars(tag, tag_cstr);

        if(tag_str.empty()){
            node = *config_node;
        }
        else{

            switch(std::count(tag_str.begin(),tag_str.end(),'|')){
                case 0: {
                    node=(*config_node)[tag_str];
                }
                    break;
                case 1:{
                    std::string child=tag_str;
                    int pos=child.find('|');
                    std::string child2=child.substr(pos+1);
                    child=child.substr(0,pos);

                    node=(*config_node)[child][child2];
                }
                    break;

                case 2:{
                    std::string child=tag_str;
                    int pos=child.find('|');
                    std::string child2=child.substr(pos+1);
                    child=child.substr(0,pos);
                    pos=child2.find('|');
                    std::string child3=child2.substr(pos+1);
                    child2=child2.substr(0,pos);

                    node=(*config_node)[child][child2][child3];
                }
                    break;

                case 3:{
                    std::string child=tag_str;
                    int pos=child.find('|');
                    std::string child2=child.substr(pos+1);
                    child=child.substr(0,pos);
                    pos=child2.find('|');
                    std::string child3=child2.substr(pos+1);
                    child2=child2.substr(0,pos);
                    pos=child3.find('|');
                    std::string child4=child3.substr(pos+1);
                    child3=child3.substr(0,pos);

                    node=(*config_node)[child][child2][child3][child4];

                }
                    break;

                case 4:{
                    std::string child=tag_str;
                    int pos=child.find('|');
                    std::string child2=child.substr(pos+1);
                    child=child.substr(0,pos);
                    pos=child2.find('|');
                    std::string child3=child2.substr(pos+1);
                    child2=child2.substr(0,pos);
                    pos=child3.find('|');
                    std::string child4=child3.substr(pos+1);
                    child3=child3.substr(0, pos);
                    pos=child4.find('|');
                    std::string child5=child4.substr(pos+1);
                    child4=child4.substr(0,pos);
                    node=(*config_node)[child][child2][child3][child4][child5];

                }
                    break;

                case 5:{
                    std::string child=tag_str;
                    int pos=child.find('|');
                    std::string child2=child.substr(pos+1);
                    child=child.substr(0,pos);
                    pos=child2.find('|');
                    std::string child3=child2.substr(pos+1);
                    child2=child2.substr(0,pos);
                    pos=child3.find('|');
                    std::string child4=child3.substr(pos+1);
                    child3=child3.substr(0, pos);
                    pos=child4.find('|');
                    std::string child5=child4.substr(pos+1);
                    child4=child4.substr(0,pos);
                    pos=child5.find('|');
                    std::string child6=child5.substr(pos+1);
                    child5=child5.substr(0,pos);
                    node=(*config_node)[child][child2][child3][child4][child5][child6];
                }
                    break;

                case 6:{
                    std::string child=tag_str;
                    int pos=child.find('|');
                    std::string child2=child.substr(pos+1);
                    child=child.substr(0,pos);
                    pos=child2.find('|');
                    std::string child3=child2.substr(pos+1);
                    child2=child2.substr(0,pos);
                    pos=child3.find('|');
                    std::string child4=child3.substr(pos+1);
                    child3=child3.substr(0, pos);
                    pos=child4.find('|');
                    std::string child5=child4.substr(pos+1);
                    child4=child4.substr(0,pos);
                    pos=child5.find('|');
                    std::string child6=child5.substr(pos+1);
                    child5=child5.substr(0,pos);
                    pos=child6.find('|');
                    std::string child7=child6.substr(pos+1);
                    child6=child6.substr(0,pos);
                    node=(*config_node)[child][child2][child3][child4][child5][child6][child7];
                }
                    break;

                case 7:{
                    std::string child=tag_str;
                    int pos=child.find('|');
                    std::string child2=child.substr(pos+1);
                    child=child.substr(0,pos);
                    pos=child2.find('|');
                    std::string child3=child2.substr(pos+1);
                    child2=child2.substr(0,pos);
                    pos=child3.find('|');
                    std::string child4=child3.substr(pos+1);
                    child3=child3.substr(0, pos);
                    pos=child4.find('|');
                    std::string child5=child4.substr(pos+1);
                    child4=child4.substr(0,pos);
                    pos=child5.find('|');
                    std::string child6=child5.substr(pos+1);
                    child5=child5.substr(0,pos);
                    pos=child6.find('|');
                    std::string child7=child6.substr(pos+1);
                    child6=child6.substr(0,pos);
                    std::string child8=child7.substr(pos+1);
                    child7=child7.substr(0,pos);
                    node=(*config_node)[child][child2][child3][child4][child5][child6][child7][child8];
                }
                    break;

                default:
                LOGE("j_list_children fail %s,level too deep",tag_str.c_str());
                return NULL;
                    break;
            }
        }
    }

    if(!node.IsDefined()) return NULL;
    if (!node.IsMap()) return NULL;

    jclass strClass = env->FindClass("java/lang/String");
    std::vector<std::string> keys;
    for (auto it = node.begin(); it != node.end(); ++it) {
        keys.push_back(it->first.as<std::string>());
    }
    jobjectArray arr = env->NewObjectArray(keys.size(), strClass, NULL);
    for (size_t i = 0; i < keys.size(); i++) {
        env->SetObjectArrayElement(arr, i, env->NewStringUTF(keys[i].c_str()));
    }
    return arr;
}
#elif CFG_TYPE_TOML
#include "cpptoml/include/cpptoml.h"
static std::shared_ptr<cpptoml::table>* j_open_config_str(JNIEnv* env,jobject self,jstring jconfig_str) {
    jboolean is_copy=false;
    const char* config_str=env->GetStringUTFChars(jconfig_str,&is_copy);
    std::istringstream config_stream(config_str);
    cpptoml::parser p{config_stream};
    auto toml=p.parse();
    LOGE("open_config_str %d",toml.use_count());
    env->ReleaseStringUTFChars(jconfig_str,config_str);
    std::unique_ptr<std::shared_ptr<cpptoml::table>> toml_p=std::make_unique<std::shared_ptr<cpptoml::table>>(toml);
    return toml_p.release();
}
static jstring j_close_config_str(JNIEnv* env,jobject self,std::shared_ptr<cpptoml::table>* config_table){
    std::unique_ptr<std::shared_ptr<cpptoml::table>> toml_p;
    toml_p.reset(config_table);
    std::ostringstream out_strm;
    out_strm << *toml_p;
    std::string out=out_strm.str();
    jboolean is_copy=false;
    jstring result=env->NewStringUTF(out.c_str());
    return result;
}
static std::shared_ptr<cpptoml::table>* j_open_config_file(JNIEnv* env,jobject self,jstring config_path){
    jboolean is_copy=false;
    const char* path=env->GetStringUTFChars(config_path,&is_copy);

    std::shared_ptr<cpptoml::table> toml=cpptoml::parse_file(path);
    env->ReleaseStringUTFChars(config_path,path);
    std::unique_ptr<std::shared_ptr<cpptoml::table>> toml_p=std::make_unique<std::shared_ptr<cpptoml::table>>(toml);
    return toml_p.release();
}
static jstring j_load_config_entry(JNIEnv* env,jobject self,std::shared_ptr<cpptoml::table>* config_table,jstring tag){
    jboolean is_copy=false;
    const char* tag_cstr=env->GetStringUTFChars(tag,&is_copy);
    std::string  tag_str(tag_cstr);
    env->ReleaseStringUTFChars(tag,tag_cstr);

    size_t pos=tag_str.find('|');
    std::string  table_name=tag_str.substr(0,pos);
    std::string  key_name=tag_str.substr(pos+1);

    std::shared_ptr<cpptoml::table> table=(*config_table)->get_table(table_name);
    if(!table){
        return NULL;
    }
    if(const auto val=table->get_as<bool>(key_name);val){
        std::string val_str=*val?"true":"false";
        LOGE("load_config_entry Z %s %s",tag_str.c_str(),val_str.c_str());
        jstring result=env->NewStringUTF(val_str.c_str());
        return result;
    }
    else if(const auto val=table->get_as<int>(key_name);val){
        std::string val_str=std::to_string(*val);
        LOGE("load_config_entry I %s %s",tag_str.c_str(),val_str.c_str());
        jstring result=env->NewStringUTF(val_str.c_str());
        return result;
    }
    else if(const auto val=table->get_as<double>(key_name);val){
        std::string val_str=std::to_string(*val);
        LOGE("load_config_entry F %s %s",tag_str.c_str(),val_str.c_str());
        jstring result=env->NewStringUTF(val_str.c_str());
        return result;
    }
    else if(const auto val=table->get_as<std::string>(key_name);val){
        LOGE("load_config_entry S %s %s",tag_str.c_str(),val->c_str());
        jstring result=env->NewStringUTF(val->c_str());
        return result;
    }

    return NULL;
}
static jobjectArray j_load_config_entry_ty_arr(JNIEnv* env,jobject self,std::shared_ptr<cpptoml::table>* config_table,jstring tag){
    return NULL;
}
static void j_save_config_entry(JNIEnv* env,jobject self,std::shared_ptr<cpptoml::table>* config_table,jstring tag,jstring val){
    jboolean is_copy=false;
    const char* tag_cstr=env->GetStringUTFChars(tag,&is_copy);
    const char* val_cstr=env->GetStringUTFChars(val,&is_copy);
    std::string  tag_str(tag_cstr);
    std::string  val_str(val_cstr);
    env->ReleaseStringUTFChars(tag,tag_cstr);
    env->ReleaseStringUTFChars(val,val_cstr);

    size_t pos=tag_str.find('|');
    std::string  table_name=tag_str.substr(0,pos);
    std::string  key_name=tag_str.substr(pos+1);
    std::shared_ptr<cpptoml::table> table=(*config_table)->get_table(table_name);
    if(val_str=="true"||val_str=="false"){
        LOGE("save_config_entry Z %s %s",tag_str.c_str(),val_str.c_str());
        table->insert(key_name,val_str=="true");
    }
    else if(val_str.find('.')!=std::string::npos){
        LOGE("save_config_entry F %s %s",tag_str.c_str(),val_str.c_str());
        table->insert(key_name,std::stod(val_str));
    }
    else if(const auto begin=val_str[0]!='-'?val_str.begin():++val_str.begin();std::all_of(begin,val_str.end(),::isdigit)){
        LOGE("save_config_entry I %s %s",tag_str.c_str(),val_str.c_str());
        table->insert(key_name,std::stoi(val_str));
    }
    else{
        LOGE("save_config_entry S %s %s",tag_str.c_str(),val_str.c_str());
        table->insert(key_name,val_str);
    }
}
static void j_save_config_entry_ty_arr(JNIEnv* env,jobject self,std::shared_ptr<cpptoml::table>* config_table,jstring tag,jobjectArray val) {
}
static void j_close_config_file(JNIEnv* env,jobject self,std::shared_ptr<cpptoml::table>* config_table,jstring config_path){
    jboolean is_copy=false;
    const char* path=env->GetStringUTFChars(config_path,&is_copy);
    std::ofstream fout(path);
    fout << **config_table;
    fout.close();
    env->ReleaseStringUTFChars(config_path,path);
    std::unique_ptr<std::shared_ptr<cpptoml::table>> toml_p;
    toml_p.reset(config_table);
}
#else
#error "CFG_TYPE_XXX not defined"
#endif

int register_Emulator$Config(JNIEnv* env){

    static const JNINativeMethod methods[] = {

            { "native_open_config", "(Ljava/lang/String;)J", (void *) j_open_config_str },
            { "native_close_config", "(J)Ljava/lang/String;", (void *) j_close_config_str },
            { "native_open_config_file", "(Ljava/lang/String;)J", (void *) j_open_config_file },
            { "native_load_config_entry", "(JLjava/lang/String;)Ljava/lang/String;", (void *) j_load_config_entry },
            { "native_load_config_entry_ty_arr", "(JLjava/lang/String;)[Ljava/lang/String;", (void *) j_load_config_entry_ty_arr },
            { "native_save_config_entry", "(JLjava/lang/String;Ljava/lang/String;)V", (void *) j_save_config_entry },
            { "native_save_config_entry_ty_arr", "(JLjava/lang/String;[Ljava/lang/String;)V", (void *) j_save_config_entry_ty_arr },
            { "native_close_config_file", "(JLjava/lang/String;)V", (void *) j_close_config_file },
            { "native_list_children", "(JLjava/lang/String;)[Ljava/lang/String;", (void *) j_list_children },
    };
    jclass clazz = env->FindClass("aenu/emulator/Emulator$Config");
    return env->RegisterNatives(clazz,methods, sizeof(methods)/sizeof(methods[0]));
}

//public native void setup_game_path(String path);
//public native void setup_game_path(Emulator.Path path);
static void j_setup_game_path(JNIEnv* env,jobject self,jobject path ){

    jclass path_cls;
    if(env->IsInstanceOf(path,path_cls=env->FindClass("aenu/emulator/Emulator$Path"))){
        jfieldID f_fd  = env->GetFieldID(path_cls, "fd", "I");
        ae::boot_game_fd=env->GetIntField(path, f_fd);
        jfieldID f_dec_key_fd  = env->GetFieldID(path_cls, "dec_key_fd", "I");
        ae::boot_game_dec_key_fd=env->GetIntField(path, f_dec_key_fd);
        if(ae::boot_game_fd!=-1)
            ae::boot_type=ae::BOOT_TYPE_WITH_FD;
        else{
            jfieldID f_uri  = env->GetFieldID(path_cls, "uri", "Ljava/lang/String;");
            ae::boot_game_uri=std::string(env->GetStringUTFChars(reinterpret_cast<jstring>(env->GetObjectField(path, f_uri)), nullptr));
            ae::boot_type=ae::BOOT_TYPE_WITH_URI;
        }
    }
    else if(env->IsInstanceOf(path,path_cls=env->FindClass("java/lang/String"))){
        const char*  _path = env->GetStringUTFChars(reinterpret_cast<jstring>(path), nullptr);
        ae::boot_type=ae::BOOT_TYPE_WITH_PATH;
        ae::boot_game_path=std::string(_path);
        env->ReleaseStringUTFChars(reinterpret_cast<jstring>(path), _path);
    }
}

static void j_boot(JNIEnv* env,jobject self){
    std::thread(ae::main_thr).detach();
}

static void j_change_surface(JNIEnv* env,jobject self,jint w,jint h){
    ae::window_width=w;
    ae::window_height=h;
}

static void j_setup_surface(JNIEnv* env,jobject self,jobject surface){

    if(ae::window){
        ANativeWindow_release(ae::window);
        ae::window=nullptr;}
    if(surface) {
        ae::window=ANativeWindow_fromSurface(env,surface);
        ae::window_width=ANativeWindow_getWidth(ae::window);
        ae::window_height=ANativeWindow_getHeight(ae::window);
    }
}

static void j_key_event(JNIEnv* env,jobject self,jint key_code,jboolean pressed,jint value){
    ae::key_event(key_code,pressed,value);
}

static void j_pause(JNIEnv* env,jobject self){
    ae::pause();
}

static void j_resume(JNIEnv* env,jobject self){
    ae::resume();
}

static jboolean j_is_running(JNIEnv* env,jobject self){
    return ae::is_running();
}

static jboolean j_is_paused(JNIEnv* env,jobject self){
    return ae::is_paused();
}

static void j_quit(JNIEnv* env,jobject self){
    ae::quit();
}

int register_Emulator(JNIEnv* env){
    static const JNINativeMethod methods[] = {
            { "setup_game_path", "(Ljava/lang/String;)V", (void *) j_setup_game_path },
            { "setup_game_path", "(Laenu/emulator/Emulator$Path;)V", (void *) j_setup_game_path },
            { "setup_surface", "(Landroid/view/Surface;)V", (void *) j_setup_surface },
            { "boot", "()V", (void *) j_boot },
            { "key_event", "(IZI)V", (void *) j_key_event },
            { "quit", "()V", (void *) j_quit },
            { "is_running", "()Z", (void *) j_is_running },
            { "is_paused", "()Z", (void *) j_is_paused },
            { "pause", "()V", (void *) j_pause },
            { "resume", "()V", (void *) j_resume },
            { "change_surface", "(II)V", (void *) j_change_surface },
    };
    return env->RegisterNatives(env->FindClass("aenu/emulator/Emulator"),methods, sizeof(methods)/sizeof(methods[0]));
}