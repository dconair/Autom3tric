package com.autodj.desktop

import com.autodj.app.audio.AudioEngine
import com.autodj.app.dj.DjController
import com.autodj.app.dj.MemoryLibraryStore
import com.autodj.app.dj.MixState
import com.autodj.app.dj.OrderChoice
import com.autodj.app.dj.TransitionChoice
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.ImageComposeScene
import androidx.compose.ui.unit.Density
import com.autodj.app.ui.dj.MixScreen
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import org.jetbrains.skia.EncodedImageFormat
import org.junit.Assume.assumeTrue
import java.io.File
import java.nio.file.Files
import kotlin.math.abs
import kotlin.math.sqrt
import kotlin.test.AfterTest
import kotlin.test.BeforeTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The automatic mix against the real native library: library songs -> list -> plan -> playback with songs loaded just in time
 * (the test plays the part of the audio thread by pulling blocks through the offline render) -> WAV export.
 */
class DjMixTest {
    private lateinit var engine: AudioEngine
    private lateinit var scope: CoroutineScope
    private lateinit var dir: File

    @BeforeTest fun setUp() {
        val path = System.getProperty("autodj.native.path")
        assumeTrue("no native library built on this OS", path != null && File(path).isFile)
        engine = AudioEngine(48000, voices = 4)
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
        dir = Files.createTempDirectory("autodj-mix-test").toFile()
    }

    @AfterTest fun tearDown() {
        if (::scope.isInitialized) scope.cancel()
        if (::engine.isInitialized) engine.close()
        if (::dir.isInitialized) dir.deleteRecursively()
    }

    private fun controller() =
        DjController(engine, DesktopTrackDecoder(afconvert = null, ffmpeg = null), MemoryLibraryStore(), scope, io = Dispatchers.IO, ui = Dispatchers.Default)

    private fun DjController.demoLibrary(): DjController {
        addDemoTracks()
        val end = System.currentTimeMillis() + 120_000
        Thread.sleep(100)
        while (busy) {
            assertTrue(System.currentTimeMillis() < end, "import did not finish")
            Thread.sleep(50)
        }
        assertEquals(4, tracks.count { it.ready })
        return this
    }

    private fun await(what: String, timeoutMs: Long = 20_000, cond: () -> Boolean) {
        val end = System.currentTimeMillis() + timeoutMs
        while (!cond()) {
            assertTrue(System.currentTimeMillis() < end, "timeout: $what")
            Thread.sleep(10)
        }
    }

    @Test fun listPlanAndOrdering() {
        val dj = controller().demoLibrary()
        dj.mix.addAll()
        assertEquals(4, dj.mix.list.size)
        await("plan") { dj.mix.plan != null }
        val plan = checkNotNull(dj.mix.plan)
        assertEquals(4, plan.items.size)
        assertEquals(3, plan.transitions.size)
        println("plan: ${plan.totalSec} s, transitions ${plan.transitions.map { it.label + " " + it.lengthBeats }}")
        assertTrue(plan.transitions.none { it.type == "cut" }, "demo songs are within a few BPM of each other")
        assertTrue(plan.totalSec > 100.0)

        dj.mix.chooseTransition(TransitionChoice.Cut)
        await("cut plan") { dj.mix.plan?.transitions?.all { it.type == "cut" } == true }

        val before = dj.mix.list.toList()
        dj.mix.optimiseOrder(OrderChoice.TempoUp)
        await("reordered") { dj.mix.list.toList() != before }
        assertEquals(before.toSet(), dj.mix.list.toSet())
        val bpms = dj.mix.list.map { checkNotNull(dj.item(it)?.analysis).bpm }
        assertEquals(bpms.sorted(), bpms, "tempo ascending")

        dj.mix.move(0, 1)
        dj.mix.removeAt(0)
        assertEquals(3, dj.mix.list.size)
        dj.remove(dj.mix.list[0])
        assertEquals(2, dj.mix.list.size, "a song removed from the library leaves the list")
    }

    @Test fun mixPlaysThroughWithSongsLoadedJustInTime() {
        val dj = controller().demoLibrary()
        dj.mix.chooseTransition(TransitionChoice.EqBlend)
        dj.mix.addAll()
        await("plan") { dj.mix.plan != null }
        val total = checkNotNull(dj.mix.plan).totalSec
        dj.mix.start(0)

        val block = 4800
        val out = FloatArray(block * 2)
        val seenItems = HashSet<Int>()
        var sawWaiting = false
        var frames = 0L
        var silentSeconds = 0
        var secEnergy = 0.0
        val end = System.currentTimeMillis() + 180_000
        while (dj.mix.state != MixState.Finished) {
            assertTrue(System.currentTimeMillis() < end, "mix did not finish; state=${dj.mix.state} item=${dj.snapshot.plan.item} error=${dj.mix.error}")
            // before the mix is up the "audio thread" only idles; afterwards it renders in blocks of 0.1 s
            engine.renderOffline(block, out)
            var e = 0.0
            for (v in out) e += v * v
            if (dj.mix.state == MixState.Playing) {
                frames += block
                secEnergy += e
                if (frames % 48000L == 0L) {
                    if (sqrt(secEnergy / (48000.0 * 2)) < 0.01) silentSeconds++
                    secEnergy = 0.0
                }
            }
            dj.poll()
            if (dj.mix.state == MixState.Playing) {
                seenItems.add(dj.snapshot.plan.item)
                sawWaiting = sawWaiting || dj.snapshot.plan.waiting >= 0
            }
            Thread.sleep(2)  // the decoder threads get their share, like next to a real audio callback
        }
        println("played ${frames / 48000.0} s of ${total} s planned; items seen $seenItems; waited=$sawWaiting; silent seconds=$silentSeconds")
        assertEquals(setOf(0, 1, 2, 3), seenItems)
        assertTrue(!sawWaiting, "songs were bound before they were due")
        assertEquals(0, silentSeconds, "no gaps in the mix")
        assertTrue(abs(frames / 48000.0 - total) < 1.0, "the mix is as long as planned")
        assertEquals(null, dj.mix.error)
        dj.mix.stop()
        await("stopped") { dj.mix.state == MixState.Idle }
        // everything handed back: a new start works
        engine.renderOffline(block, out)
        dj.mix.start(2)
        await("loading") { dj.mix.state == MixState.Playing }
        for (i in 0 until 20) {
            engine.renderOffline(block, out)
            dj.poll()
        }
        assertEquals(0, dj.snapshot.plan.item.coerceAtLeast(0), "restarted at its first song")
        dj.mix.stop()
        await("stopped again") { dj.mix.state == MixState.Idle }
    }

    @Test fun exportWritesAWavAndATrackList() {
        val dj = controller().demoLibrary()
        dj.mix.addAll()
        await("plan") { dj.mix.plan != null }
        val plan = checkNotNull(dj.mix.plan)
        val wav = File(dir, "mix.wav")
        dj.mix.export(wav.path, 16)
        await("export started") { dj.mix.exportProgress != null || dj.mix.exportResult != null }
        await("export done", 120_000) { dj.mix.exportProgress == null }
        assertEquals(null, dj.mix.error)
        assertEquals(wav.path, dj.mix.exportResult)
        val bytes = wav.readBytes()
        assertEquals("RIFF", String(bytes, 0, 4))
        val frames = (bytes.size - 44) / 4
        assertEquals(plan.totalSec, frames / 48000.0, 0.1)
        var peak = 0
        var i = 44
        while (i + 1 < bytes.size) {
            peak = maxOf(peak, abs((bytes[i].toInt() and 0xFF) or (bytes[i + 1].toInt() shl 8)))
            i += 2
        }
        assertTrue(peak in 3000..32767, "audible but not clipping: $peak")
        val list = File(dir, "mix-Tracklist.txt").readLines()
        assertEquals(4, list.size)
        println(list.joinToString("\n"))
        // the same export can be cancelled
        val second = File(dir, "cancel.wav")
        dj.mix.export(second.path, 24)
        await("second export started") { dj.mix.exportProgress != null }
        dj.mix.cancelExport()
        await("second export ended", 60_000) { dj.mix.exportProgress == null }
        assertTrue(!second.exists(), "a cancelled export leaves no file")
    }

    @OptIn(ExperimentalComposeUiApi::class)
    private fun shot(dj: DjController, name: String, width: Int, height: Int) {
        val scene = ImageComposeScene(width = width, height = height, density = Density(1f)) {
            MixScreen(dj, onGoToLibrary = {}, onExport = {})
        }
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
        val dj = controller().demoLibrary()
        shot(dj, "mix_empty", 1180, 760)
        dj.mix.addAll()
        await("plan") { dj.mix.plan != null }
        shot(dj, "mix_desktop", 1180, 760)
        shot(dj, "mix_phone", 412, 860)
        dj.mix.start(0)
        await("playing") { dj.mix.state == MixState.Playing }
        val out = FloatArray(4800 * 2)
        // play into the first transition
        val firstTransition = checkNotNull(dj.mix.runningPlan).transitions[0]
        while (dj.snapshot.plan.timeSec < firstTransition.startSec + firstTransition.lengthSec * 0.4) {
            engine.renderOffline(4800, out)
            dj.poll()
            Thread.sleep(1)
        }
        shot(dj, "mix_playing", 1180, 760)
        dj.mix.stop()
        await("stopped") { dj.mix.state == MixState.Idle }
    }
}
