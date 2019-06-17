# Building libraries for Android

## Build the .jar file

* Call `gradle build` with only the Android dependencies (see [build.gradle](build.gradle))

* Copy the jar file to `android-support.jar`

* Call `gradle build` with the Facebook dependencies (see [build.gradle](build.gradle))

## Repack the .jar file

* Create a text file containing the class files from `android-support.jar`

		$ jar tvf android-support.jar > support.txt

* Repack the facebook jar file with

		$ python repack.py ../build/package/share/java/facebooksdk-5.0.0.jar support.txt facebooksdk-5.0.0.jar

* Copy the `facebooksdk-5.0.0.jar` to `facebook/lib/android/facebooksdk-5.0.0.jar`

## Copy the resources

* Unpack the facebook package `facebooksdk-5.0.0.tar.gz` (generated when building the jar file)

		$ mkdir foo
		$ cd foo
		$ tar xf ../facebooksdk-5.0.0.tar.gz

* Make sure the share/java/<dependency>/res/ files doesn't collide with any in the `android.jar` from the engine

	* `http://d.defold.com/archive/<SHA1>/engine/armv7-android/android.jar`

* Copy the resources to the extension

		$ python copy_resources.py foo/share/java/res/ ../../res/android/res

## Done!


