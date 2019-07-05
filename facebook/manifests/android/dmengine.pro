# Some extra modifiers since we remove quite a lot in the engine
-dontwarn android.view.autofill.AutofillManager

-keep class android.support.v4.app.BaseFragmentActivityApi14
-keepclassmembers  class android.support.v4.app.BaseFragmentActivityApi14 {
    *;
}

# The actual extension code
-keep class com.defold.facebook.** {
   *;
}
