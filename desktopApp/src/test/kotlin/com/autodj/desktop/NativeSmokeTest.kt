package com.autodj.desktop

import com.autodj.app.audio.AudioEngine
import org.junit.Assume.assumeTrue
import java.io.File
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/**
 * Loads the real libautodj_native.dylib through JNI. Runs only where the gradle build produced it (macOS); on other
 * systems the tests are skipped.
 */
class NativeSmokeTest {
    private fun requireDylib() {
        val path = System.getProperty("autodj.native.path")
        assumeTrue("no native library built on this OS", path != null && File(path).isFile)
    }

    @Test fun engineLoadsAndAnswersThroughJni() {
        requireDylib()
        AudioEngine(48000, voices = 4).use { e ->
            assertTrue(e.specs.size >= 120, "registry has ${e.specs.size} parameters")
            assertTrue(e.loadDemoSource(0, 0))
            val density = checkNotNull(e.spec("granular.v0.density"))
            e.setParam(density.id, 77f)
            assertEquals(77f, e.getParam(density.id), 1e-4f)
            assertNotNull(e.sourcePeaks(0, 256))
            assertNotNull(e.sourceInfo(0))
            assertNotNull(e.spec("granular.chaos"))
            assertNotNull(e.pollStatus() ?: e.pollStatus(), "status poll")
        }
    }

    @Test fun coreAudioStartAndStopDoNotCrash() {
        requireDylib()
        AudioEngine(48000, voices = 4).use { e ->
            e.loadDemoSource(0, 0)
            // A CI runner may have no audio device: a non-zero result is acceptable, a crash is not.
            val result = e.start()
            println("CoreAudio start result=$result info=${e.driverInfo()}")
            Thread.sleep(300)
            println("after 300 ms: ${e.driverInfo()}  status=${e.pollStatus()}")
            e.stop()
        }
    }
}
