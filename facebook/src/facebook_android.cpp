#if defined(DM_PLATFORM_ANDROID)

#include <assert.h>

#include <dmsdk/sdk.h>
#include <dmsdk/extension/extension.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/mutex.h>
#include <dmsdk/script/script.h>
#include <dmsdk/dlib/android.h>

#include <pthread.h>
#include <unistd.h>

#include "facebook_private.h"
#include "facebook_util.h"
#include "facebook_analytics.h"

extern struct android_app* g_AndroidApp;

struct Facebook
{
    Facebook()
    {
        memset(this, 0, sizeof(*this));
    }

    jobject m_FB;
    jmethodID m_GetSdkVersion;
    jmethodID m_Logout;
    jmethodID m_IteratePermissions;
    jmethodID m_GetAccessToken;
    jmethodID m_ShowDialog;
    jmethodID m_LoginWithPermissions;
    jmethodID m_FetchDeferredAppLink;

    jmethodID m_PostEvent;
    jmethodID m_EnableEventUsage;
    jmethodID m_DisableEventUsage;

    jobject m_FBApp;
    jmethodID m_Activate;
    jmethodID m_Deactivate;

    const char* m_AppId;
    int m_RefCount; // depending if we have a shared state or not
    int m_DisableFaceBookEvents;

    dmFacebook::CommandQueue m_CommandQueue;
};

static Facebook g_Facebook;

static const char* StrDup(JNIEnv* env, jstring s)
{
    if (s != NULL)
    {
        const char* str = env->GetStringUTFChars(s, 0);
        const char* dup = strdup(str);
        env->ReleaseStringUTFChars(s, str);
        return dup;
    }
    else
    {
        return 0x0;
    }
}

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_com_defold_facebook_FacebookJNI_onLogin
  (JNIEnv* env, jobject, jlong userData, jint state, jstring error)
{
    dmFacebook::FacebookCommand cmd;
    cmd.m_Type = dmFacebook::COMMAND_TYPE_LOGIN;
    cmd.m_State = (int)state;
    cmd.m_Callback = (dmScript::LuaCallbackInfo*)userData;
    cmd.m_Error = StrDup(env, error);
    dmFacebook::QueuePush(&g_Facebook.m_CommandQueue, &cmd);
}

JNIEXPORT void JNICALL Java_com_defold_facebook_FacebookJNI_onLoginWithPermissions
  (JNIEnv* env, jobject, jlong userData, jint state, jstring error)
{
    dmFacebook::FacebookCommand cmd;

    cmd.m_Type  = dmFacebook::COMMAND_TYPE_LOGIN_WITH_PERMISSIONS;
    cmd.m_State = (int) state;
    cmd.m_Callback = (dmScript::LuaCallbackInfo*)userData;
    cmd.m_Error = StrDup(env, error);

    dmFacebook::QueuePush(&g_Facebook.m_CommandQueue, &cmd);
}

JNIEXPORT void JNICALL Java_com_defold_facebook_FacebookJNI_onRequestRead
  (JNIEnv* env, jobject, jlong userData, jstring error)
{
    dmFacebook::FacebookCommand cmd;
    cmd.m_Type = dmFacebook::COMMAND_TYPE_REQUEST_READ;
    cmd.m_Callback = (dmScript::LuaCallbackInfo*)userData;
    cmd.m_Error = StrDup(env, error);
    dmFacebook::QueuePush(&g_Facebook.m_CommandQueue, &cmd);
}

JNIEXPORT void JNICALL Java_com_defold_facebook_FacebookJNI_onRequestPublish
  (JNIEnv* env, jobject, jlong userData, jstring error)
{
    dmFacebook::FacebookCommand cmd;
    cmd.m_Type = dmFacebook::COMMAND_TYPE_REQUEST_PUBLISH;
    cmd.m_Callback = (dmScript::LuaCallbackInfo*)userData;
    cmd.m_Error = StrDup(env, error);
    dmFacebook::QueuePush(&g_Facebook.m_CommandQueue, &cmd);
}

JNIEXPORT void JNICALL Java_com_defold_facebook_FacebookJNI_onDialogComplete
  (JNIEnv *env, jobject, jlong userData, jstring results, jstring error)
{
    dmFacebook::FacebookCommand cmd;
    cmd.m_Type = dmFacebook::COMMAND_TYPE_DIALOG_COMPLETE;
    cmd.m_Callback = (dmScript::LuaCallbackInfo*)userData;
    cmd.m_Results = StrDup(env, results);
    cmd.m_Error = StrDup(env, error);
    dmFacebook::QueuePush(&g_Facebook.m_CommandQueue, &cmd);
}

JNIEXPORT void JNICALL Java_com_defold_facebook_FacebookJNI_onIterateMeEntry
  (JNIEnv* env, jobject, jlong user_data, jstring key, jstring value)
{
    dmScript::LuaCallbackInfo* callback = (dmScript::LuaCallbackInfo*)user_data;
    lua_State* L = dmScript::GetCallbackLuaContext(callback);
    if (key) {
        const char* str_key = env->GetStringUTFChars(key, 0);
        lua_pushstring(L, str_key);
        env->ReleaseStringUTFChars(key, str_key);
    } else {
        lua_pushnil(L);
    }

    if (value) {
        const char* str_value = env->GetStringUTFChars(value, 0);
        lua_pushstring(L, str_value);
        env->ReleaseStringUTFChars(value, str_value);
    } else {
        lua_pushnil(L);
    }
    lua_rawset(L, -3);
}

JNIEXPORT void JNICALL Java_com_defold_facebook_FacebookJNI_onIteratePermissionsEntry
  (JNIEnv* env, jobject, jlong user_data, jstring permission)
{
    lua_State* L = (lua_State*)user_data;
    int i = lua_objlen(L, -1);

    lua_pushnumber(L, i + 1);

    if (permission) {
        const char* str_permission = env->GetStringUTFChars(permission, 0);
        lua_pushstring(L, str_permission);
        env->ReleaseStringUTFChars(permission, str_permission);
    } else {
        lua_pushnil(L);
    }
    lua_rawset(L, -3);
}

JNIEXPORT void JNICALL Java_com_defold_facebook_FacebookJNI_onFetchDeferredAppLinkData
  (JNIEnv* env, jobject, jlong user_data, jstring results, jboolean isError)
{
    dmFacebook::FacebookCommand cmd;
    cmd.m_Callback = (dmScript::LuaCallbackInfo*)user_data;
    cmd.m_Type = dmFacebook::COMMAND_TYPE_DEFERRED_APP_LINK;
    if (JNI_TRUE == isError) {
        cmd.m_Error = StrDup(env, results);
    } else {
        cmd.m_Results = StrDup(env, results);
    }
    dmFacebook::QueuePush(&g_Facebook.m_CommandQueue, &cmd);
}

#ifdef __cplusplus
}
#endif

int Platform_FacebookLogout(lua_State* L)
{
    int top = lua_gettop(L);

    dmAndroid::ThreadAttacher threadAttacher;
    JNIEnv* env = threadAttacher.GetEnv();

    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_Logout);

    assert(top == lua_gettop(L));
    return 0;
}

int Platform_FacebookLoginWithPermissions(lua_State* L, const char** permissions,
    uint32_t permission_count, int audience, dmScript::LuaCallbackInfo* callback)
{
    // This function must always return so memory for `permissions` can be free'd.
    char cstr_permissions[2048];
    cstr_permissions[0] = 0x0;
    dmFacebook::JoinCStringArray(permissions, permission_count, cstr_permissions, sizeof(cstr_permissions) / sizeof(cstr_permissions[0]), ",");

    dmAndroid::ThreadAttacher threadAttacher;
    JNIEnv* env = threadAttacher.GetEnv();

    jstring jstr_permissions = env->NewStringUTF(cstr_permissions);
    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_LoginWithPermissions, (jlong) callback, (jint) audience, jstr_permissions);
    env->DeleteLocalRef(jstr_permissions);

    return 0;
}

int Platform_FacebookAccessToken(lua_State* L)
{
    int top = lua_gettop(L);

    dmAndroid::ThreadAttacher threadAttacher;
    JNIEnv* env = threadAttacher.GetEnv();

    jstring str_access_token = (jstring)env->CallObjectMethod(g_Facebook.m_FB, g_Facebook.m_GetAccessToken);

    if (str_access_token) {
        const char* access_token = env->GetStringUTFChars(str_access_token, 0);
        lua_pushstring(L, access_token);
        env->ReleaseStringUTFChars(str_access_token, access_token);
    } else {
        lua_pushnil(L);
    }

    assert(top + 1 == lua_gettop(L));
    return 1;
}

int Platform_FacebookPermissions(lua_State* L)
{
    int top = lua_gettop(L);

    lua_newtable(L);

    dmAndroid::ThreadAttacher threadAttacher;
    JNIEnv* env = threadAttacher.GetEnv();

    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_IteratePermissions, (jlong)L);

    assert(top + 1 == lua_gettop(L));
    return 1;
}

int Platform_FacebookPostEvent(lua_State* L)
{
    int argc = lua_gettop(L);
    const char* event = dmFacebook::Analytics::GetEvent(L, 1);
    double valueToSum = luaL_checknumber(L, 2);

    // Transform LUA table to a format that can be used by all platforms.
    const char* keys[dmFacebook::Analytics::MAX_PARAMS] = { 0 };
    const char* values[dmFacebook::Analytics::MAX_PARAMS] = { 0 };
    unsigned int length = 0;
    // TABLE is an optional argument and should only be parsed if provided.
    if (argc == 3)
    {
        length = dmFacebook::Analytics::MAX_PARAMS;
        dmFacebook::Analytics::GetParameterTable(L, 3, keys, values, &length);
    }

    // Prepare Java call
    dmAndroid::ThreadAttacher threadAttacher;
    JNIEnv* env = threadAttacher.GetEnv();

    jstring jEvent = env->NewStringUTF(event);
    jdouble jValueToSum = (jdouble) valueToSum;
    jclass jStringClass = env->FindClass("java/lang/String");
    jobjectArray jKeys = env->NewObjectArray(length, jStringClass, 0);
    jobjectArray jValues = env->NewObjectArray(length, jStringClass, 0);
    for (unsigned int i = 0; i < length; ++i)
    {
        jstring jKey = env->NewStringUTF(keys[i]);
        env->SetObjectArrayElement(jKeys, i, jKey);
        jstring jValue = env->NewStringUTF(values[i]);
        env->SetObjectArrayElement(jValues, i, jValue);
    }

    // Call com.defold.facebook.FacebookJNI.postEvent
    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_PostEvent, jEvent, jValueToSum, jKeys, jValues);

    return 0;
}

int Platform_FacebookEnableEventUsage(lua_State* L)
{
    dmAndroid::ThreadAttacher threadAttacher;
    JNIEnv* env = threadAttacher.GetEnv();

    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_EnableEventUsage);

    return 0;
}

int Platform_FacebookDisableEventUsage(lua_State* L)
{
    dmAndroid::ThreadAttacher threadAttacher;
    JNIEnv* env = threadAttacher.GetEnv();

    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_DisableEventUsage);

    return 0;
}

int Platform_FacebookShowDialog(lua_State* L)
{
    int top = lua_gettop(L);

    const char* dialog = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    dmScript::LuaCallbackInfo* callback = dmScript::CreateCallback(L, 3);

    dmAndroid::ThreadAttacher threadAttacher;
    JNIEnv* env = threadAttacher.GetEnv();

    lua_newtable(L);
    int to_index = lua_gettop(L);
    if (0 == dmFacebook::DialogTableToAndroid(L, dialog, 2, to_index)) {
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return luaL_error(L, "Could not convert show dialog param table.");
    }

    int size_needed = 1 + dmFacebook::LuaTableToJson(L, to_index, 0, 0);
    char* params_json = (char*)malloc(size_needed);

    if (params_json == 0 || 0 == dmFacebook::LuaTableToJson(L, to_index, params_json, size_needed)) {
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        if( params_json ) {
            free(params_json);
        }
        return luaL_error(L, "Dialog params table too large.");
    }
    lua_pop(L, 1);

    jstring str_dialog = env->NewStringUTF(dialog);
    jstring str_params = env->NewStringUTF(params_json);
    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_ShowDialog, (jlong)callback, str_dialog, str_params);
    env->DeleteLocalRef(str_dialog);
    env->DeleteLocalRef(str_params);
    free((void*)params_json);

    assert(top == lua_gettop(L));
    return 0;
}

int Platform_FetchDeferredAppLinkData(lua_State* L, dmScript::LuaCallbackInfo* callback)
{
    DM_LUA_STACK_CHECK(L, 0);

    dmAndroid::ThreadAttacher threadAttacher;
    JNIEnv* env = threadAttacher.GetEnv();

    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_FetchDeferredAppLink, (jlong)callback);

    return 0;
}

const char* Platform_GetVersion()
{
    dmAndroid::ThreadAttacher threadAttacher;
    JNIEnv* env = threadAttacher.GetEnv();

    jstring jSdkVersion = (jstring)env->CallObjectMethod(g_Facebook.m_FB, g_Facebook.m_GetSdkVersion);
    const char* out = StrDup(env, jSdkVersion);

    return out;
}


bool Platform_FacebookInitialized()
{
    return g_Facebook.m_FBApp != 0 && g_Facebook.m_FB != 0;
}

int Platform_FacebookInit(lua_State* L)
{
    dmAndroid::ThreadAttacher threadAttacher;
    JNIEnv* env = threadAttacher.GetEnv();

    // FacebookAppJNI
    {
        jclass fb_app_class = dmAndroid::LoadClass(env, "com.defold.facebook.FacebookAppJNI");

        g_Facebook.m_Activate = env->GetMethodID(fb_app_class, "activate", "()V");
        g_Facebook.m_Deactivate = env->GetMethodID(fb_app_class, "deactivate", "()V");

        jmethodID fb_app_jni_constructor = env->GetMethodID(fb_app_class, "<init>", "(Landroid/app/Activity;Ljava/lang/String;)V");
        jstring str_app_id = env->NewStringUTF(g_Facebook.m_AppId);
        g_Facebook.m_FBApp = env->NewGlobalRef(env->NewObject(fb_app_class, fb_app_jni_constructor, g_AndroidApp->activity->clazz, str_app_id));
        env->DeleteLocalRef(str_app_id);

        if(!g_Facebook.m_DisableFaceBookEvents)
        {
            env->CallVoidMethod(g_Facebook.m_FBApp, g_Facebook.m_Activate);
        }
    }

    // FacebookJNI
    {
        jclass fb_class = dmAndroid::LoadClass(env, "com.defold.facebook.FacebookJNI");

        g_Facebook.m_GetSdkVersion = env->GetMethodID(fb_class, "getSdkVersion", "()Ljava/lang/String;");
        g_Facebook.m_Logout = env->GetMethodID(fb_class, "logout", "()V");
        g_Facebook.m_IteratePermissions = env->GetMethodID(fb_class, "iteratePermissions", "(J)V");
        g_Facebook.m_GetAccessToken = env->GetMethodID(fb_class, "getAccessToken", "()Ljava/lang/String;");
        g_Facebook.m_ShowDialog = env->GetMethodID(fb_class, "showDialog", "(JLjava/lang/String;Ljava/lang/String;)V");
        g_Facebook.m_FetchDeferredAppLink = env->GetMethodID(fb_class, "fetchDeferredAppLinkData", "(J)V");

        g_Facebook.m_LoginWithPermissions = env->GetMethodID(fb_class, "loginWithPermissions", "(JILjava/lang/String;)V");

        g_Facebook.m_PostEvent = env->GetMethodID(fb_class, "postEvent", "(Ljava/lang/String;D[Ljava/lang/String;[Ljava/lang/String;)V");
        g_Facebook.m_EnableEventUsage = env->GetMethodID(fb_class, "enableEventUsage", "()V");
        g_Facebook.m_DisableEventUsage = env->GetMethodID(fb_class, "disableEventUsage", "()V");

        jmethodID fb_jni_constructor = env->GetMethodID(fb_class, "<init>", "(Landroid/app/Activity;Ljava/lang/String;)V");

        jstring str_app_id = env->NewStringUTF(g_Facebook.m_AppId);
        g_Facebook.m_FB = env->NewGlobalRef(env->NewObject(fb_class, fb_jni_constructor, g_AndroidApp->activity->clazz, str_app_id));
        env->DeleteLocalRef(str_app_id);
    }

    return 0;
}

dmExtension::Result Platform_AppInitializeFacebook(dmExtension::AppParams* params, const char* app_id)
{
    g_Facebook.m_AppId = app_id;
    g_Facebook.m_DisableFaceBookEvents = dmConfigFile::GetInt(params->m_ConfigFile, "facebook.disable_events", 0);
    dmFacebook::QueueCreate(&g_Facebook.m_CommandQueue);

    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_AppFinalizeFacebook(dmExtension::AppParams* params)
{
    if (g_Facebook.m_FBApp != 0)
    {
        dmAndroid::ThreadAttacher threadAttacher;
        JNIEnv* env = threadAttacher.GetEnv();

        if(!g_Facebook.m_DisableFaceBookEvents)
        {
            env->CallVoidMethod(g_Facebook.m_FBApp, g_Facebook.m_Deactivate);
        }
        env->DeleteGlobalRef(g_Facebook.m_FBApp);

        g_Facebook.m_FBApp = 0;
        memset(&g_Facebook, 0x00, sizeof(Facebook));
    }

    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_InitializeFacebook(dmExtension::Params* params)
{
    g_Facebook.m_RefCount++;

    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_UpdateFacebook(dmExtension::Params* params)
{
    dmFacebook::QueueFlush(&g_Facebook.m_CommandQueue, dmFacebook::HandleCommand, (void*)params->m_L);
    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_FinalizeFacebook(dmExtension::Params* params)
{
    if (g_Facebook.m_FB != NULL)
    {
        if (--g_Facebook.m_RefCount == 0)
        {
            dmAndroid::ThreadAttacher threadAttacher;
            JNIEnv* env = threadAttacher.GetEnv();

            env->DeleteGlobalRef(g_Facebook.m_FB);

            g_Facebook.m_FB = NULL;

            dmFacebook::QueueDestroy(&g_Facebook.m_CommandQueue);
        }
    }

    return dmExtension::RESULT_OK;
}

void Platform_OnEventFacebook(dmExtension::Params* params, const dmExtension::Event* event)
{
    if( (g_Facebook.m_FBApp) && (!g_Facebook.m_DisableFaceBookEvents ) )
    {
        dmAndroid::ThreadAttacher threadAttacher;
        JNIEnv* env = threadAttacher.GetEnv();

        if( event->m_Event == dmExtension::EVENT_ID_ACTIVATEAPP )
            env->CallVoidMethod(g_Facebook.m_FBApp, g_Facebook.m_Activate);
        else if( event->m_Event == dmExtension::EVENT_ID_DEACTIVATEAPP )
            env->CallVoidMethod(g_Facebook.m_FBApp, g_Facebook.m_Deactivate);
    }
}

#endif // DM_PLATFORM_ANDROID
