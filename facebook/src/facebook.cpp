#if defined(DM_PLATFORM_ANDROID) || defined(DM_PLATFORM_IOS) || defined(DM_PLATFORM_HTML5)

#include "facebook_private.h"
#include "facebook_analytics.h"
#include <dmsdk/sdk.h>
#include <dmsdk/dlib/log.h>

#define MODULE_NAME "facebook"  // the c++ module name from ext.manifest, also used as the Lua module name

namespace dmFacebook {

#define CHECK_FACEBOOK_INIT(_L_) \
    if (!Platform_FacebookInitialized()) return luaL_error(_L_, "Facebook has not been initialized, is facebook.appid set in game.project?");

static int Facebook_AccessToken(lua_State* L)
{
    CHECK_FACEBOOK_INIT(L);
    return Platform_FacebookAccessToken(L);
}

static int Facebook_Logout(lua_State* L)
{
    CHECK_FACEBOOK_INIT(L);
    return Platform_FacebookLogout(L);
}

static int Facebook_Permissions(lua_State* L)
{
    CHECK_FACEBOOK_INIT(L);
    return Platform_FacebookPermissions(L);
}

static int Facebook_PostEvent(lua_State* L)
{
    CHECK_FACEBOOK_INIT(L);
    return Platform_FacebookPostEvent(L);
}

static int Facebook_EnableEventUsage(lua_State* L)
{
    CHECK_FACEBOOK_INIT(L);
    return Platform_FacebookEnableEventUsage(L);
}

static int Facebook_DisableEventUsage(lua_State* L)
{
    CHECK_FACEBOOK_INIT(L);
    return Platform_FacebookDisableEventUsage(L);
}

static int Facebook_ShowDialog(lua_State* L)
{
    CHECK_FACEBOOK_INIT(L);
    return Platform_FacebookShowDialog(L);
}

static int Facebook_LoginWithPermissions(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    CHECK_FACEBOOK_INIT(L);

    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TNUMBER);
    dmScript::LuaCallbackInfo* callback = dmScript::CreateCallback(L, 3);

    char* permissions[128];
    int permission_count = luaTableToCArray(L, 1, permissions, sizeof(permissions) / sizeof(permissions[0]));
    if (permission_count == -1)
    {
        return luaL_error(L, "Facebook permissions must be strings");
    }

    int audience = luaL_checkinteger(L, 2);

    Platform_FacebookLoginWithPermissions(L, (const char**) permissions, permission_count, audience, callback);

    for (int i = 0; i < permission_count; ++i) {
        free(permissions[i]);
    }

    return 0;
}

static int Facebook_GetVersion(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    CHECK_FACEBOOK_INIT(L);

    const char* version = Platform_GetVersion();
    if (!version)
    {
        return luaL_error(L, "get_version not supported");
    }
    lua_pushstring(L, version);
    free((void*)version);

    return 1;
}

static int Facebook_FetchDeferredAppLinkData(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    CHECK_FACEBOOK_INIT(L);

    dmScript::LuaCallbackInfo* callback = dmScript::CreateCallback(L, 1);

    Platform_FetchDeferredAppLinkData(L, callback);

    return 0;
}

static int Facebook_Init(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (Platform_FacebookInitialized())
    {
        return luaL_error(L, "Facebook has already been initialized");
    }
    Platform_FacebookInit(L);

    return 0;
}

static int Facebook_EnableAdvertiserTracking(lua_State* L)
{
#if defined(DM_PLATFORM_IOS) 
    CHECK_FACEBOOK_INIT(L);
    return Platform_FacebookEnableAdvertiserTracking(L);
#else
    return 0;
#endif
}

static int Facebook_DisableAdvertiserTracking(lua_State* L)
{
#if defined(DM_PLATFORM_IOS) 
    CHECK_FACEBOOK_INIT(L);
    return Platform_FacebookDisableAdvertiserTracking(L);
#else
    return 0;
#endif
}

static int Facebook_SetDefaultAudience(lua_State* L)
{
#if defined(DM_PLATFORM_IOS) 
    CHECK_FACEBOOK_INIT(L);
    DM_LUA_STACK_CHECK(L, 0);
    int audience = luaL_checkinteger(L, 1);
    Platform_FacebookSetDefaultAudience(audience);
    return 0;
#else
    dmLogWarning("`set_default_audience` is iOS only function.");
    return 0;
#endif
}

static int Facebook_LoginWithTrackingPreference(lua_State* L)
{
#if defined(DM_PLATFORM_IOS) 
    CHECK_FACEBOOK_INIT(L);
    DM_LUA_STACK_CHECK(L, 0);

    int login_tracking = luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    int type = lua_type(L, 3);
    const char* crypto_nonce = 0x0;
    if (type == LUA_TSTRING)
    {
        crypto_nonce = lua_tostring(L, 3);
    }
    else if (type == LUA_TNIL)
    {
        // do nothing, it's fine
    }
    else
    {
        return luaL_error(L, "`crypto_nonce` should be string or `nil`");
    }
    dmScript::LuaCallbackInfo* callback = dmScript::CreateCallback(L, 4);

    char* permissions[128];
    int permission_count = luaTableToCArray(L, 2, permissions, sizeof(permissions) / sizeof(permissions[0]));
    if (permission_count == -1)
    {
        return luaL_error(L, "Facebook permissions must be strings");
    }

    Platform_FacebookLoginWithTrackingPreference((dmFacebook::LoginTracking)login_tracking, (const char**) permissions, permission_count, crypto_nonce, callback);

    for (int i = 0; i < permission_count; ++i)
    {
        free(permissions[i]);
    }
    return 0;
#else
    dmLogWarning("`login_with_tracking_preference` is iOS only function.");
    return 0;
#endif
}

static int Facebook_GetCurrentAuthenticationToken(lua_State* L)
{
#if defined(DM_PLATFORM_IOS) 
    CHECK_FACEBOOK_INIT(L);
    DM_LUA_STACK_CHECK(L, 1);
    const char* token = Platform_FacebookGetCurrentAuthenticationToken();
    if (token == 0x0)
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushstring(L, token);
    }
    return 1;
#else
    dmLogWarning("`get_current_authentication_token` is iOS only function.");
    return 0;
#endif
}

static int Facebook_GetCurrentProfile(lua_State* L)
{
#if defined(DM_PLATFORM_IOS) 
    CHECK_FACEBOOK_INIT(L);
    DM_LUA_STACK_CHECK(L, 1);
    return Platform_FacebookGetCurrentProfile(L);
#else
    dmLogWarning("`get_current_profile` is iOS only function.");
    return 0;
#endif
}

static const luaL_reg Facebook_methods[] =
{
    {"init", Facebook_Init},
    {"get_version", Facebook_GetVersion},
    {"logout", Facebook_Logout},
    {"access_token", Facebook_AccessToken},
    {"permissions", Facebook_Permissions},
    {"post_event", Facebook_PostEvent},
    {"enable_event_usage", Facebook_EnableEventUsage},
    {"disable_event_usage", Facebook_DisableEventUsage},
    {"show_dialog", Facebook_ShowDialog},
    {"login_with_permissions", Facebook_LoginWithPermissions},
    {"deferred_deep_link", Facebook_FetchDeferredAppLinkData},
    {"enable_advertiser_tracking", Facebook_EnableAdvertiserTracking},
    {"disable_advertiser_tracking", Facebook_DisableAdvertiserTracking},
    // iOS only function. FB SDK 17.0:
    {"set_default_audience", Facebook_SetDefaultAudience},
    {"login_with_tracking_preference", Facebook_LoginWithTrackingPreference},
    {"get_current_authentication_token", Facebook_GetCurrentAuthenticationToken},
    {"get_current_profile", Facebook_GetCurrentProfile},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, MODULE_NAME, Facebook_methods);

    #define SETCONSTANT(name, val) \
        lua_pushnumber(L, (lua_Number) val); \
        lua_setfield(L, -2, #name);\

    SETCONSTANT(STATE_CREATED,              dmFacebook::STATE_CREATED);
    SETCONSTANT(STATE_CREATED_TOKEN_LOADED, dmFacebook::STATE_CREATED_TOKEN_LOADED);
    SETCONSTANT(STATE_CREATED_OPENING,      dmFacebook::STATE_CREATED_OPENING);
    SETCONSTANT(STATE_OPEN,                 dmFacebook::STATE_OPEN);
    SETCONSTANT(STATE_OPEN_TOKEN_EXTENDED,  dmFacebook::STATE_OPEN_TOKEN_EXTENDED);
    SETCONSTANT(STATE_CLOSED,               dmFacebook::STATE_CLOSED);
    SETCONSTANT(STATE_CLOSED_LOGIN_FAILED,  dmFacebook::STATE_CLOSED_LOGIN_FAILED);

    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_NONE,   dmFacebook::GAMEREQUEST_ACTIONTYPE_NONE);
    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_SEND,   dmFacebook::GAMEREQUEST_ACTIONTYPE_SEND);
    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_ASKFOR, dmFacebook::GAMEREQUEST_ACTIONTYPE_ASKFOR);
    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_TURN,   dmFacebook::GAMEREQUEST_ACTIONTYPE_TURN);

    SETCONSTANT(GAMEREQUEST_FILTER_NONE,        dmFacebook::GAMEREQUEST_FILTER_NONE);
    SETCONSTANT(GAMEREQUEST_FILTER_APPUSERS,    dmFacebook::GAMEREQUEST_FILTER_APPUSERS);
    SETCONSTANT(GAMEREQUEST_FILTER_APPNONUSERS, dmFacebook::GAMEREQUEST_FILTER_APPNONUSERS);

    SETCONSTANT(AUDIENCE_NONE,     dmFacebook::AUDIENCE_NONE);
    SETCONSTANT(AUDIENCE_ONLYME,   dmFacebook::AUDIENCE_ONLYME);
    SETCONSTANT(AUDIENCE_FRIENDS,  dmFacebook::AUDIENCE_FRIENDS);
    SETCONSTANT(AUDIENCE_EVERYONE, dmFacebook::AUDIENCE_EVERYONE);

    SETCONSTANT(LOGIN_TRACKING_LIMITED,  dmFacebook::LOGIN_TRACKING_LIMITED);
    SETCONSTANT(LOGIN_TRACKING_ENABLED, dmFacebook::LOGIN_TRACKING_ENABLED);

#undef SETCONSTANT

#if defined(DM_PLATFORM_HTML5)
    lua_pushstring(L, dmFacebook::GRAPH_API_VERSION);
    lua_setfield(L, -2, "GRAPH_API_VERSION");
#endif

    dmFacebook::Analytics::RegisterConstants(L, MODULE_NAME);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

} // namespace

static dmExtension::Result AppInitializeFacebook(dmExtension::AppParams* params)
{
    const char* app_id = dmConfigFile::GetString(params->m_ConfigFile, "facebook.appid", 0);
    if( !app_id )
    {
        dmLogDebug("No facebook.appid. Disabling module");
        return dmExtension::RESULT_OK;
    }
    return Platform_AppInitializeFacebook(params, app_id);
}

static dmExtension::Result AppFinalizeFacebook(dmExtension::AppParams* params)
{
    return Platform_AppFinalizeFacebook(params);
}

static dmExtension::Result InitializeFacebook(dmExtension::Params* params)
{
    dmFacebook::LuaInit(params->m_L);
    dmExtension::Result r = Platform_InitializeFacebook(params);
    if (r == dmExtension::RESULT_OK)
    {
        const int autoinit = dmConfigFile::GetInt(params->m_ConfigFile, "facebook.autoinit", 1);
        if (autoinit)
        {
            dmFacebook::Facebook_Init(params->m_L);
        }
    }
    return r;
}

static dmExtension::Result FinalizeFacebook(dmExtension::Params* params)
{
    return Platform_FinalizeFacebook(params);
}

static dmExtension::Result UpdateFacebook(dmExtension::Params* params)
{
    return Platform_UpdateFacebook(params);
}

static void OnEventFacebook(dmExtension::Params* params, const dmExtension::Event* event)
{
    Platform_OnEventFacebook(params, event);
}

DM_DECLARE_EXTENSION(FacebookExtExternal, "Facebook", AppInitializeFacebook, AppFinalizeFacebook, InitializeFacebook, UpdateFacebook, OnEventFacebook, FinalizeFacebook)

#else

extern "C" void FacebookExtExternal()
{
}

#endif
