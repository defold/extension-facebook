#if defined(DM_PLATFORM_ANDROID)

#include <assert.h>

#include <dmsdk/sdk.h>
#include <dmsdk/extension/extension.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/mutex.h>
#include <dmsdk/script/script.h>

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

static JNIEnv* Attach()
{
    JNIEnv* env;
    g_AndroidApp->activity->vm->AttachCurrentThread(&env, NULL);
    return env;
}

static bool Detach(JNIEnv* env)
{
    bool exception = (bool) env->ExceptionCheck();
    env->ExceptionClear();
    g_AndroidApp->activity->vm->DetachCurrentThread();

    return !exception;
}

namespace dmFacebook {

int Facebook_Logout(lua_State* L)
{
    if(!g_Facebook.m_FBApp)
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    int top = lua_gettop(L);

    JNIEnv* env = Attach();

    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_Logout);

    if (!Detach(env))
    {
        luaL_error(L, "An unexpected error occurred.");
    }

    assert(top == lua_gettop(L));
    return 0;
}

bool PlatformFacebookInitialized()
{
    return (bool)g_Facebook.m_FBApp;
}

void Platform_FacebookLoginWithPermissions(lua_State* L, const char** permissions,
    uint32_t permission_count, int audience, dmScript::LuaCallbackInfo* callback)
{
    // This function must always return so memory for `permissions` can be free'd.
    char cstr_permissions[2048];
    cstr_permissions[0] = 0x0;
    JoinCStringArray(permissions, permission_count, cstr_permissions, sizeof(cstr_permissions) / sizeof(cstr_permissions[0]), ",");

    JNIEnv* environment = Attach();
    jstring jstr_permissions = environment->NewStringUTF(cstr_permissions);
    environment->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_LoginWithPermissions, (jlong) callback, (jint) audience, jstr_permissions);
    environment->DeleteLocalRef(jstr_permissions);

    if (!Detach(environment))
    {
        dmLogError("An unexpected error occurred during Facebook JNI interaction.");
    }
}

int Facebook_AccessToken(lua_State* L)
{
    if(!g_Facebook.m_FBApp)
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    int top = lua_gettop(L);

    JNIEnv* env = Attach();

    jstring str_access_token = (jstring)env->CallObjectMethod(g_Facebook.m_FB, g_Facebook.m_GetAccessToken);

    if (str_access_token) {
        const char* access_token = env->GetStringUTFChars(str_access_token, 0);
        lua_pushstring(L, access_token);
        env->ReleaseStringUTFChars(str_access_token, access_token);
    } else {
        lua_pushnil(L);
    }

    if (!Detach(env))
    {
        luaL_error(L, "An unexpected error occurred.");
    }

    assert(top + 1 == lua_gettop(L));
    return 1;
}

int Facebook_Permissions(lua_State* L)
{
    if(!g_Facebook.m_FBApp)
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    int top = lua_gettop(L);

    lua_newtable(L);

    JNIEnv* env = Attach();

    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_IteratePermissions, (jlong)L);

    if (!Detach(env))
    {
        luaL_error(L, "An unexpected error occurred.");
    }

    assert(top + 1 == lua_gettop(L));
    return 1;
}

int Facebook_PostEvent(lua_State* L)
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
    JNIEnv* env = Attach();
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

    if (!Detach(env))
    {
        luaL_error(L, "An unexpected error occurred.");
    }

    return 0;
}

int Facebook_EnableEventUsage(lua_State* L)
{
    JNIEnv* env = Attach();
    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_EnableEventUsage);

    if (!Detach(env))
    {
        luaL_error(L, "An unexpected error occurred.");
    }

    return 0;
}

int Facebook_DisableEventUsage(lua_State* L)
{
    JNIEnv* env = Attach();
    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_DisableEventUsage);

    if (!Detach(env))
    {
        luaL_error(L, "An unexpected error occurred.");
    }

    return 0;
}

int Facebook_ShowDialog(lua_State* L)
{
    if(!g_Facebook.m_FBApp)
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    int top = lua_gettop(L);

    const char* dialog = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    dmScript::LuaCallbackInfo* callback = dmScript::CreateCallback(L, 3);

    JNIEnv* env = Attach();

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

    if (!Detach(env))
    {
        assert(top == lua_gettop(L));
        return luaL_error(L, "An unexpected error occurred.");
    }

    assert(top == lua_gettop(L));
    return 0;
}

void Platform_FetchDeferredAppLinkData(lua_State* L, dmScript::LuaCallbackInfo* callback)
{
    DM_LUA_STACK_CHECK(L, 0);

    JNIEnv* environment = Attach();
    environment->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_FetchDeferredAppLink, (jlong)callback);

    if (!Detach(environment))
    {
        luaL_error(L, "An unexpected error occurred during Facebook JNI interaction.");
    }
}

} // namespace

const char* Platform_GetVersion()
{
    JNIEnv* env = Attach();
    jstring jSdkVersion = (jstring)env->CallObjectMethod(g_Facebook.m_FB, g_Facebook.m_GetSdkVersion);
    const char* out = StrDup(env, jSdkVersion);
    if (!Detach(env))
    {
        free((void*)out);
        out = 0;
    }
    return out;
}


bool Platform_FacebookInitialized()
{
    return g_Facebook.m_FBApp != 0 && g_Facebook.m_FB != 0;
}

dmExtension::Result Platform_AppInitializeFacebook(dmExtension::AppParams* params, const char* app_id)
{
    if (!g_Facebook.m_FBApp)
    {
        JNIEnv* env = Attach();

        jclass activity_class = env->FindClass("android/app/NativeActivity");
        jmethodID get_class_loader = env->GetMethodID(activity_class,"getClassLoader", "()Ljava/lang/ClassLoader;");
        jobject cls = env->CallObjectMethod(g_AndroidApp->activity->clazz, get_class_loader);
        jclass class_loader = env->FindClass("java/lang/ClassLoader");
        jmethodID find_class = env->GetMethodID(class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        jstring str_class_name = env->NewStringUTF("com.defold.facebook.FacebookAppJNI");
        jclass fb_class = (jclass)env->CallObjectMethod(cls, find_class, str_class_name);
        env->DeleteLocalRef(str_class_name);

        g_Facebook.m_Activate = env->GetMethodID(fb_class, "activate", "()V");
        g_Facebook.m_Deactivate = env->GetMethodID(fb_class, "deactivate", "()V");
        g_Facebook.m_DisableFaceBookEvents = dmConfigFile::GetInt(params->m_ConfigFile, "facebook.disable_events", 0);

        jmethodID jni_constructor = env->GetMethodID(fb_class, "<init>", "(Landroid/app/Activity;Ljava/lang/String;)V");
        jstring str_app_id = env->NewStringUTF(app_id);
        g_Facebook.m_FBApp = env->NewGlobalRef(env->NewObject(fb_class, jni_constructor, g_AndroidApp->activity->clazz, str_app_id));
        env->DeleteLocalRef(str_app_id);

        if(!g_Facebook.m_DisableFaceBookEvents)
        {
            env->CallVoidMethod(g_Facebook.m_FBApp, g_Facebook.m_Activate);
        }

        return Detach(env) ? dmExtension::RESULT_OK : dmExtension::RESULT_INIT_ERROR;
    }

    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_AppFinalizeFacebook(dmExtension::AppParams* params)
{
    bool javaStatus = false;
    if (g_Facebook.m_FBApp != 0)
    {
        JNIEnv* env = Attach();
        if(!g_Facebook.m_DisableFaceBookEvents)
        {
            env->CallVoidMethod(g_Facebook.m_FBApp, g_Facebook.m_Deactivate);
        }
        env->DeleteGlobalRef(g_Facebook.m_FBApp);

        javaStatus = !Detach(env);

        g_Facebook.m_FBApp = 0;
        memset(&g_Facebook, 0x00, sizeof(Facebook));
    }

    // javaStatus should really be checked and an error returned if something is wrong.
    (void)javaStatus;
    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_InitializeFacebook(dmExtension::Params* params)
{
    if( !g_Facebook.m_FBApp )
    {
        return dmExtension::RESULT_OK;
    }

    if (!g_Facebook.m_FB)
    {
        dmFacebook::QueueCreate(&g_Facebook.m_CommandQueue);

        JNIEnv* env = Attach();

        jclass activity_class = env->FindClass("android/app/NativeActivity");
        jmethodID get_class_loader = env->GetMethodID(activity_class,"getClassLoader", "()Ljava/lang/ClassLoader;");
        jobject cls = env->CallObjectMethod(g_AndroidApp->activity->clazz, get_class_loader);
        jclass class_loader = env->FindClass("java/lang/ClassLoader");
        jmethodID find_class = env->GetMethodID(class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        jstring str_class_name = env->NewStringUTF("com.defold.facebook.FacebookJNI");
        jclass fb_class = (jclass)env->CallObjectMethod(cls, find_class, str_class_name);
        env->DeleteLocalRef(str_class_name);

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

        jmethodID jni_constructor = env->GetMethodID(fb_class, "<init>", "(Landroid/app/Activity;Ljava/lang/String;)V");

        const char* app_id = dmConfigFile::GetString(params->m_ConfigFile, "facebook.appid", 0);
        jstring str_app_id = env->NewStringUTF(app_id);
        g_Facebook.m_FB = env->NewGlobalRef(env->NewObject(fb_class, jni_constructor, g_AndroidApp->activity->clazz, str_app_id));
        env->DeleteLocalRef(str_app_id);

        if (!Detach(env))
        {
            luaL_error(params->m_L, "An unexpected error occurred.");
        }
    }

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
            JNIEnv* env = Attach();
            env->DeleteGlobalRef(g_Facebook.m_FB);

            if (!Detach(env))
            {
                luaL_error(params->m_L, "An unexpected error occurred.");
            }

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
        JNIEnv* env = Attach();

        if( event->m_Event == dmExtension::EVENT_ID_ACTIVATEAPP )
            env->CallVoidMethod(g_Facebook.m_FBApp, g_Facebook.m_Activate);
        else if( event->m_Event == dmExtension::EVENT_ID_DEACTIVATEAPP )
            env->CallVoidMethod(g_Facebook.m_FBApp, g_Facebook.m_Deactivate);

        if (!Detach(env))
        {
            luaL_error(params->m_L, "An unexpected error occurred.");
        }
    }
}

#endif // DM_PLATFORM_ANDROID
