#if defined(DM_PLATFORM_IOS)

#include <dmsdk/dlib/log.h>
#include <dmsdk/script/script.h>
#include <dmsdk/extension/extension.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <FBSDKCoreKit/FBSDKCoreKit.h>
#import <FBSDKLoginKit/FBSDKLoginKit.h>
#import <FBSDKShareKit/FBSDKShareKit.h>
#import <objc/runtime.h>

#include "facebook_private.h"
#include "facebook_analytics.h"

struct Facebook
{
    Facebook() {
        memset(this, 0, sizeof(*this));
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
    }

    FBSDKLoginManager *m_Login;
    NSDictionary* m_Me;
    int m_Callback;
    int m_Self;
    int m_DisableFaceBookEvents;
    lua_State* m_MainThread;
    bool m_AccessTokenAvailable;
    bool m_AccessTokenRequested;
    id<UIApplicationDelegate,
       FBSDKSharingDelegate,
       FBSDKGameRequestDialogDelegate> m_Delegate;
};

Facebook g_Facebook;

static void UpdateUserData();
static void DoLogin();


static void RunDialogResultCallback(lua_State*L, NSDictionary* result, NSError* error);

// AppDelegate used temporarily to hijack all AppDelegate messages
// An improvment could be to create generic proxy
@interface FacebookAppDelegate : NSObject <UIApplicationDelegate, FBSDKSharingDelegate, FBSDKGameRequestDialogDelegate>

- (BOOL)application:(UIApplication *)application
                   openURL:(NSURL *)url
                   sourceApplication:(NSString *)sourceApplication
                   annotation:(id)annotation;

- (BOOL)application:(UIApplication *)application
                   didFinishLaunchingWithOptions:(NSDictionary *)launchOptions;
@end

@implementation FacebookAppDelegate
    - (BOOL)application:(UIApplication *)application
                       openURL:(NSURL *)url
                       sourceApplication:(NSString *)sourceApplication
                       annotation:(id)annotation {
        if(!g_Facebook.m_Login)
        {
            return false;
        }
        return [[FBSDKApplicationDelegate sharedInstance] application:application
                                                              openURL:url
                                                    sourceApplication:sourceApplication
                                                           annotation:annotation];
    }

    - (void)applicationDidBecomeActive:(UIApplication *)application {
        if(!g_Facebook.m_Login)
        {
            return;
        }
        if(!g_Facebook.m_DisableFaceBookEvents)
        {
            [FBSDKAppEvents activateApp];
        }

        // At the point of app activation, the currentAccessToken will be available if present.
        // If a token has been requested (through a login call), do the login at this point, or just update the userdata if logged in.
        g_Facebook.m_AccessTokenAvailable = true;
        if(g_Facebook.m_AccessTokenRequested) {
            g_Facebook.m_AccessTokenRequested = false;
            if ([FBSDKAccessToken currentAccessToken]) {
                if (![FBSDKAccessToken currentAccessToken].dataAccessExpired) {
                    UpdateUserData();
                    return;
                }
                dmLogWarning("MAWE: DATA ACCESS EXPIRED! REQUIRING RELOGIN!");
            }
            DoLogin();
        }
    }

    - (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
        if(!g_Facebook.m_Login)
        {
            return false;
        }
        return [[FBSDKApplicationDelegate sharedInstance] application:application
                                        didFinishLaunchingWithOptions:launchOptions];
    }

    // Sharing related methods
    - (void)sharer:(id<FBSDKSharing>)sharer didCompleteWithResults :(NSDictionary *)results {
        if(!g_Facebook.m_Login)
        {
            return;
        }
        if (results != nil) {
            // fix result so it complies with JS result fields
            NSMutableDictionary* new_res = [NSMutableDictionary dictionary];
            if (results[@"postId"]) {
                [new_res setValue:results[@"postId"] forKey:@"post_id"];
            }
            RunDialogResultCallback(g_Facebook.m_MainThread, new_res, 0);
        } else {
            RunDialogResultCallback(g_Facebook.m_MainThread, 0, 0);
        }
    }

    - (void)sharer:(id<FBSDKSharing>)sharer didFailWithError:(NSError *)error {
        if(!g_Facebook.m_Login)
        {
            return;
        }
        RunDialogResultCallback(g_Facebook.m_MainThread, 0, error);
    }

    - (void)sharerDidCancel:(id<FBSDKSharing>)sharer {
        if(!g_Facebook.m_Login)
        {
            return;
        }
        NSMutableDictionary *errorDetail = [NSMutableDictionary dictionary];
        [errorDetail setValue:@"Share dialog was cancelled" forKey:NSLocalizedDescriptionKey];
        RunDialogResultCallback(g_Facebook.m_MainThread, 0, [NSError errorWithDomain:@"facebook" code:0 userInfo:errorDetail]);
    }

    // Game request related methods
    - (void)gameRequestDialog:(FBSDKGameRequestDialog *)gameRequestDialog didCompleteWithResults:(NSDictionary *)results {
        if(!g_Facebook.m_Login)
        {
            return;
        }
        if (results != nil) {
            // fix result so it complies with JS result fields
            NSMutableDictionary* new_res = [NSMutableDictionary dictionaryWithDictionary:@{
                @"to" : [[NSMutableArray alloc] init]
            }];

            for (NSString* key in results) {
                if ([key hasPrefix:@"to"]) {
                    [new_res[@"to"] addObject:results[key]];
                } else {
                    [new_res setObject:results[key] forKey:key];
                }

                // Alias request_id == request,
                // want to have same result fields on both Android & iOS.
                if ([key isEqualToString:@"request"]) {
                    [new_res setObject:results[key] forKey:@"request_id"];
                }
            }
            RunDialogResultCallback(g_Facebook.m_MainThread, new_res, 0);
        } else {
            RunDialogResultCallback(g_Facebook.m_MainThread, 0, 0);
        }
    }

    - (void)gameRequestDialog:(FBSDKGameRequestDialog *)gameRequestDialog didFailWithError:(NSError *)error {
        if(!g_Facebook.m_Login)
        {
            return;
        }
        RunDialogResultCallback(g_Facebook.m_MainThread, 0, error);
    }

    - (void)gameRequestDialogDidCancel:(FBSDKGameRequestDialog *)gameRequestDialog {
        if(!g_Facebook.m_Login)
        {
            return;
        }
        NSMutableDictionary *errorDetail = [NSMutableDictionary dictionary];
        [errorDetail setValue:@"Game request dialog was cancelled" forKey:NSLocalizedDescriptionKey];
        RunDialogResultCallback(g_Facebook.m_MainThread, 0, [NSError errorWithDomain:@"facebook" code:0 userInfo:errorDetail]);
    }


@end

////////////////////////////////////////////////////////////////////////////////
// Helper functions for Lua API

static id LuaToObjC(lua_State* L, int index);
static NSArray* TableToNSArray(lua_State* L, int table_index)
{
    int top = lua_gettop(L);
    NSMutableArray* arr = [[NSMutableArray alloc] init];
    lua_pushnil(L);
    while (lua_next(L, table_index) != 0) {
        [arr addObject: LuaToObjC(L, lua_gettop(L))];
        lua_pop(L, 1);
    }

    assert(top == lua_gettop(L));
    return arr;
}

static id LuaToObjC(lua_State* L, int index)
{
    int top = lua_gettop(L);
    id r = nil;
    int actual_lua_type = lua_type(L, index);

    if (actual_lua_type == LUA_TSTRING) {
        r = [NSString stringWithUTF8String: lua_tostring(L, index)];

    } else if (actual_lua_type == LUA_TTABLE) {
        r = TableToNSArray(L, index);

    } else if (actual_lua_type == LUA_TNUMBER) {
        r = [NSNumber numberWithDouble: lua_tonumber(L, index)];

    } else if (actual_lua_type == LUA_TBOOLEAN) {
        r = [NSNumber numberWithBool:lua_toboolean(L, index)];

    } else {
        dmLogWarning("Unsupported value type '%d'", lua_type(L, index));
    }

    assert(top == lua_gettop(L));
    return r;
}

static id GetTableValue(lua_State* L, int table_index, NSArray* keys, int expected_lua_type)
{
    id r = nil;
    int top = lua_gettop(L);
    for (NSString *key in keys) {
        lua_getfield(L, table_index, [key UTF8String]);
        if (!lua_isnil(L, -1)) {

            int actual_lua_type = lua_type(L, -1);
            if (actual_lua_type != expected_lua_type) {
                dmLogError("Lua conversion expected entry '%s' to be %s but got %s",
                    [key UTF8String], lua_typename(L, expected_lua_type), lua_typename(L, actual_lua_type));
            } else {
                r = LuaToObjC(L, lua_gettop(L));
            }

        }
        lua_pop(L, 1);
    }

    assert(top == lua_gettop(L));
    return r;
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

static void RunStateCallback(lua_State*L, dmFacebook::State status, NSError* error)
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        int top = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, g_Facebook.m_Callback);
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

        lua_pushnumber(L, (lua_Number) status);
        dmFacebook::PushError(L, [error.localizedDescription UTF8String]);

        int ret = lua_pcall(L, 3, 0, 0);
        if (ret != 0) {
            dmLogError("Error running facebook callback");
        }
        assert(top == lua_gettop(L));
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Callback);
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        g_Facebook.m_Callback = LUA_NOREF;
        g_Facebook.m_Self = LUA_NOREF;
    } else {
        dmLogError("No callback set");
    }
}

static void ObjCToLua(lua_State*L, id obj)
{
    if ([obj isKindOfClass:[NSString class]]) {
        const char* str = [((NSString*) obj) UTF8String];
        lua_pushstring(L, str);
    } else if ([obj isKindOfClass:[NSNumber class]]) {
        lua_pushnumber(L, [((NSNumber*) obj) doubleValue]);
    } else if ([obj isKindOfClass:[NSNull class]]) {
        lua_pushnil(L);
    } else if ([obj isKindOfClass:[NSDictionary class]]) {
        NSDictionary* dict = (NSDictionary*) obj;
        lua_createtable(L, 0, [dict count]);
        for (NSString* key in dict) {
            lua_pushstring(L, [key UTF8String]);
            id value = [dict objectForKey:key];
            ObjCToLua(L, (NSDictionary*) value);
            lua_rawset(L, -3);
        }
    } else if ([obj isKindOfClass:[NSArray class]]) {
        NSArray* a = (NSArray*) obj;
        lua_createtable(L, [a count], 0);
        for (int i = 0; i < [a count]; ++i) {
            ObjCToLua(L, [a objectAtIndex: i]);
            lua_rawseti(L, -2, i+1);
        }
    } else {
        dmLogWarning("Unsupported value '%s' (%s)", [[obj description] UTF8String], [[[obj class] description] UTF8String]);
        lua_pushnil(L);
    }
}

static void RunDialogResultCallback(lua_State*L, NSDictionary* result, NSError* error)
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        int top = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, g_Facebook.m_Callback);
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

        ObjCToLua(L, result);

        dmFacebook::PushError(L, [error.localizedDescription UTF8String]);

        int ret = lua_pcall(L, 3, 0, 0);
        if (ret != 0) {
            dmLogError("Error running facebook callback");
        }
        assert(top == lua_gettop(L));
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Callback);
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        g_Facebook.m_Callback = LUA_NOREF;
        g_Facebook.m_Self = LUA_NOREF;
    } else {
        dmLogError("No callback set");
    }
}

static void AppendArray(lua_State*L, NSMutableArray* array, int table)
{
    lua_pushnil(L);
    while (lua_next(L, table) != 0) {
        const char* p = luaL_checkstring(L, -1);
        [array addObject: [NSString stringWithUTF8String: p]];
        lua_pop(L, 1);
    }
}

static void UpdateUserData()
{
    // Login successfull, now grab user info
    // In SDK 4+ we explicitly have to set which fields we want,
    // since earlier SDK versions returned these by default.
    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    [params setObject:@"last_name,link,id,gender,email,locale,name,first_name,updated_time" forKey:@"fields"];
    [[[FBSDKGraphRequest alloc] initWithGraphPath:@"me" parameters:params]
    startWithCompletionHandler:^(FBSDKGraphRequestConnection *connection, id graphresult, NSError *error) {

        [g_Facebook.m_Me release];
        if (!error) {
            g_Facebook.m_Me = [[NSDictionary alloc] initWithDictionary: graphresult];
            RunStateCallback(g_Facebook.m_MainThread, dmFacebook::STATE_OPEN, error);
        } else {
            g_Facebook.m_Me = nil;
            dmLogWarning("Failed to fetch user-info: %s", [[error localizedDescription] UTF8String]);
            RunStateCallback(g_Facebook.m_MainThread, dmFacebook::STATE_CLOSED_LOGIN_FAILED, error);
        }

    }];
}

static void DoLogin()
{
    NSMutableArray *permissions = [[NSMutableArray alloc] initWithObjects: @"public_profile", @"email", @"user_friends", nil];
    [g_Facebook.m_Login logInWithPermissions: permissions fromViewController:nil handler:^(FBSDKLoginManagerLoginResult *result, NSError *error) {
        if (error) {
            RunStateCallback(g_Facebook.m_MainThread, dmFacebook::STATE_CLOSED_LOGIN_FAILED, error);
        } else if (result.isCancelled) {
            NSMutableDictionary *errorDetail = [NSMutableDictionary dictionary];
            [errorDetail setValue:@"Login was cancelled" forKey:NSLocalizedDescriptionKey];
            RunStateCallback(g_Facebook.m_MainThread, dmFacebook::STATE_CLOSED_LOGIN_FAILED, [NSError errorWithDomain:@"facebook" code:0 userInfo:errorDetail]);
        } else {
            if ([result.grantedPermissions containsObject:@"public_profile"] &&
                [result.grantedPermissions containsObject:@"email"] &&
                [result.grantedPermissions containsObject:@"user_friends"]) {

                UpdateUserData();

            } else {
                // Note that the user can still be logged in at this point, but with reduced set of permissions.
                // In order to be consistent with other platforms, we consider this to be a failed login.
                NSMutableDictionary *errorDetail = [NSMutableDictionary dictionary];
                [errorDetail setValue:@"Not granted all requested permissions." forKey:NSLocalizedDescriptionKey];
                RunStateCallback(g_Facebook.m_MainThread, dmFacebook::STATE_CLOSED_LOGIN_FAILED, [NSError errorWithDomain:@"facebook" code:0 userInfo:errorDetail]);
            }
        }

    }];
}

static FBSDKDefaultAudience convertDefaultAudience(int fromLuaInt) {
    switch (fromLuaInt) {
        case 2:
            return FBSDKDefaultAudienceOnlyMe;
        case 4:
            return FBSDKDefaultAudienceEveryone;
        case 3:
        default:
            return FBSDKDefaultAudienceFriends;
    }
}

static FBSDKGameRequestActionType convertGameRequestAction(int fromLuaInt) {
    switch (fromLuaInt) {
        case 3:
            return FBSDKGameRequestActionTypeAskFor;
        case 4:
            return FBSDKGameRequestActionTypeTurn;
        case 2:
            return FBSDKGameRequestActionTypeSend;
        case 1:
        default:
            return FBSDKGameRequestActionTypeNone;
    }
}

static FBSDKGameRequestFilter convertGameRequestFilters(int fromLuaInt) {
    switch (fromLuaInt) {
        case 3:
            return FBSDKGameRequestFilterAppNonUsers;
        case 2:
            return FBSDKGameRequestFilterAppUsers;
        case 1:
        default:
            return FBSDKGameRequestFilterNone;
    }
}

static void PrepareCallback(lua_State* thread, FBSDKLoginManagerLoginResult* result, NSError* error)
{
    if (error)
    {
        RunCallback(thread, &g_Facebook.m_Self, &g_Facebook.m_Callback,
            [error.localizedDescription UTF8String], dmFacebook::STATE_CLOSED_LOGIN_FAILED);
    }
    else if (result.isCancelled)
    {
        RunCallback(thread, &g_Facebook.m_Self, &g_Facebook.m_Callback,
            "Login was cancelled", dmFacebook::STATE_CLOSED_LOGIN_FAILED);
    }
    else
    {
        RunCallback(thread, &g_Facebook.m_Self, &g_Facebook.m_Callback,
            nil, dmFacebook::STATE_OPEN);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Lua API
//

namespace dmFacebook {

bool PlatformFacebookInitialized()
{
    return !!g_Facebook.m_Login;
}

void PlatformFacebookLoginWithPermissions(lua_State* L, const char** permissions,
    uint32_t permission_count, int audience, int callback, int context, lua_State* thread)
{
    // This function must always return so memory for `permissions` can be free'd.
    VerifyCallback(L);
    g_Facebook.m_Callback = callback;
    g_Facebook.m_Self = context;

    NSMutableArray* ns_permissions = [[NSMutableArray alloc] init];
    for (uint32_t i = 0; i < permission_count; ++i)
    {
        const char* permission = permissions[i];
        [ns_permissions addObject: [NSString stringWithUTF8String: permission]];
    }

    @try {
        [g_Facebook.m_Login setDefaultAudience: convertDefaultAudience(audience)];
        [g_Facebook.m_Login logInWithPermissions: ns_permissions fromViewController:nil handler:^(FBSDKLoginManagerLoginResult *result, NSError *error) {
            PrepareCallback(thread, result, error);
        }];
    } @catch (NSException* exception) {
        NSString* errorMessage = [NSString stringWithFormat:@"Unable to request permissions: %@", exception.reason];
        RunCallback(thread, &g_Facebook.m_Self, &g_Facebook.m_Callback,
            [errorMessage UTF8String], dmFacebook::STATE_CLOSED_LOGIN_FAILED);
    }
}


int Facebook_Logout(lua_State* L)
{
    if(!g_Facebook.m_Login)
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    [g_Facebook.m_Login logOut];
    [g_Facebook.m_Me release];
    g_Facebook.m_Me = 0;
    return 0;
}

static void RunCallback(lua_State* L, NSError* error)
{
    RunCallback(L, &g_Facebook.m_Self, &g_Facebook.m_Callback, [error.localizedDescription UTF8String], -1);
}

int Facebook_AccessToken(lua_State* L)
{
    if(!g_Facebook.m_Login)
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }

    if ([FBSDKAccessToken currentAccessToken] && [FBSDKAccessToken currentAccessToken].dataAccessExpired) {
        dmLogWarning("MAWE: DATA ACCESS EXPIRED! REQUIRING RELOGIN!");
        lua_pushnil(L);
        return 1;
    }

    const char* token = [[[FBSDKAccessToken currentAccessToken] tokenString] UTF8String];
    lua_pushstring(L, token);
    return 1;
}

int Facebook_Permissions(lua_State* L)
{
    if(!g_Facebook.m_Login)
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    int top = lua_gettop(L);

    lua_newtable(L);
    NSSet* permissions = [[FBSDKAccessToken currentAccessToken] permissions];
    int i = 1;
    for (NSString* p in permissions) {
        lua_pushnumber(L, i);
        lua_pushstring(L, [p UTF8String]);
        lua_rawset(L, -3);
        ++i;
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

    // Prepare for Objective-C
    NSString* objcEvent = [NSString stringWithCString:event encoding:NSASCIIStringEncoding];
    NSMutableDictionary* params = [NSMutableDictionary dictionary];
    for (unsigned int i = 0; i < length; ++i)
    {
        NSString* objcKey = [NSString stringWithCString:keys[i] encoding:NSASCIIStringEncoding];
        NSString* objcValue = [NSString stringWithCString:values[i] encoding:NSASCIIStringEncoding];

        params[objcKey] = objcValue;
    }

    [FBSDKAppEvents logEvent:objcEvent valueToSum:valueToSum parameters:params];

    return 0;
}

int Facebook_EnableEventUsage(lua_State* L)
{
    [FBSDKSettings setLimitEventAndDataUsage:false];

    return 0;
}

int Facebook_DisableEventUsage(lua_State* L)
{
    [FBSDKSettings setLimitEventAndDataUsage:true];

    return 0;
}

int Facebook_ShowDialog(lua_State* L)
{
    if(!g_Facebook.m_Login)
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    int top = lua_gettop(L);
    VerifyCallback(L);

    dmhash_t dialog = dmHashString64(luaL_checkstring(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    g_Facebook.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);
    dmScript::GetInstance(L);
    g_Facebook.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
    g_Facebook.m_MainThread = dmScript::GetMainThread(L);

    if (dialog == dmHashString64("feed")) {

        FBSDKShareLinkContent* content = [[FBSDKShareLinkContent alloc] init];
        content.contentURL         = [NSURL URLWithString:GetTableValue(L, 2, @[@"link"], LUA_TSTRING)];
        content.peopleIDs          = GetTableValue(L, 2, @[@"people_ids"], LUA_TTABLE);
        content.placeID            = GetTableValue(L, 2, @[@"place_id"], LUA_TSTRING);
        content.ref                = GetTableValue(L, 2, @[@"ref"], LUA_TSTRING);

        [FBSDKShareDialog showFromViewController:nil withContent:content delegate:g_Facebook.m_Delegate];

    } else if (dialog == dmHashString64("apprequests") || dialog == dmHashString64("apprequest")) {

        FBSDKGameRequestContent* content = [[FBSDKGameRequestContent alloc] init];
        content.title      = GetTableValue(L, 2, @[@"title"], LUA_TSTRING);
        content.message    = GetTableValue(L, 2, @[@"message"], LUA_TSTRING);
        content.actionType = convertGameRequestAction([GetTableValue(L, 2, @[@"action_type"], LUA_TNUMBER) unsignedIntValue]);
        content.filters    = convertGameRequestFilters([GetTableValue(L, 2, @[@"filters"], LUA_TNUMBER) unsignedIntValue]);
        content.data       = GetTableValue(L, 2, @[@"data"], LUA_TSTRING);
        content.objectID   = GetTableValue(L, 2, @[@"object_id"], LUA_TSTRING);

        NSArray* suggestions = GetTableValue(L, 2, @[@"suggestions"], LUA_TTABLE);
        if (suggestions != nil) {
            content.recipientSuggestions = suggestions;
        }

        // comply with JS way of specifying recipients/to
        NSString* to = GetTableValue(L, 2, @[@"to"], LUA_TSTRING);
        if (to != nil && [to respondsToSelector:@selector(componentsSeparatedByString:)]) {
            content.recipients = [to componentsSeparatedByString:@","];
        }
        NSArray* recipients = GetTableValue(L, 2, @[@"recipients"], LUA_TTABLE);
        if (recipients != nil) {
            content.recipients = recipients;
        }

        [FBSDKGameRequestDialog showWithContent:content delegate:g_Facebook.m_Delegate];

    } else {
        NSMutableDictionary *errorDetail = [NSMutableDictionary dictionary];
        [errorDetail setValue:@"Invalid dialog type" forKey:NSLocalizedDescriptionKey];
        NSError* error = [NSError errorWithDomain:@"facebook" code:0 userInfo:errorDetail];

        lua_State* main_thread = dmScript::GetMainThread(L);
        RunDialogResultCallback(main_thread, 0, error);
    }

    assert(top == lua_gettop(L));
    return 0;
}

} // namespace

const char* Platform_GetVersion()
{
    const char* version = (const char*)[[FBSDKSettings sdkVersion] UTF8String];
    return strdup(version);
}

bool Platform_FacebookInitialized()
{
    return g_Facebook.m_Login != 0;
}

dmExtension::Result Platform_AppInitializeFacebook(dmExtension::AppParams* params, const char* app_id)
{
    g_Facebook.m_Delegate = [[FacebookAppDelegate alloc] init];
    dmExtension::RegisteriOSUIApplicationDelegate(g_Facebook.m_Delegate);

    g_Facebook.m_DisableFaceBookEvents = dmConfigFile::GetInt(params->m_ConfigFile, "facebook.disable_events", 0);

    [FBSDKSettings setAppID: [NSString stringWithUTF8String: app_id]];

    g_Facebook.m_Login = [[FBSDKLoginManager alloc] init];

    return dmExtension::RESULT_OK;
}


dmExtension::Result Platform_AppFinalizeFacebook(dmExtension::AppParams* params)
{
    if(!g_Facebook.m_Login)
    {
        return dmExtension::RESULT_OK;
    }

    dmExtension::UnregisteriOSUIApplicationDelegate(g_Facebook.m_Delegate);
    [g_Facebook.m_Login release];
    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_InitializeFacebook(dmExtension::Params* params)
{
    (void)params;
    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_UpdateFacebook(dmExtension::Params* params)
{
    (void)params;
    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_FinalizeFacebook(dmExtension::Params* params)
{
    (void)params;
    return dmExtension::RESULT_OK;
}

void Platform_OnEventFacebook(dmExtension::Params* params, const dmExtension::Event* event)
{
    (void)params; (void)event;
}

#endif // DM_PLATFORM_IOS
