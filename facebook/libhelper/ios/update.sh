#!/usr/bin/env bash

VERSION=v8.1.0
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

# These are the ones that XCode resolved and put into the Frameworks folder when using the above mentioned frameworks
SDK=${DYNAMO_HOME}/ext/SDKs/XcodeDefault12.1.xctoolchain
DST=../../res/ios/Frameworks

mkdir -p $DST

${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftAVFoundation.dylib -r -o ${DST}/libswiftAVFoundation.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftAccelerate.dylib -r -o ${DST}/libswiftAccelerate.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCore.dylib -r -o ${DST}/libswiftCore.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCoreAudio.dylib -r -o ${DST}/libswiftCoreAudio.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCoreData.dylib -r -o ${DST}/libswiftCoreData.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCoreFoundation.dylib -r -o ${DST}/libswiftCoreFoundation.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCoreGraphics.dylib -r -o ${DST}/libswiftCoreGraphics.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCoreImage.dylib -r -o ${DST}/libswiftCoreImage.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCoreLocation.dylib -r -o ${DST}/libswiftCoreLocation.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftCoreMedia.dylib -r -o ${DST}/libswiftCoreMedia.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftDarwin.dylib -r -o ${DST}/libswiftDarwin.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftDispatch.dylib -r -o ${DST}/libswiftDispatch.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftFoundation.dylib -r -o ${DST}/libswiftFoundation.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftMetal.dylib -r -o ${DST}/libswiftMetal.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftObjectiveC.dylib -r -o ${DST}/libswiftObjectiveC.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftPhotos.dylib -r -o ${DST}/libswiftPhotos.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftQuartzCore.dylib -r -o ${DST}/libswiftQuartzCore.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftUIKit.dylib -r -o ${DST}/libswiftUIKit.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftos.dylib -r -o ${DST}/libswiftos.dylib
${SDK}/usr/bin/bitcode_strip ${SDK}/usr/lib/swift-5.0/iphoneos/libswiftsimd.dylib -r -o ${DST}/libswiftsimd.dylib
