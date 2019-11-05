# How to update the Android SDK

1. Clone https://github.com/britzl/pompom
2. Make a copy of facebook-5.9.0.sh and name it appropriately.
3. Modify the copied file so that it references the recommended versions of the dependencies required for the version of the Facebook SDK you are upgrading to. (https://developers.facebook.com/docs/android/componentsdks)
4. Run the modified script.
5. Copy the generated files (the `res`, `lib` and `manifest` folders) from the `export` folder to this project
