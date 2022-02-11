#if defined(DM_PLATFORM_IOS)

#include <dmsdk/dlib/hash.h>
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
#include "facebook_util.h"
#include "facebook_analytics.h"

@class FacebookAppDelegate;

struct Facebook
{
    Facebook() {
        memset(this, 0, sizeof(*this));
    }

    FBSDKLoginManager*          m_Login;
    dmFacebook::CommandQueue    m_CommandQueue;
    FacebookAppDelegate*        m_Delegate;
    int                         m_DisableFaceBookEvents;
    const char*                 m_AppId;

};

Facebook g_Facebook;

static const char* ObjCToJson(id obj);
static void PushJsonCommand(dmScript::LuaCallbackInfo* callback, const char* result, const char* error);

// AppDelegate used temporarily to hijack all AppDelegate messages
// An improvment could be to create generic proxy
@interface FacebookAppDelegate : NSObject <UIApplicationDelegate, FBSDKSharingDelegate, FBSDKGameRequestDialogDelegate>
@property dmScript::LuaCallbackInfo* m_Callback;

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
    }

    - (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
        [FBSDKApplicationDelegate initializeSDK:launchOptions];
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

            PushJsonCommand(self.m_Callback, ObjCToJson(new_res), 0);
        } else {
            PushJsonCommand(self.m_Callback, 0, 0);
        }
    }

    - (void)sharer:(id<FBSDKSharing>)sharer didFailWithError:(NSError *)error {
        if(!g_Facebook.m_Login)
        {
            return;
        }
        PushJsonCommand(self.m_Callback, 0, strdup([error.localizedDescription UTF8String]));
    }

    - (void)sharerDidCancel:(id<FBSDKSharing>)sharer {
        if(!g_Facebook.m_Login)
        {
            return;
        }
        NSMutableDictionary *errorDetail = [NSMutableDictionary dictionary];
        [errorDetail setValue:@"Share dialog was cancelled" forKey:NSLocalizedDescriptionKey];
        NSError* error = [NSError errorWithDomain:@"facebook" code:0 userInfo:errorDetail];
        PushJsonCommand(self.m_Callback, 0, strdup([error.localizedDescription UTF8String]));
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
            PushJsonCommand(self.m_Callback, ObjCToJson(new_res), 0);
        } else {
            PushJsonCommand(self.m_Callback, 0, 0);
        }
    }

    - (void)gameRequestDialog:(FBSDKGameRequestDialog *)gameRequestDialog didFailWithError:(NSError *)error {
        if(!g_Facebook.m_Login)
        {
            return;
        }
        PushJsonCommand(self.m_Callback, 0, strdup([error.localizedDescription UTF8String]));
    }

    - (void)gameRequestDialogDidCancel:(FBSDKGameRequestDialog *)gameRequestDialog {
        if(!g_Facebook.m_Login)
        {
            return;
        }
        NSMutableDictionary *errorDetail = [NSMutableDictionary dictionary];
        [errorDetail setValue:@"Game request dialog was cancelled" forKey:NSLocalizedDescriptionKey];
        NSError* error = [NSError errorWithDomain:@"facebook" code:0 userInfo:errorDetail];
        PushJsonCommand(self.m_Callback, 0, strdup([error.localizedDescription UTF8String]));
    }


@end


struct FacebookAppDelegateRegister
{
    FacebookAppDelegateRegister() {
        g_Facebook.m_Delegate = [[FacebookAppDelegate alloc] init];
        dmExtension::RegisteriOSUIApplicationDelegate(g_Facebook.m_Delegate);
    }

    ~FacebookAppDelegateRegister() {
        dmExtension::UnregisteriOSUIApplicationDelegate(g_Facebook.m_Delegate);
        [g_Facebook.m_Delegate release];
    }
};

FacebookAppDelegateRegister g_FacebookDelegateRegister;


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

// Caller will own the buffer, and must call free() on it!
static const char* ObjCToJson(id obj)
{
    NSError* error;
    NSData* jsonData = [NSJSONSerialization dataWithJSONObject:obj options:(NSJSONWritingOptions)0 error:&error];
    if (!jsonData)
    {
        return 0;
    }
    NSString* nsstring = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
    const char* json = strdup([nsstring UTF8String]);
    [nsstring release];
    return json;
}

static void PushJsonCommand(dmScript::LuaCallbackInfo* callback, const char* result, const char* error)
{
    dmFacebook::FacebookCommand cmd;
    cmd.m_Callback = callback;
    cmd.m_Results = result;
    cmd.m_Error = error;
    cmd.m_Type = dmFacebook::COMMAND_TYPE_DIALOG_COMPLETE; // will invoke the json callback path
    dmFacebook::QueuePush(&g_Facebook.m_CommandQueue, &cmd);
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

static void LoginCallback(dmScript::LuaCallbackInfo* callback, FBSDKLoginManagerLoginResult* result, NSError* error)
{
    if (error)
    {
        dmFacebook::RunStatusCallback(callback, [error.localizedDescription UTF8String], dmFacebook::STATE_CLOSED_LOGIN_FAILED);
    }
    else if (result.isCancelled)
    {
        dmFacebook::RunStatusCallback(callback, "Login was cancelled", dmFacebook::STATE_CLOSED_LOGIN_FAILED);
    }
    else
    {
        dmFacebook::RunStatusCallback(callback, 0, dmFacebook::STATE_OPEN);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Lua API
//

int Platform_FacebookLoginWithPermissions(lua_State* L, const char** permissions,
    uint32_t permission_count, int audience, dmScript::LuaCallbackInfo* callback)
{
    // This function must always return so memory for `permissions` can be free'd.
    g_Facebook.m_Delegate.m_Callback = callback;

    // Check if there already is a access token, and if it has expired.
    // In such case we want to reautorize instead of doing a new login.
    if ([FBSDKAccessToken currentAccessToken]) {
        if ([FBSDKAccessToken currentAccessToken].dataAccessExpired) {
            [g_Facebook.m_Login reauthorizeDataAccess:nil handler:^(FBSDKLoginManagerLoginResult *result, NSError *error) {
                LoginCallback(callback, result, error);
            }];
            return 0;
        }
    }

    NSMutableArray* ns_permissions = [[NSMutableArray alloc] init];
    for (uint32_t i = 0; i < permission_count; ++i)
    {
        const char* permission = permissions[i];
        [ns_permissions addObject: [NSString stringWithUTF8String: permission]];
    }

    @try {
        [g_Facebook.m_Login setDefaultAudience: convertDefaultAudience(audience)];
        [g_Facebook.m_Login logInWithPermissions: ns_permissions fromViewController:nil handler:^(FBSDKLoginManagerLoginResult *result, NSError *error) {
            LoginCallback(callback, result, error);
        }];
    } @catch (NSException* exception) {
        NSString* errorMessage = [NSString stringWithFormat:@"Unable to request permissions: %@", exception.reason];
        dmFacebook::RunStatusCallback(callback, [errorMessage UTF8String], dmFacebook::STATE_CLOSED_LOGIN_FAILED);
    }
    return 0;
}

int Platform_FetchDeferredAppLinkData(lua_State* L, dmScript::LuaCallbackInfo* callback)
{
    g_Facebook.m_Delegate.m_Callback = callback;
    [FBSDKAppLinkUtility fetchDeferredAppLink:^(NSURL *url, NSError *error) {
        const char* errorMsg = 0;
        const char* result = 0;
        if (error) {
            NSString *errorMessage =
                error.userInfo[FBSDKErrorLocalizedDescriptionKey] ?:
                error.userInfo[FBSDKErrorDeveloperMessageKey] ?:
                error.localizedDescription;
            errorMsg = (char*)[errorMessage UTF8String];
        } else {
            NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
            if (url.absoluteString) {
                [dict setObject:url.absoluteString forKey:@"ref"];
                FBSDKURL *parsedUrl = [FBSDKURL URLWithInboundURL:url sourceApplication:nil];
                if (parsedUrl) {
                    if (parsedUrl.appLinkExtras) {
                        [dict setObject:parsedUrl.appLinkExtras forKey:@"extras"];
                    }

                    if (parsedUrl.targetURL) {
                        [dict setObject:parsedUrl.targetURL.absoluteString forKey:@"target_url"];
                    }
                }

                NSError * err;
                NSData * jsonData = [NSJSONSerialization  dataWithJSONObject:dict options:0 error:&err];
                if (!jsonData && err) {
                    errorMsg = (char*)[err.localizedDescription UTF8String];
                } else {
                    NSString * jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
                    result = (char*)[jsonString UTF8String];
                }
            } else {
                result = "{}";
            }
        }
        if (errorMsg) {
            errorMsg = (char*)[NSString stringWithFormat:@"{'error':'%s'}", errorMsg];
        }
        PushJsonCommand(callback, result?strdup(result):0, errorMsg?strdup(errorMsg):0);
    }];
    return 0;
}

int Platform_FacebookLogout(lua_State* L)
{
    if(!g_Facebook.m_Login)
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    [g_Facebook.m_Login logOut];
    return 0;
}

int Platform_FacebookAccessToken(lua_State* L)
{
    if(!g_Facebook.m_Login)
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }

    if ([FBSDKAccessToken currentAccessToken] && [FBSDKAccessToken currentAccessToken].dataAccessExpired) {
        lua_pushnil(L);
        return 1;
    }

    const char* token = [[[FBSDKAccessToken currentAccessToken] tokenString] UTF8String];
    lua_pushstring(L, token);
    return 1;
}

int Platform_FacebookPermissions(lua_State* L)
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

int Platform_FacebookEnableEventUsage(lua_State* L)
{
    [FBSDKSettings setLimitEventAndDataUsage:false];

    return 0;
}

int Platform_FacebookDisableEventUsage(lua_State* L)
{
    [FBSDKSettings setLimitEventAndDataUsage:true];

    return 0;
}

int Platform_EnableAdvertiserTracking(lua_State* L)
{
    [FBSDKSettings setAdvertiserTrackingEnabled :true];

    return 0;
}

int Platform_DisableAdvertiserTracking(lua_State* L)
{
    [FBSDKSettings setAdvertiserTrackingEnabled :false];

    return 0;
}

int Platform_FacebookShowDialog(lua_State* L)
{
    int top = lua_gettop(L);

    dmhash_t dialog = dmHashString64(luaL_checkstring(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    dmScript::LuaCallbackInfo* callback = dmScript::CreateCallback(L, 3);
    g_Facebook.m_Delegate.m_Callback = callback;

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

        PushJsonCommand(callback, 0, strdup([error.localizedDescription UTF8String]));
    }

    assert(top == lua_gettop(L));
    return 0;
}

const char* Platform_GetVersion()
{
    const char* version = (const char*)[[FBSDKSettings sdkVersion] UTF8String];
    return strdup(version);
}

bool Platform_FacebookInitialized()
{
    return g_Facebook.m_Login != 0;
}


int Platform_FacebookInit(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    g_Facebook.m_Login = [[FBSDKLoginManager alloc] init];
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
    if(g_Facebook.m_Login)
    {
        [g_Facebook.m_Login release];
        g_Facebook.m_Login = nil;
    }

    dmFacebook::QueueDestroy(&g_Facebook.m_CommandQueue);
    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_InitializeFacebook(dmExtension::Params* params)
{
    (void)params;
    return dmExtension::RESULT_OK;
}

dmExtension::Result Platform_UpdateFacebook(dmExtension::Params* params)
{
    dmFacebook::QueueFlush(&g_Facebook.m_CommandQueue, dmFacebook::HandleCommand, (void*)params->m_L);
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
