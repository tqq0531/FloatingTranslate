#include <jni.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rime_api.h"
static RimeApi* (*loaded_api)(void);
#define rime_get_api() loaded_api()
#include "FuyiRime.c"
JNIEXPORT jstring JNICALL Java_com_fuyi_keyboard_NativePinyin_initialize(JNIEnv* env,jclass cls,jstring library,jstring shared,jstring user) {
 const char *lib=(*env)->GetStringUTFChars(env,library,0),*data=(*env)->GetStringUTFChars(env,shared,0),*runtime=(*env)->GetStringUTFChars(env,user,0);
 void *handle=dlopen(lib,RTLD_NOW|RTLD_LOCAL);
 const char *result="";
 if(!handle) result=dlerror();
 else {loaded_api=dlsym(handle,"rime_get_api");if(!loaded_api)result="Rime API missing";else if(!FuyiRimeInitialize(data,runtime))result="Rime schema initialization failed";}
 jstring answer=(*env)->NewStringUTF(env,result?result:"");
 (*env)->ReleaseStringUTFChars(env,library,lib);(*env)->ReleaseStringUTFChars(env,shared,data);(*env)->ReleaseStringUTFChars(env,user,runtime);return answer;
}
JNIEXPORT jobjectArray JNICALL Java_com_fuyi_keyboard_NativePinyin_query(JNIEnv* env,jclass cls,jstring spelling) {
 const char *raw=(*env)->GetStringUTFChars(env,spelling,0);char* texts[100]={0};int consumed[100]={0};
 int count=FuyiRimeQuery(raw,texts,consumed,100);
 jobjectArray result=(*env)->NewObjectArray(env,count*2,(*env)->FindClass(env,"java/lang/String"),NULL);
 for(int i=0;i<count;i++){char n[24];snprintf(n,sizeof(n),"%d",consumed[i]);(*env)->SetObjectArrayElement(env,result,i*2,(*env)->NewStringUTF(env,texts[i]));(*env)->SetObjectArrayElement(env,result,i*2+1,(*env)->NewStringUTF(env,n));}
 FuyiRimeFreeTexts(texts,count);(*env)->ReleaseStringUTFChars(env,spelling,raw);return result;
}
JNIEXPORT jstring JNICALL Java_com_fuyi_keyboard_NativePinyin_version(JNIEnv* env,jclass cls){return (*env)->NewStringUTF(env,FuyiRimeVersion());}
