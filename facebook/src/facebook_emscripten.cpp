#if defined(DM_PLATFORM_HTML5)

#include <assert.h>
#include <dmsdk/extension/extension.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/json.h>
#include <dmsdk/script/script.h>
#include <unistd.h>
#include <stdlib.h>

#include "facebook_private.h"
#include "facebook_util.h"
#include "facebook_analytics.h"

// Must match iOS for now
enum Audience
{
    AUDIENCE_NONE = 0,
    AUDIENCE_ONLYME = 10,
    AUDIENCE_FRIENDS = 20,
    AUDIENCE_EVERYONE = 30
};

struct Facebook
{
    Facebook()
    {
        memset(this, 0, sizeof(*this));
        m_Initialized = false;
    }
    dmScript::LuaCallbackInfo* m_Callback;
    const char* m_appId;
    const char* m_PermissionsJson;
    bool m_Initialized;
};

Facebook g_Facebook;

typedef void (*OnAccessTokenCallback)(void* L, const char* access_token);
typedef void (*OnShowDialogCallback)(void* luacallback, const char* url, const char* error);
typedef void (*OnLoginWithPermissionsCallback)(void* luacallback, int status, const char* error, const char* permissions_json);

extern "C" {
    // Implementation in library_facebook.js
    void dmFacebookInitialize(const char* app_id, const char* version);
    void dmFacebookAccessToken(OnAccessTokenCallback callback, lua_State* L);
    void dmFacebookShowDialog(const char* params, const char* method, OnShowDialogCallback callback, dmScript::LuaCallbackInfo* luacallback);
    void dmFacebookDoLogout();
    void dmFacebookLoginWithPermissions(int state_open, int state_closed, int state_failed, const char* permissions, OnLoginWithPermissionsCallback callback, dmScript::LuaCallbackInfo* luacallback);
    void dmFacebookPostEvent(const char* event, double valueToSum, const char* keys, const char* values);
    void dmFacebookEnableEventUsage();
    void dmFacebookDisableEventUsage();
}

namespace dmFacebook
{

int Facebook_Logout(lua_State* L)
{
    if( !g_Facebook.m_appId )
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    int top = lua_gettop(L);

    dmFacebookDoLogout();

    if (g_Facebook.m_PermissionsJson) {
        g_Facebook.m_PermissionsJson = 0;
    }

    assert(top == lua_gettop(L));
    return 0;
}

static void OnLoginWithPermissions(dmScript::LuaCallbackInfo* callback, int status, const char* error, const char* permissions_json)
{
    if (permissions_json != 0x0)
    {
        g_Facebook.m_PermissionsJson = permissions_json;
    }

    dmFacebook::RunStatusCallback(callback, error, status);
    dmScript::DestroyCallback(callback);
}

void Platform_FacebookLoginWithPermissions(lua_State* L, const char** permissions,
    uint32_t permission_count, int audience, dmScript::LuaCallbackInfo* callback)
{
    char cstr_permissions[2048];
    cstr_permissions[0] = 0x0;
    JoinCStringArray(permissions, permission_count, cstr_permissions,
        sizeof(cstr_permissions) / sizeof(cstr_permissions[0]), ",");

    dmFacebookLoginWithPermissions(
        dmFacebook::STATE_OPEN, dmFacebook::STATE_CLOSED, dmFacebook::STATE_CLOSED_LOGIN_FAILED,
        cstr_permissions, (OnLoginWithPermissionsCallback) OnLoginWithPermissions, callback);
}

static void OnAccessTokenComplete(void* L, const char* access_token)
{
    if(access_token != 0)
    {
        lua_pushstring((lua_State *)L, access_token);
    }
    else
    {
        lua_pushnil((lua_State *)L);
        dmLogError("Access_token is null (logged out?).");
    }
}

int Facebook_AccessToken(lua_State* L)
{
    if( !g_Facebook.m_appId )
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    DM_LUA_STACK_CHECK(L, 1);

    dmFacebookAccessToken( (OnAccessTokenCallback) OnAccessTokenComplete, L);

    return 1;
}

int Facebook_Permissions(lua_State* L)
{
    if( !g_Facebook.m_appId )
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    DM_LUA_STACK_CHECK(L, 1);

    if(g_Facebook.m_PermissionsJson != 0)
    {
        // Note that the permissionsJsonArray contains a regular string array in json format,
        // e.g. ["foo", "bar", "baz", ...]
        if (dmFacebook::PushLuaTableFromJson(L, g_Facebook.m_PermissionsJson)) {
            dmLogError("Failed to parse Facebook_Permissions response");
            lua_newtable(L);
        }
    }
    else
    {
        dmLogError("Got empty Facebook_Permissions response (or FB error).");
        // This follows the iOS implementation...
        lua_newtable((lua_State *)L);
    }

    return 1;
}

static void OnShowDialogComplete(dmScript::LuaCallbackInfo* callback, const char* result_json, const char* error)
{
    dmFacebook::RunJsonResultCallback(callback, result_json, error);
    dmScript::DestroyCallback(callback);
}

int Facebook_ShowDialog(lua_State* L)
{
    if( !g_Facebook.m_appId )
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    DM_LUA_STACK_CHECK(L, 0);

    const char* dialog = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    dmScript::LuaCallbackInfo* callback = dmScript::CreateCallback(L, 3);

    lua_newtable(L);
    int to_index = lua_gettop(L);
    if (0 == dmFacebook::DialogTableToEmscripten(L, dialog, 2, to_index)) {
        lua_pop(L, 1);
        return DM_LUA_ERROR("Could not convert show dialog param table.");
    }

    int size_needed = 1 + dmFacebook::LuaTableToJson(L, to_index, 0, 0);
    char* params_json = (char*)malloc(size_needed);

    if (params_json == 0 || 0 == dmFacebook::LuaTableToJson(L, to_index, params_json, size_needed)) {
        lua_pop(L, 1);
        if( params_json ) {
            free(params_json);
        }
        return DM_LUA_ERROR("Dialog params table too large.");
    }
    lua_pop(L, 1);

    dmFacebookShowDialog(params_json, dialog, (OnShowDialogCallback) OnShowDialogComplete, callback);
    free(params_json);
    return 0;
}

int Facebook_PostEvent(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
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

    const char* json_keys = dmFacebook::CStringArrayToJsonString(keys, length);
    const char* json_values = dmFacebook::CStringArrayToJsonString(values, length);

    // Forward call to JavaScript
    dmFacebookPostEvent(event, valueToSum, json_keys, json_values);

    free((void*) json_keys);
    free((void*) json_values);

    return 0;
}

int Facebook_EnableEventUsage(lua_State* L)
{
    dmFacebookEnableEventUsage();

    return 0;
}

int Facebook_DisableEventUsage(lua_State* L)
{
    dmFacebookDisableEventUsage();

    return 0;
}

void Platform_FetchDeferredAppLinkData(lua_State* L, dmScript::LuaCallbackInfo* callback)
{
    dmLogOnceDebug("get_deferred_app_link() function isn't supported on HTML5 platform");
}

} // namespace

const char* Platform_GetVersion()
{
    return strdup(dmFacebook::GRAPH_API_VERSION);
}

bool Platform_FacebookInitialized()
{
    return g_Facebook.m_Initialized;
}

int Facebook_Init(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    if(!g_Facebook.m_Initialized)
    {
        dmFacebookInitialize(g_Facebook.m_appId, dmFacebook::GRAPH_API_VERSION);
        dmLogDebug("FB initialized.");
        g_Facebook.m_Initialized = true;
    }

    return 0;
}

dmExtension::Result Platform_AppInitializeFacebook(dmExtension::AppParams* params, const char* app_id)
{
    (void)params;
    g_Facebook.m_appId = app_id;
    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_AppFinalizeFacebook(dmExtension::AppParams* params)
{
    (void)params;
    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_InitializeFacebook(dmExtension::Params* params)
{

    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_FinalizeFacebook(dmExtension::Params* params)
{
    (void)params;
    // TODO: "Uninit" FB SDK here?
    g_Facebook = Facebook();
    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_UpdateFacebook(dmExtension::Params* params)
{
    (void)params;
    return dmExtension::RESULT_OK;
}

void Platform_OnEventFacebook(dmExtension::Params* params, const dmExtension::Event* event)
{
    (void)params; (void)event;
}

#endif // DM_PLATFORM_HTML5
