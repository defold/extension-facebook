#!/usr/bin/env bash

VERSION=v6.5.1
FILE=FacebookSDK_Static.zip
URL=https://github.com/facebook/facebook-ios-sdk/releases/download/$VERSION/$FILE
TMP=tmpSdk

wget $URL

unzip $FILE -d $TMP

cp -v -r $TMP/FBSDKCoreKit.framework ../../lib/ios/
cp -v -r $TMP/FBSDKLoginKit.framework ../../lib/ios/
cp -v -r $TMP/FBSDKShareKit.framework ../../lib/ios/

rm $FILE
rm -rf $TMP
