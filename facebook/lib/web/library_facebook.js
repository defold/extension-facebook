var LibraryFacebook = {
        $FBinner: {
            loginTimestamp: -1,
            needsReauth: false
        },

        dmFacebookInitialize: function(app_id, version) {
            // We assume that the Facebook javascript SDK is loaded by now.
            // This should be done via a script tag (synchronously) in the html page:
            // <script type="text/javascript" src="//connect.facebook.net/en_US/sdk.js"></script>
            // This script tag MUST be located before the engine (game) js script tag.
            try {
                FB.init({
                    appId      : UTF8ToString(app_id),
                    status     : false,
                    xfbml      : false,
                    version    : UTF8ToString(version),
                });

                window._dmFacebookUpdatePermissions = function(callback) {
                    try {
                        FB.api('/me/permissions', function (response) {
                            var e = (response && response.error ? response.error.message : 0);
                            if(e == 0 && response.data) {
                                var permissions = [];
                                for (var i=0; i<response.data.length; i++) {
                                    if(response.data[i].permission && response.data[i].status) {
                                        if(response.data[i].status === 'granted') {
                                            permissions.push(response.data[i].permission);
                                        } else if(response.data[i].status === 'declined') {
                                            // TODO: Handle declined permissions?
                                        }
                                    }
                                }
                                // Just make json of the acutal permissions (array)
                                var permissions_data = JSON.stringify(permissions);
                                callback(0, permissions_data);
                            } else {
                                callback(e, 0);
                            }
                        });
                    } catch (e){
                        console.error("Facebook permissions failed " + e);
                    }
                };

            } catch (e){
                console.error("Facebook initialize failed " + e);
            }
        },

        // https://developers.facebook.com/docs/reference/javascript/FB.getAuthResponse/
        dmFacebookAccessToken: function(callback, lua_state) {
            try {
                var response = FB.getAuthResponse(); // Cached??
                var access_token = (response && response.accessToken ? response.accessToken : 0);

                if(access_token != 0) {

                    // Check if data access has expired
                    if (FBinner.loginTimestamp > 0) {
                        var currentTimeStamp = (!Date.now ? +new Date() : Date.now());
                        var delta = (currentTimeStamp - FBinner.loginTimestamp) / 1000;
                        if (delta >= response.reauthorize_required_in) {
                            FBinner.needsReauth = true;
                            dynCall('vii', callback, [lua_state, 0]);
                            return;
                        }
                    }

                    var buf = allocate(intArrayFromString(access_token), 'i8', ALLOC_STACK);
                    dynCall('vii', callback, [lua_state, buf]);
                } else {
                    dynCall('vii', callback, [lua_state, 0]);
                }
            } catch (e){
                console.error("Facebook access token failed " + e);
            }
        },

        // https://developers.facebook.com/docs/javascript/reference/FB.ui
        dmFacebookShowDialog: function(params, mth, callback, lua_state) {
            var par = JSON.parse(UTF8ToString(params));
            par.method = UTF8ToString(mth);

            try {
                FB.ui(par, function(response) {
                    // https://developers.facebook.com/docs/graph-api/using-graph-api/v2.0
                    //   (Section 'Handling Errors')
                    var e = (response && response.error ? response.error.message : 0);
                    if(e == 0) {
                        var res_data = JSON.stringify(response);
                        var res_buf = allocate(intArrayFromString(res_data), 'i8', ALLOC_STACK);
                        dynCall('viii', callback, [lua_state, res_buf, e]);
                    } else {
                        var error = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                        dynCall('viii', callback, [lua_state, 0, error]);
                    }
                });
            } catch (e) {
                console.error("Facebook show dialog failed " + e);
            }
        },

        // https://developers.facebook.com/docs/reference/javascript/FB.AppEvents.LogEvent
        dmFacebookPostEvent: function(event, value_to_sum, keys, values) {
            var params = {};
            try {
                event = UTF8ToString(event);
                keys = JSON.parse(UTF8ToString(keys));
                values = JSON.parse(UTF8ToString(values));
                for (var i = 0; i < keys.length; ++i) {
                    params[keys[i]] = values[i];
                }
            } catch (e) {
                console.error("Unable to parse data from Defold: " + e);
            }

            try {
                FB.AppEvents.logEvent(event, value_to_sum, params);
            } catch (e) {
                console.error("Unable to post event to Facebook Analytics: " + e);
            }
        },

        // https://developers.facebook.com/docs/javascript/reference/v2.0
        dmFacebookEnableEventUsage: function() {
            console.error("Limiting Facebook Analytics is not supported for Canvas");
        },

        // https://developers.facebook.com/docs/javascript/reference/v2.0
        dmFacebookDisableEventUsage: function() {
            console.error("Limiting Facebook Analytics is not supported for Canvas");
        },

        dmFacebookDoLogout: function() {
            try {
                FB.logout(function(response) {
                    // user is now logged out
                });
            } catch (e){
                console.error("Facebook logout failed " + e);
            }
        },

        dmFacebookLoginWithPermissions: function(
            state_open, state_closed, state_failed, permissions, callback, thread) {
            try {
                var opts = {scope: UTF8ToString(permissions)};
                if (FBinner.needsReauth) {
                    opts.auth_type = "reauthorize";
                    FBinner.needsReauth = false;
                }

                FB.login(function(response) {
                    var error = response && response.error ? response.error.message : 0;
                    if (error == 0 && response.authResponse) {

                        FBinner.loginTimestamp = (!Date.now ? +new Date() : Date.now());

                        window._dmFacebookUpdatePermissions(function (_error, _permissions) {
                            if (_error == 0) {
                                var permissionsbuf = allocate(intArrayFromString(_permissions), 'i8', ALLOC_STACK);
                                dynCall('viiii', callback, [thread, state_open, 0, permissionsbuf]);
                            } else {
                                var errbuf = allocate(intArrayFromString(_error), 'i8', ALLOC_STACK);
                                dynCall('viiii', callback, [thread, state_failed, errbuf, 0]);
                            }
                        });
                    } else if (error != 0) {
                        var errbuf = allocate(intArrayFromString(error), 'i8', ALLOC_STACK);
                        dynCall('viiii', callback, [thread, state_closed, errbuf, 0]);
                    } else {
                        var errmsg = "Login was cancelled";
                        var errbuf = allocate(intArrayFromString(errmsg), 'i8', ALLOC_STACK);
                        dynCall('viiii', callback, [thread, state_failed, errbuf, 0]);
                    }
                }, opts);
            } catch (error) {
                console.error("An unexpected error occurred during Facebook JavaScript interaction: " + error);
            }
        }
}

autoAddDeps(LibraryFacebook, '$FBinner');
mergeInto(LibraryManager.library, LibraryFacebook);
