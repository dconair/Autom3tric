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
import com.autodj.app.ui.dj.DecksScreen
import com.autodj.app.ui.dj.MixScreen
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.asCoroutineDispatcher
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
    // Compose state is written on one thread, like on the real UI thread
    private val uiThread = java.util.concurrent.Executors.newSingleThreadExecutor()
    private val ui = uiThread.asCoroutineDispatcher()

    @BeforeTest fun setUp() {
        val path = System.getProperty("autodj.native.path")
        assumeTrue("no native library built on this OS", path != null && File(path).isFile)
        engine = AudioEngine(48000, voices = 4)
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
        dir = Files.createTempDirectory("autodj-mix-test").toFile()
    }

    @AfterTest fun tearDown() {
        if (::scope.isInitialized) scope.cancel()
        Thread.sleep(500)  // a decoder thread that was mid-call finishes before the engine goes away
        if (::engine.isInitialized) engine.close()
        uiThread.shutdown()
        if (::dir.isInitialized) dir.deleteRecursively()
    }

    private fun controller() =
        DjController(engine, DesktopTrackDecoder(afconvert = null, ffmpeg = null), MemoryLibraryStore(), scope, io = Dispatchers.IO, ui = ui)

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
            Themed { MixScreen(dj, onGoToLibrary = {}, onExport = {}) }
        }
        try {
            val bytes = checkNotNull(scene.settled().encodeToData(EncodedImageFormat.PNG)).bytes
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

    @OptIn(ExperimentalComposeUiApi::class)
    private fun shotDecks(dj: DjController, name: String, width: Int, height: Int) {
        val scene = ImageComposeScene(width = width, height = height, density = Density(1f)) { Themed { DecksScreen(dj, onGoToLibrary = {}) } }
        try {
            val bytes = checkNotNull(scene.settled().encodeToData(EncodedImageFormat.PNG)).bytes
            val out = File(System.getProperty("autodj.screenshotDir") ?: "build/screenshots").apply { mkdirs() }
            File(out, "$name.png").writeBytes(bytes)
            assertTrue(bytes.size > 8_000, "render of $name looks blank")
        } finally {
            scene.close()
        }
    }

    @Test fun decksLoadPlayAndSync() {
        val dj = controller().demoLibrary()
        shotDecks(dj, "decks_empty", 1180, 800)
        dj.decks.load(0, dj.tracks[0])
        dj.decks.load(1, dj.tracks[1])
        await("both decks loaded") { dj.decks.deck[0].item != null && dj.decks.deck[1].item != null }
        val out = FloatArray(4800 * 2)
        fun pump(blocks: Int) {
            repeat(blocks) {
                engine.renderOffline(4800, out)
                dj.poll()
            }
        }
        pump(2)
        assertTrue(dj.snapshot.deck[0].loaded && dj.snapshot.deck[1].loaded)
        dj.decks.set(1, "sync", 1f)
        dj.decks.playPause(0)
        pump(3)
        dj.decks.playPause(1)
        pump(40)  // 4 s
        val a = dj.snapshot.deck[0]
        val b = dj.snapshot.deck[1]
        println("deck A ${a.bpm} bpm pos ${a.posSec} phase ${a.beatPhase} | deck B ${b.bpm} bpm pos ${b.posSec} phase ${b.beatPhase}")
        assertTrue(a.playing && b.playing)
        assertEquals(a.bpm, b.bpm, 0.2, "deck B follows deck A's tempo")
        val dphase = (a.beatPhase - b.beatPhase).let { it - Math.round(it) }
        assertTrue(abs(dphase) < 0.05, "beats are aligned: $dphase")
        dj.decks.set(0, "eq_low_db", -26f)
        dj.decks.set(0, "volume", 0.7f)
        dj.decks.set(-1, "crossfader", -0.4f)
        pump(10)
        shotDecks(dj, "decks_playing", 1180, 800)
        shotDecks(dj, "decks_phone", 412, 900)
        // hotcues and loops that the analysis prepared
        val cues = dj.decks.deck[0].cues.filterNotNull()
        assertTrue(cues.size >= 2, "the analysis prepares hotcues: $cues")
        val target = cues.last()
        dj.decks.cuePad(0, dj.decks.deck[0].cues.indexOfFirst { it == target })
        pump(3)
        assertTrue(abs(dj.snapshot.deck[0].posSec - target.sec) < 0.6, "a pad jumps to its cue: ${dj.snapshot.deck[0].posSec} vs ${target.sec}")
        dj.decks.loopBeats(0, 4)
        val loop = checkNotNull(dj.decks.deck[0].loop)
        pump(10)
        var lo = 1e9
        var hi = -1e9
        repeat(40) {  // 4 s inside a 4-beat loop (~2 s) of the demo song
            pump(1)
            val p = dj.snapshot.deck[0].posSec
            lo = minOf(lo, p)
            hi = maxOf(hi, p)
        }
        assertTrue(lo >= loop.startSec - 0.1 && hi <= loop.endSec + 0.1, "the position stays in the loop: $lo..$hi vs ${loop.startSec}..${loop.endSec}")
        assertTrue(dj.snapshot.deck[0].playing)
        dj.decks.loopOff(0)
        assertTrue(dj.decks.deck[0].loop == null)
        pump(10)
        assertTrue(dj.snapshot.deck[0].posSec > hi - 0.1, "plays on after the loop")
        shotDecks(dj, "decks_cues", 1180, 800)
        dj.decks.cue(0)
        pump(5)
        assertTrue(!dj.snapshot.deck[0].playing)
        assertTrue(dj.snapshot.deck[0].posSec < 5.0, "cue returns to the start (first bar)")
        dj.decks.eject(1)
        pump(3)
        assertTrue(!dj.snapshot.deck[1].loaded)
        await("deck state follows") { dj.decks.deck[1].item == null }
        // the auto mix takes the decks: loading is refused meanwhile
        dj.mix.addAll()
        await("plan") { dj.mix.plan != null }
        dj.mix.start(0)
        await("playing") { dj.mix.state == MixState.Playing }
        dj.decks.load(1, dj.tracks[2])
        assertTrue(dj.decks.error != null, "manual loading is refused while the auto mix runs")
        dj.mix.stop()
        await("stopped") { dj.mix.state == MixState.Idle }
    }
}
