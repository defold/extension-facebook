
# iOS

How to generate the libraries for the FaceBook SDK

* Clone the SDK

		$ git clone https://github.com/facebook/facebook-objc-sdk.git

* Change directory

		$ cd facebook-objc-sdk/samples/Scrumptious

* Initialize the pod structure (if you haven't already, run `gem install cocoapods`)

		$ pod init

* Edit the `Podfile` with the text editor of your choice

		$ subl Podfile

Make sure it contains something like this:

	platform :ios, '8.0'
	target 'Scrumptious' do
	  use_frameworks!
	  pod 'FBSDKCoreKit', '~> 5.0.0'
	  pod 'FBSDKLoginKit', '~> 5.0.0'
	  pod 'FBSDKShareKit', '~> 5.0.0'
	end

* Install the pods

		$ pod install

**Caution** Note if the command output's a message like this:

	[!] CocoaPods did not set the base configuration of your project because your project already has a custom config set. In order for CocoaPods integration to work at all, please either set the base configurations of the target `Scrumptious` to `Target Support Files/Pods-Scrumptious/Pods-Scrumptious.release.xcconfig` or include the `Target Support Files/Pods-Scrumptious/Pods-Scrumptious.release.xcconfig` in your build configuration (`../Configurations/Application.xcconfig`).

[More info](https://discuss.multi-os-engine.org/t/pods-podfile-dir-path-and-pods-root-not-defined/823/2)

Then add the include to `../../Configurations/Application.xcconfig`:

	#include "../Scrumptious/Pods/Target Support Files/Pods-Scrumptious/Pods-Scrumptious.release.xcconfig"

* Open the newly create workspace `Scrumptious.xcworkspace` (NOT the original `Scrumptious.xcodeproj`)

		$ open samples/Scrumptious/Scrumptious.xcworkspace

After launching XCode, you should see both the old project project as well as a `Pods` project in the same workspace

* Set the project to build the release configuration

* Build the project with <kbd>⌘ Command</kbd> + <kbd>B</kbd>

*I had to select a mobile provisioning profile for it to allow me to build the project*

* After building, locate the built frameworks

It's not very intuitive, but they end up next to the built app.
Select the Project / Products / Scrumptious.app, and select the `Full Path`, e.g.

	/Users/mathiaswesterdahl/Library/Developer/Xcode/DerivedData/Scrumptious-aoeenivsjjyurcccftvbhkanjusr/Build/Products/Release-iphoneos/Scrumptious.app

Now, modify the path a bit, to see all the built artifacts:

	$ ls /Users/mathiaswesterdahl/Library/Developer/Xcode/DerivedData/Scrumptious-aoeenivsjjyurcccftvbhkanjusr/Build/Products/Release-iphoneos/
	FBSDKCoreKit			FBSDKShareKit			Scrumptious.app
	FBSDKLoginKit			Pods_Scrumptious.framework	Scrumptious.app.dSYM

* Copy the frameworks to the extension library folder

		$ cp -r /Users/mathiaswesterdahl/Library/Developer/Xcode/DerivedData/Scrumptious-aoeenivsjjyurcccftvbhkanjusr/Build/Products/Release-iphoneos/FBSDKCoreKit/FBSDKCoreKit.framework ~/projects/extension-facebook/facebook/lib/ios/
		$ cp -r /Users/mathiaswesterdahl/Library/Developer/Xcode/DerivedData/Scrumptious-aoeenivsjjyurcccftvbhkanjusr/Build/Products/Release-iphoneos/FBSDKLoginKit/FBSDKLoginKit.framework ~/projects/extension-facebook/facebook/lib/ios/
		$ cp -r /Users/mathiaswesterdahl/Library/Developer/Xcode/DerivedData/Scrumptious-aoeenivsjjyurcccftvbhkanjusr/Build/Products/Release-iphoneos/FBSDKShareKit/FBSDKShareKit.framework ~/projects/extension-facebook/facebook/lib/ios/

* Now build the extension, and you're done!

