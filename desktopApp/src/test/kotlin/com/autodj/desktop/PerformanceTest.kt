package com.autodj.desktop

import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.ImageComposeScene
import androidx.compose.ui.unit.Density
import com.autodj.app.audio.AudioEngine
import com.autodj.app.dj.DjController
import com.autodj.app.dj.MemoryLibraryStore
import com.autodj.app.dj.PAD_COUNT
import com.autodj.app.dj.PadPatterns
import com.autodj.app.dj.PerformanceController
import com.autodj.app.ui.dj.EffectsScreen
import com.autodj.app.ui.dj.PadsScreen
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.cancel
import org.jetbrains.skia.EncodedImageFormat
import org.junit.Assume.assumeTrue
import java.io.File
import kotlin.math.sqrt
import kotlin.test.AfterTest
import kotlin.test.BeforeTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/** Effects and drum pads against the real native library (rendered offline, the way the other DJ tests do it). */
class PerformanceTest {
    private lateinit var engine: AudioEngine
    private lateinit var scope: CoroutineScope
    private val uiThread = java.util.concurrent.Executors.newSingleThreadExecutor()
    private val ui = uiThread.asCoroutineDispatcher()

    @BeforeTest fun setUp() {
        val path = System.getProperty("autodj.native.path")
        assumeTrue("no native library built on this OS", path != null && File(path).isFile)
        engine = AudioEngine(48000, voices = 4)
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    }

    @AfterTest fun tearDown() {
        if (::scope.isInitialized) scope.cancel()
        Thread.sleep(200)
        if (::engine.isInitialized) engine.close()
        uiThread.shutdown()
    }

    /** Renders [seconds] and returns the mono mix-down plus the pad steps the status reported. */
    private fun render(seconds: Double, steps: MutableSet<Int>? = null, poll: (() -> Unit)? = null): FloatArray {
        val block = 480
        val out = FloatArray(block * 2)
        val total = (seconds * 48000).toInt() / block
        val mono = FloatArray(total * block)
        for (b in 0 until total) {
            engine.renderOffline(block, out)
            for (i in 0 until block) mono[b * block + i] = 0.5f * (out[2 * i] + out[2 * i + 1])
            if (steps != null) engine.pollDj()?.let { if (it.padStep >= 0) steps.add(it.padStep) }
            poll?.invoke()
        }
        return mono
    }

    private fun rms(x: FloatArray, fromSec: Double, toSec: Double): Double {
        val a = (fromSec * 48000).toInt().coerceIn(0, x.size)
        val b = (toSec * 48000).toInt().coerceIn(a, x.size)
        if (b <= a) return 0.0
        var e = 0.0
        for (i in a until b) e += x[i].toDouble() * x[i]
        return sqrt(e / (b - a))
    }

    @Test fun everyParameterTheScreensUseExists() {
        for (key in listOf(
            "fx.echo.on", "fx.echo.time", "fx.echo.feedback", "fx.echo.mix", "fx.reverb.on", "fx.reverb.mix", "fx.flanger.on", "fx.flanger.mix",
            "fx.roll.on", "fx.roll.size", "fx.stutter.on", "fx.stutter.rate", "pads.volume", "pads.seq_on", "pads.swing", "transport.bpm",
        )) {
            assertNotNull(engine.spec(key), "parameter $key")
        }
    }

    @Test fun aPadTapSoundsAndTheSequencerRunsThroughAllSteps() {
        val perf = PerformanceController(engine)
        perf.set("pads.volume", 0.9f)  // the thresholds below were set for this level; the product default is lower
        perf.loadPattern(PadPatterns.last())  // empty
        render(0.2)
        perf.hit(0)
        val tap = render(0.6)
        assertTrue(rms(tap, 0.0, 0.3) > 0.05, "the kick is audible")
        assertTrue(rms(tap, 0.4, 0.6) < rms(tap, 0.0, 0.3) * 0.5, "and decays")

        val silent = render(2.0)
        assertTrue(rms(silent, 1.0, 2.0) < 1e-3, "nothing plays by itself")

        perf.loadPattern(PadPatterns[0])  // house
        perf.setOn("pads.seq_on", true)
        val steps = HashSet<Int>()
        val song = render(4.0, steps)
        assertEquals((0..15).toSet(), steps, "the step indicator visits all sixteen steps")
        assertTrue(rms(song, 1.0, 4.0) > 0.03, "the groove is audible")
        perf.setOn("pads.seq_on", false)
        val after = render(2.5)
        assertTrue(rms(after, 1.5, 2.5) < 1e-3, "the sequencer stops, the tails run out")
        assertEquals(-1, engine.pollDj()?.padStep ?: -1)

        // programming a single step
        perf.loadPattern(PadPatterns.last())
        perf.toggleStep(2, 0)
        assertTrue(perf.stepOn(2, 0) && !perf.stepOn(2, 1))
        assertEquals("Eigenes Muster", perf.patternName)
        perf.setOn("pads.seq_on", true)
        val one = render(3.0)
        assertTrue(rms(one, 0.0, 3.0) > 0.02, "the snare on the downbeat plays")
        perf.clearRow(2)
        assertEquals(0, perf.rows[2])
        perf.setOn("pads.seq_on", false)
    }

    @Test fun echoAddsARepeatOneBeatLater() {
        val perf = PerformanceController(engine)
        perf.loadPattern(PadPatterns.last())
        perf.set("transport.bpm", 120f)
        perf.set("fx.echo.time", 1f)  // 1/4 = one beat = 0.5 s
        perf.set("fx.echo.mix", 0.8f)
        render(0.2)
        perf.hit(4)  // closed hat: ends quickly, so any later sound is the echo
        val dry = render(1.5)
        perf.setOn("fx.echo.on", true)
        render(0.3)
        perf.hit(4)
        val wet = render(1.5)
        val dryRepeat = rms(dry, 0.5, 0.65)
        val wetRepeat = rms(wet, 0.5, 0.65)
        println("repeat window rms: dry $dryRepeat, wet $wetRepeat")
        assertTrue(wetRepeat > 0.005 && wetRepeat > dryRepeat * 5, "the echo repeats the hat one beat later")
        perf.setOn("fx.echo.on", false)
        val tail = render(1.5)
        assertTrue(tail.isNotEmpty())
    }

    @Test fun holdingRollAndStutterChangesTheSoundAndReleasingBringsItBack() {
        val perf = PerformanceController(engine)
        perf.loadPattern(PadPatterns[0])
        perf.setOn("pads.seq_on", true)
        val plain = render(3.0)
        perf.setOn("fx.stutter.on", true)
        val stutter = render(1.5)
        perf.setOn("fx.stutter.on", false)
        perf.setOn("fx.roll.on", true)
        val roll = render(1.5)
        perf.setOn("fx.roll.on", false)
        val back = render(2.0)
        assertTrue(rms(plain, 1.0, 3.0) > 0.03)
        // the stutter gate silences half of every period: the 10 ms rms values vary much more than in the plain mix
        fun variation(x: FloatArray): Double {
            val w = 480
            val vals = (0 until x.size / w).map { rms(x, it * w / 48000.0, (it + 1) * w / 48000.0) }
            val mean = vals.average()
            return vals.sumOf { (it - mean) * (it - mean) } / vals.size / (mean * mean + 1e-9)
        }
        println("variation plain ${variation(plain)} stutter ${variation(stutter)} roll ${variation(roll)} back ${variation(back)}")
        assertTrue(variation(stutter) > variation(plain), "stutter gates the sound")
        assertTrue(rms(back, 1.0, 2.0) > 0.03, "after releasing, the groove is back")
        perf.setOn("pads.seq_on", false)
    }

    @Test fun freezeTurnsTheLastSecondsIntoAGranularSource() {
        val dj = DjController(engine, DesktopTrackDecoder(afconvert = null, ffmpeg = null), MemoryLibraryStore(), scope, io = Dispatchers.IO, ui = ui)
        var slot: Int? = -1
        var error: String? = null
        val done = java.util.concurrent.atomic.AtomicBoolean(false)
        dj.captureToGranular(2.0) { s, e ->
            slot = s
            error = e
            done.set(true)
        }
        await(done)
        assertEquals(null, slot, "nothing was rendered yet: $error")
        assertTrue(error != null)
        dj.perf.setOn("pads.seq_on", true)
        render(3.0)
        done.set(false)
        dj.captureToGranular(2.0) { s, e ->
            slot = s
            error = e
            done.set(true)
        }
        await(done)
        assertEquals(com.autodj.app.dj.FIRST_GRANULAR_SLOT, slot, "error: $error")
        assertEquals(listOf("Mix-Aufnahme 1"), dj.granularSlots.map { it.name })
        val peaks = assertNotNull(engine.sourcePeaks(slot!!, 64), "the engine holds the recording")
        assertTrue(peaks.any { kotlin.math.abs(it) > 0.05f })
        dj.perf.setOn("pads.seq_on", false)
    }

    private fun await(flag: java.util.concurrent.atomic.AtomicBoolean) {
        val end = System.currentTimeMillis() + 10_000
        while (!flag.get()) {
            assertTrue(System.currentTimeMillis() < end, "timeout")
            Thread.sleep(5)
        }
    }

    @OptIn(ExperimentalComposeUiApi::class)
    private fun shot(content: @androidx.compose.runtime.Composable () -> Unit, name: String, width: Int, height: Int) {
        val scene = ImageComposeScene(width = width, height = height, density = Density(1f)) { content() }
        try {
            val bytes = checkNotNull(scene.render().encodeToData(EncodedImageFormat.PNG)).bytes
            val out = File(System.getProperty("autodj.screenshotDir") ?: "build/screenshots").apply { mkdirs() }
            File(out, "$name.png").writeBytes(bytes)
            assertTrue(bytes.size > 8_000, "render of $name looks blank")
        } finally {
            scene.close()
        }
    }

    @Test fun screenshots() {
        val dj = DjController(engine, DesktopTrackDecoder(afconvert = null, ffmpeg = null), MemoryLibraryStore(), scope, io = Dispatchers.IO, ui = ui)
        shot({ EffectsScreen(dj, onCaptureToGranular = {}) }, "effects_desktop", 1180, 760)
        shot({ PadsScreen(dj) }, "pads_desktop", 1180, 900)
        dj.perf.setOn("fx.echo.on", true)
        dj.perf.setOn("fx.reverb.on", true)
        dj.perf.setOn("fx.stutter.on", true)
        dj.perf.set("fx.echo.time", 2f)
        dj.perf.setOn("pads.seq_on", true)
        render(0.9) { dj.poll() }
        shot({ EffectsScreen(dj, onCaptureToGranular = {}) }, "effects_on_desktop", 1180, 760)
        shot({ EffectsScreen(dj, onCaptureToGranular = {}) }, "effects_phone", 412, 1250)
        shot({ PadsScreen(dj) }, "pads_playing_desktop", 1180, 900)
        shot({ PadsScreen(dj) }, "pads_phone", 412, 1500)
        assertEquals(PAD_COUNT, dj.perf.rows.size)
        dj.perf.setOn("pads.seq_on", false)
    }
}
