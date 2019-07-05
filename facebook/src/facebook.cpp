#if defined(DM_PLATFORM_ANDROID) || defined(DM_PLATFORM_IOS) || defined(DM_PLATFORM_HTML5)

#include "facebook_private.h"
#include "facebook_analytics.h"
#include <dmsdk/sdk.h>
#include <dmsdk/dlib/log.h>

#define MODULE_NAME "facebook"  // the c++ module name from ext.manifest, also used as the Lua module name

namespace dmFacebook {

static int Facebook_LoginWithPermissions(lua_State* L)
{
    if (!Platform_FacebookInitialized())
    {
        return luaL_error(L, "Facebook module has not been initialized, is facebook.appid set in game.project?");
    }

    DM_LUA_STACK_CHECK(L, 0);
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TNUMBER);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    char* permissions[128];
    int permission_count = luaTableToCArray(L, 1, permissions, sizeof(permissions) / sizeof(permissions[0]));
    if (permission_count == -1)
    {
        return luaL_error(L, "Facebook permissions must be strings");
    }

    int audience = luaL_checkinteger(L, 2);

    lua_pushvalue(L, 3);
    int callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    int context = dmScript::Ref(L, LUA_REGISTRYINDEX);

    lua_State* thread = dmScript::GetMainThread(L);

    PlatformFacebookLoginWithPermissions(L, (const char**) permissions, permission_count, audience, callback, context, thread);

    for (int i = 0; i < permission_count; ++i) {
        free(permissions[i]);
    }

    return 0;
}

static int Facebook_GetVersion(lua_State* L)
{
    const char* version = Platform_GetVersion();
    if (!version)
        return luaL_error(L, "get_version not supported");
    lua_pushstring(L, version);
    free((void*)version);
    return 1;
}

static const luaL_reg Facebook_methods[] =
{
    {"get_version", Facebook_GetVersion},
    {"logout", Facebook_Logout},
    {"access_token", Facebook_AccessToken},
    {"permissions", Facebook_Permissions},
    {"post_event", Facebook_PostEvent},
    {"enable_event_usage", Facebook_EnableEventUsage},
    {"disable_event_usage", Facebook_DisableEventUsage},
    {"show_dialog", Facebook_ShowDialog},
    {"login_with_permissions", Facebook_LoginWithPermissions},
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
    return Platform_InitializeFacebook(params);
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

