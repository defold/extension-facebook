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
        m_Callback = LUA_NOREF;
        m_Initialized = false;
    }
    int m_Callback;
    int m_Self;
    const char* m_appId;
    const char* m_PermissionsJson;
    bool m_Initialized;
};

Facebook g_Facebook;

typedef void (*OnAccessTokenCallback)(void *L, const char* access_token);
typedef void (*OnMeCallback)(void *L, const char* json);
typedef void (*OnShowDialogCallback)(void* L, const char* url, const char* error);
typedef void (*OnLoginWithPermissionsCallback)(void* L, int status, const char* error, const char* permissions_json);

extern "C" {
    // Implementation in library_facebook.js
    void dmFacebookInitialize(const char* app_id, const char* version);
    void dmFacebookAccessToken(OnAccessTokenCallback callback, lua_State* L);
    void dmFacebookShowDialog(const char* params, const char* method, OnShowDialogCallback callback, lua_State* L);
    void dmFacebookDoLogout();
    void dmFacebookLoginWithPermissions(int state_open, int state_closed, int state_failed, const char* permissions, OnLoginWithPermissionsCallback callback, lua_State* thread);
    void dmFacebookPostEvent(const char* event, double valueToSum, const char* keys, const char* values);
    void dmFacebookEnableEventUsage();
    void dmFacebookDisableEventUsage();
}

static void RunCallback(lua_State* L, const char *error)
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        int top = lua_gettop(L);

        int callback = g_Facebook.m_Callback;
        g_Facebook.m_Callback = LUA_NOREF;
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);

        // Setup self
        lua_rawgeti(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);

        if (!dmScript::IsInstanceValid(L))
        {
            dmLogError("Could not run facebook callback because the instance has been deleted.");
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return;
        }

        dmFacebook::PushError(L, error);

        lua_pcall(L, 2, 0, 0);
        assert(top == lua_gettop(L));
        dmScript::Unref(L, LUA_REGISTRYINDEX, callback);
    } else {
        dmLogError("No callback set");
    }
}

static void RunDialogResultCallback(lua_State* L, const char *result_json, const char *error)
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        int top = lua_gettop(L);

        int callback = g_Facebook.m_Callback;
        g_Facebook.m_Callback = LUA_NOREF;
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);

        // Setup self
        lua_rawgeti(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);

        if (!dmScript::IsInstanceValid(L))
        {
            dmLogError("Could not run facebook callback because the instance has been deleted.");
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return;
        }

        if(result_json != 0)
        {
            if (dmFacebook::PushLuaTableFromJson(L, result_json)) {
                dmLogError("Failed to parse dialog result JSON");
                lua_newtable(L);
            }
        }
        else
        {
            dmLogError("Got empty dialog result JSON (or FB error).");
            lua_newtable((lua_State *)L);
        }

        dmFacebook::PushError(L, error);

        lua_pcall(L, 3, 0, 0);
        assert(top == lua_gettop(L));
        dmScript::Unref(L, LUA_REGISTRYINDEX, callback);
    } else {
        dmLogError("No callback set");
    }
}

static void VerifyCallback(lua_State* L)
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        dmLogError("Unexpected callback set");
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Callback);
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        g_Facebook.m_Callback = LUA_NOREF;
        g_Facebook.m_Self = LUA_NOREF;
    }
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
    VerifyCallback(L);

    dmFacebookDoLogout();

    if (g_Facebook.m_PermissionsJson) {
        g_Facebook.m_PermissionsJson = 0;
    }

    assert(top == lua_gettop(L));
    return 0;
}

bool PlatformFacebookInitialized()
{
    return !!g_Facebook.m_appId;
}

static void OnLoginWithPermissions(void* L, int status, const char* error, const char* permissions_json)
{
    if (permissions_json != 0x0)
    {
        g_Facebook.m_PermissionsJson = permissions_json;
    }

    RunCallback((lua_State*) L, &g_Facebook.m_Self, &g_Facebook.m_Callback, error, status);
}

void PlatformFacebookLoginWithPermissions(lua_State* L, const char** permissions,
    uint32_t permission_count, int audience, int callback, int context, lua_State* thread)
{
    VerifyCallback(L);
    g_Facebook.m_Callback = callback;
    g_Facebook.m_Self = context;

    char cstr_permissions[2048];
    cstr_permissions[0] = 0x0;
    JoinCStringArray(permissions, permission_count, cstr_permissions,
        sizeof(cstr_permissions) / sizeof(cstr_permissions[0]), ",");

    dmFacebookLoginWithPermissions(
        dmFacebook::STATE_OPEN, dmFacebook::STATE_CLOSED, dmFacebook::STATE_CLOSED_LOGIN_FAILED,
        cstr_permissions, (OnLoginWithPermissionsCallback) OnLoginWithPermissions, thread);
}

static void AppendArray(lua_State* L, char* buffer, uint32_t buffer_size, int idx)
{
    lua_pushnil(L);
    *buffer = 0;
    while (lua_next(L, idx) != 0)
    {
        if (!lua_isstring(L, -1))
            luaL_error(L, "permissions can only be strings (not %s)", lua_typename(L, lua_type(L, -1)));
        if (*buffer != 0)
            dmFacebook::StrlCat(buffer, ",", buffer_size);
        const char* permission = lua_tostring(L, -1);
        dmFacebook::StrlCat(buffer, permission, buffer_size);
        lua_pop(L, 1);
    }
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
    int top = lua_gettop(L);

    dmFacebookAccessToken( (OnAccessTokenCallback) OnAccessTokenComplete, L);

    assert(top + 1 == lua_gettop(L));
    return 1;
}

int Facebook_Permissions(lua_State* L)
{
    if( !g_Facebook.m_appId )
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    int top = lua_gettop(L);

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

    assert(top + 1 == lua_gettop(L));
    return 1;
}

static void OnShowDialogComplete(void* L, const char* result_json, const char* error)
{
    RunDialogResultCallback((lua_State*)L, result_json, error);
}

int Facebook_ShowDialog(lua_State* L)
{
    if( !g_Facebook.m_appId )
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    int top = lua_gettop(L);
    VerifyCallback(L);

    const char* dialog = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    g_Facebook.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);
    dmScript::GetInstance(L);
    g_Facebook.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);

    lua_newtable(L);
    int to_index = lua_gettop(L);
    if (0 == dmFacebook::DialogTableToEmscripten(L, dialog, 2, to_index)) {
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

    dmFacebookShowDialog(params_json, dialog, (OnShowDialogCallback) OnShowDialogComplete, dmScript::GetMainThread(L));
    free(params_json);

    assert(top == lua_gettop(L));
    return 0;
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

void Platform_FetchDeferredAppLinkData(lua_State* L, int callback, int context, lua_State* thread)
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

dmExtension::Result Platform_AppInitializeFacebook(dmExtension::AppParams* params, const char* app_id)
{
    (void)params;
    (void)app_id;
    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_AppFinalizeFacebook(dmExtension::AppParams* params)
{
    (void)params;
    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_InitializeFacebook(dmExtension::Params* params)
{
    if(!g_Facebook.m_Initialized)
    {
        g_Facebook.m_appId = dmConfigFile::GetString(params->m_ConfigFile, "facebook.appid", 0); // Not sure if we need to initialize facebook this late /mawe
        if( g_Facebook.m_appId )
        {
            dmFacebookInitialize(g_Facebook.m_appId, dmFacebook::GRAPH_API_VERSION);
            dmLogDebug("FB initialized.");
            g_Facebook.m_Initialized = true;
        }
        else
        {
            dmLogDebug("No facebook.appid. Disabling module");
        }
    }

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
