package com.defold.facebook;

import java.net.URLEncoder;

import java.util.Map;
import java.util.Set;
import java.util.Iterator;
import java.util.Collection;
import java.util.Arrays;
import java.util.concurrent.CountDownLatch;

import android.app.Activity;
import android.util.Log;
import android.os.Build;
import android.os.Bundle;

import com.facebook.FacebookSdk;
import com.facebook.appevents.AppEventsLogger;
import com.facebook.login.DefaultAudience;
import com.facebook.applinks.AppLinkData;
import com.facebook.internal.BundleJSONConverter;

import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONArray;

import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;

// A helper class that initializes the facebook sdk, and also activates/deactivates the app
class FacebookAppJNI {

    private static final String TAG = "defold.facebook";

    private Activity activity;
    private String appId;

    // fb sdk patch for API < 11 support
    // Custom Executor for FB SDK
    private static final Object LOCK = new Object();
    // This code is copied from the android.os.AsyncTask implementation to set up the executor with the same parameters
    // as the one introduced in API 11
    private static final int CPU_COUNT = Runtime.getRuntime().availableProcessors();
    private static final int CORE_POOL_SIZE = CPU_COUNT + 1;
    private static final int MAXIMUM_POOL_SIZE = CPU_COUNT * 2 + 1;
    private static final int KEEP_ALIVE = 1;

    private static final ThreadFactory sThreadFactory = new ThreadFactory() {
        private final AtomicInteger mCount = new AtomicInteger(1);
        public Thread newThread(Runnable r) {
            return new Thread(r, "AsyncTask #" + mCount.getAndIncrement());
        }
    };
    private static final BlockingQueue<Runnable> sPoolWorkQueue = new LinkedBlockingQueue<Runnable>(128);

    private static Executor customFacebookExecutor;

    private static Executor getCustomExecutor() {
        synchronized (LOCK) {
            if (customFacebookExecutor == null) {
                customFacebookExecutor = new ThreadPoolExecutor(CORE_POOL_SIZE, MAXIMUM_POOL_SIZE, KEEP_ALIVE,
                        TimeUnit.SECONDS, sPoolWorkQueue, sThreadFactory);
            }
        }
        return customFacebookExecutor;
    }

    public FacebookAppJNI(Activity activity, String appId) {
        this.activity = activity;
        this.appId = appId;

        final CountDownLatch latch = new CountDownLatch(1);

        // Workaround for old devices (pre API 11)
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB) {
            FacebookSdk.setExecutor(getCustomExecutor());
        }

        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                String s = String.format("sdkInitialize: activity %s   appid: %s", FacebookAppJNI.this.activity, FacebookAppJNI.this.appId);
                Log.d(TAG, s);
                FacebookSdk.setApplicationId( FacebookAppJNI.this.appId );
                FacebookSdk.sdkInitialize( FacebookAppJNI.this.activity );
                latch.countDown();
            }
        });

        try {
            latch.await();
        } catch (InterruptedException ex) {
        }
    }

    public void activate() {
        String s = String.format("activateApp: activity %s   appid: %s", this.activity, this.appId);
        Log.d(TAG, s);
        AppEventsLogger.activateApp(this.activity, this.appId);
    }

    public void deactivate() {
        String s = String.format("deactivateApp: activity %s   appid: %s", this.activity, this.appId);
        Log.d(TAG, s);
        AppEventsLogger.deactivateApp(this.activity, this.appId);
    }
}

class FacebookJNI {

    private static final String TAG = "defold.facebook";

    private native void onLoginWithPermissions(long userData, int state, String error);

    private native void onDialogComplete(long userData, String results, String error);

    private native void onIterateMeEntry(long userData, String key, String value);

    private native void onIteratePermissionsEntry(long userData, String permission);

    private native void onFetchDeferredAppLinkData(long userData, String results, Boolean isError);

    // JSONObject.wrap not available in Java 7,
    // Using https://android.googlesource.com/platform/libcore/+/master/json/src/main/java/org/json/JSONObject.java
    private static Object JSONWrap(Object o) {
        if (o == null) {
            return null;
        }
        if (o instanceof JSONArray || o instanceof JSONObject) {
            return o;
        }
        if (o.equals(null)) {
            return o;
        }
        try {
            if (o instanceof Collection) {
                return new JSONArray((Collection) o);
            } else if (o.getClass().isArray()) {
                return new JSONArray(Arrays.asList((Object[])o));
            }
            if (o instanceof Map) {
                return new JSONObject((Map) o);
            }
            if (o instanceof Boolean ||
                o instanceof Byte ||
                o instanceof Character ||
                o instanceof Double ||
                o instanceof Float ||
                o instanceof Integer ||
                o instanceof Long ||
                o instanceof Short ||
                o instanceof String) {
                return o;
            }
            if (o.getClass().getPackage().getName().startsWith("java.")) {
                return o.toString();
            }
        } catch (Exception ignored) {
        }
        return null;
    }

    // From http://stackoverflow.com/questions/21858528/convert-a-bundle-to-json
    private static JSONObject bundleToJson(final Bundle in) throws JSONException {
        JSONObject json = new JSONObject();
        if (in == null) {
            return json;
        }
        Set<String> keys = in.keySet();
        for (String key : keys) {
            json.put(key, JSONWrap(in.get(key)));
        }

        return json;
    }

    private Facebook facebook;
    private Activity activity;

    public FacebookJNI(Activity activity, String appId) {
        this.activity = activity;
        this.facebook = new Facebook(this.activity, appId);
    }

    public void logout() {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                FacebookJNI.this.facebook.logout();
            }
        });
    }

    public void iteratePermissions(final long userData) {
        for (String permission : this.facebook.getPermissions()) {
            onIteratePermissionsEntry(userData, permission);
        }
    }

    public String getAccessToken() {
        return this.facebook.getAccessToken();
    }

    public String getSdkVersion() {
        return this.facebook.getSdkVersion();
    }

    public void loginWithPermissions(final long userData, final int audience, final String permissions) {
        this.activity.runOnUiThread(new Runnable() {

            @Override
            public void run() {
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB) {
                    // call cb immediately with failed state
                    onLoginWithPermissions(userData, 6, "Not supported, Android SDK too old.");
                } else {
                    Facebook.LoginCallback callback = new Facebook.LoginCallback() {

                        @Override
                        public void onDone(final int state, final String error) {
                            onLoginWithPermissions(userData, state, error);
                        }

                    };

                    facebook.loginWithPermissions(permissions.split(","), audience, callback);
                }
            }

        });
    }

    public void showDialog(final long userData, final String dialogType, final String paramsJson) {

        this.activity.runOnUiThread(new Runnable() {

            @Override
            public void run() {
                Facebook.DialogCallback cb = new Facebook.DialogCallback() {
                    @Override
                    public void onDone(final Bundle result, final String error) {

                        // serialize bundle as JSON for C/Lua side
                        String res = null;
                        String err = error;
                        try {
                            JSONObject obj = bundleToJson(result);
                            res = obj.toString();
                        } catch(JSONException e) {
                            err = "Error while converting dialog result to JSON: " + e.getMessage();
                        }

                        onDialogComplete(userData, res, err);
                    }
                };

                // Parse dialog params from JSON and put into Bundle
                Bundle params = new Bundle();
                JSONObject o = null;
                try {
                    o = new JSONObject(paramsJson);
                    Iterator iter = o.keys();
                    while (iter.hasNext()) {
                        String key = iter.next().toString();
                        String value = o.get(key).toString();
                        params.putString(key, value);
                    }
                } catch (Exception e) {
                    String err = "Failed to get json value";
                    Log.wtf(TAG, err, e);
                    onDialogComplete(userData, null, err);
                    return;
                }

                facebook.showDialog(dialogType, params, cb);

            }
        });

    }

    public void postEvent(String event, double valueToSum, String[] keys, String[] values) {
        facebook.postEvent(event, valueToSum, keys, values);
    }

    public void enableEventUsage() {
        facebook.disableEventUsage();
    }

    public void disableEventUsage() {
        facebook.enableEventUsage();
    }

    public void fetchDeferredAppLinkData(final long userData) {
        AppLinkData.fetchDeferredAppLinkData(this.activity,
            new AppLinkData.CompletionHandler() {
                @Override
                public void onDeferredAppLinkDataFetched(AppLinkData appLinkData) {
                    String message = null;
                    Boolean isError = false;
                    JSONObject data;
                    try {
                        if (appLinkData == null) {
                            message ="{}";
                        } else {
                            data = new JSONObject();
                            data.put("ref", appLinkData.getRef());
                            data.put("target_url", appLinkData.getTargetUri().toString());
                            if (appLinkData.getArgumentBundle() != null) {
                                data.put("extras", BundleJSONConverter.convertToJSON(appLinkData.getArgumentBundle()));
                            }
                            message = data.toString();
                        }
                    } catch (JSONException e) {
                        isError = true;
                        message = "'error':'Error while converting DeferredAppLinkData to JSON:" + e.getMessage()+"'";
                    }
                    onFetchDeferredAppLinkData(userData, message, isError);
                }
            });
    }

}
