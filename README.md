# Facebook extension

### TODO:

- [x] Android resources
- [ ] libraries
- [x] build.yml
- [ ] Info.plist stub
- [ ] stub implementation
- [x] Facebook IAP we ignore for now.

#### HTML5
- [ ] ...

#### Android

- [ ] AndroidManifest stub

https://developer.android.com/studio/build/manifest-merge

Extra packages:
-            resourceDirectories.add(Bob.getPath("res/facebook"));
-            extraPackages.add("com.facebook");
-        args.add("--extra-packages");
-        args.add("com.facebook:com.google.android.gms");
-        args.add("-S"); args.add(Bob.getPath("res/facebook"));

#### iOS
- [ ] PlistMerge function (also see the merge heuristics [here](https://developer.android.com/studio/build/manifest-merge#merge_conflict_heuristics))

# build.yml

- [x] remove facebook related jar files
- [x] remove Html libraries

# Dependencies in Bob

$ rg facebook
	src/com/dynamo/bob/Project.java
	509:            // Include built-in/default facebook and gms resources
	510:            resourceDirectories.add(Bob.getPath("res/facebook"));
	512:            extraPackages.add("com.facebook");

	src/com/dynamo/bob/bundle/AndroidBundler.java
	192:        args.add("com.facebook:com.google.android.gms");
	204:        args.add("-S"); args.add(Bob.getPath("res/facebook"));

src/com/dynamo/bob/bundle/HTML5Bundler.java
219:        // Check if game has configured a Facebook App ID
220:        String facebookAppId = projectProperties.getStringValue("facebook", "appid", null);
221:        infoData.put("DEFOLD_HAS_FACEBOOK_APP_ID", facebookAppId != null ? "true" : "false");

src/com/dynamo/bob/bundle/IOSBundler.java
246:        String facebookAppId = projectProperties.getStringValue("facebook", "appid", null);
247:        if (facebookAppId != null) {
248:            urlSchemes.add("fb" + facebookAppId);

# Info.plist

        <key>NSAppTransportSecurity</key>
        <dict>
            <key>NSExceptionDomains</key>
            <dict>
                <key>facebook.com</key>
                <dict>
                    <key>NSIncludesSubdomains</key>
                    <true/>
                    <key>NSThirdPartyExceptionRequiresForwardSecrecy</key>
                    <false/>
                </dict>
                <key>fbcdn.net</key>
                <dict>
                    <key>NSIncludesSubdomains</key>
                    <true/>
                    <key>NSThirdPartyExceptionRequiresForwardSecrecy</key>
                    <false/>
                </dict>
                <key>akamaihd.net</key>
                <dict>
                    <key>NSIncludesSubdomains</key>
                    <true/>
                    <key>NSThirdPartyExceptionRequiresForwardSecrecy</key>
                    <false/>
                </dict>
            </dict>
        </dict>

We should be able to use PListBuddy (also on Linux) to merge several Info.plist.
E.g. merge them from Left to right, where the base plist is the defold default one.

https://marcosantadev.com/manage-plist-files-plistbuddy/

/usr/libexec/PlistBuddy (on macOS)

Merges from and to, into to:
https://stackoverflow.com/a/36607504/468516

	/usr/libexec/PlistBuddy -x -c "Merge from.plist" to.plist

NOTE: PlistBuddy doesn't merge the way we wish (e.g. complains about duplicate keys)
and the interface is a bit off.

DECISION: We write a PListMerge function in IOSBundler.java using XMLPropertyListConfiguration
