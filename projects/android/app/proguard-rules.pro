# Add project specific ProGuard rules here.
# By default, the flags in this file are appended to flags specified
# in /Users/lehoangquyen/AndroidStudio/sdk/tools/proguard/proguard-android.txt
# You can edit the include path and order by changing the proguardFiles
# directive in build.gradle.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# Add any project specific keep options here:

# If your project uses WebView with JS, uncomment the following
# and specify the fully qualified class name to the JavaScript interface
# class:
#-keepclassmembers class fqcn.of.javascript.interface.for.webview {
#   public *;
#}

# required for jni
-keep class com.hqgame.networknes.GameSurfaceView {
    public private protected *;
}

# required for jni
-keep class com.hqgame.networknes.BaseActivity {
    java.lang.Class getSettingsClass();
    void runOnMainThread(...);
    void onLanServerDiscovered(...);
    java.lang.String getHostIPAddress();
}

# required for jni
-keep class com.hqgame.networknes.Settings {
    boolean isVoiceChatEnabled();
}

-dontwarn com.android.installreferrer

-keep class com.google.android.gms.games.multiplayer.turnbased.** { *; }
-dontwarn com.google.android.gms.**

-keep class com.android.vending.billing.**