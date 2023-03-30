#!/usr/bin/env bash

VERSION=v14.1.0
FILE=FacebookSDK-Static_XCFramework.zip
URL=https://github.com/facebook/facebook-ios-sdk/releases/download/$VERSION/$FILE
TMP=tmpSdk

wget $URL

unzip $FILE -d $TMP

cp -v -r $TMP/XCFrameworks/FBSDKCoreKit.xcframework/ios-arm64/FBSDKCoreKit.framework ../../lib/ios/
cp -v -r $TMP/XCFrameworks/FBSDKLoginKit.xcframework/ios-arm64/FBSDKLoginKit.framework ../../lib/ios/
cp -v -r $TMP/XCFrameworks/FBSDKShareKit.xcframework/ios-arm64/FBSDKShareKit.framework ../../lib/ios/
cp -v -r $TMP/XCFrameworks/FBSDKGamingServicesKit.xcframework/ios-arm64/FBSDKGamingServicesKit.framework ../../lib/ios/

rm $FILE
rm -rf $TMP

# These are the ones that XCode resolved and put into the Frameworks folder when using the above mentioned frameworks
SDK=${DYNAMO_HOME}/ext/SDKs/XcodeDefault12.1.xctoolchain
DST=../../res/ios/Frameworks

mkdir -p $DST

# previously used #${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftAVFoundation.dylib -r -o ${DST}/libswiftAVFoundation.dylib
# to copy the stripped versions. However, since these are later copied to the /SwiftSupport folder, they need to be unmodified
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftAVFoundation.dylib ${DST}/libswiftAVFoundation.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftAccelerate.dylib ${DST}/libswiftAccelerate.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCore.dylib ${DST}/libswiftCore.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCoreAudio.dylib ${DST}/libswiftCoreAudio.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCoreData.dylib ${DST}/libswiftCoreData.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCoreFoundation.dylib ${DST}/libswiftCoreFoundation.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCoreGraphics.dylib ${DST}/libswiftCoreGraphics.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCoreImage.dylib ${DST}/libswiftCoreImage.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCoreLocation.dylib ${DST}/libswiftCoreLocation.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCoreMedia.dylib ${DST}/libswiftCoreMedia.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftDarwin.dylib ${DST}/libswiftDarwin.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftDispatch.dylib ${DST}/libswiftDispatch.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftFoundation.dylib ${DST}/libswiftFoundation.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftMetal.dylib ${DST}/libswiftMetal.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftObjectiveC.dylib ${DST}/libswiftObjectiveC.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftPhotos.dylib ${DST}/libswiftPhotos.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftQuartzCore.dylib ${DST}/libswiftQuartzCore.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftUIKit.dylib ${DST}/libswiftUIKit.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftos.dylib ${DST}/libswiftos.dylib
cp -v ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftsimd.dylib ${DST}/libswiftsimd.dylib
