package com.autodj.app.audio

import java.io.File

/**
 * Desktop: loads libautodj_native.dylib from, in this order, -Dautodj.native.path=<file>, the application resources
 * directory of the packaged app (compose.application.resources.dir), or java.library.path.
 */
internal object NativeLibrary {
    fun load() {
        System.getProperty("autodj.native.path")?.takeIf { it.isNotBlank() }?.let { System.load(File(it).absolutePath); return }
        val resources = System.getProperty("compose.application.resources.dir")
        if (resources != null) {
            val f = File(resources, "libautodj_native.dylib")
            if (f.isFile) { System.load(f.absolutePath); return }
        }
        System.loadLibrary("autodj_native")
    }
}
