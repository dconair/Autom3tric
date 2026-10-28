package com.autodj.app.audio

/** Android: the library is packaged in the APK (libautodj_native.so). The desktop app has its own NativeLibrary. */
internal object NativeLibrary {
    fun load() = System.loadLibrary("autodj_native")
}
