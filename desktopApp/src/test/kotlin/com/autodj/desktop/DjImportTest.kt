package com.autodj.desktop

import com.autodj.app.audio.AudioEngine
import com.autodj.app.dj.DjController
import com.autodj.app.dj.JsonLibraryStore
import com.autodj.app.dj.MemoryLibraryStore
import com.autodj.app.dj.TrackItem
import com.autodj.app.dj.TrackStatus
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.cancel
import org.junit.Assume.assumeTrue
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.file.Files
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.exp
import kotlin.math.sin
import kotlin.random.Random
import kotlin.test.AfterTest
import kotlin.test.BeforeTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/**
 * The whole import path against the real native library: file -> decoder -> engine ingest -> analysis -> library row.
 * Runs wherever the gradle build produced the library (Linux with the null audio driver, macOS).
 */
class DjImportTest {
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
        dir = Files.createTempDirectory("autodj-import-test").toFile()
    }

    @AfterTest fun tearDown() {
        if (::scope.isInitialized) scope.cancel()
        if (::engine.isInitialized) engine.close()
        uiThread.shutdown()
        if (::dir.isInitialized) dir.deleteRecursively()
    }

    private fun controller(store: com.autodj.app.dj.LibraryStore = MemoryLibraryStore()) =
        DjController(engine, DesktopTrackDecoder(afconvert = null, ffmpeg = null), store, scope, io = Dispatchers.IO, ui = ui)

    private fun DjController.awaitIdle(timeoutMs: Long = 120_000) {
        val end = System.currentTimeMillis() + timeoutMs
        Thread.sleep(100)
        while (busy) {
            assertTrue(System.currentTimeMillis() < end, "import did not finish: ${tracks.map { it.name to it.status }}")
            Thread.sleep(50)
        }
    }

    /** A 4/4 house loop as a 16-bit stereo WAV: kick on every beat (accented on 1), hats on the off-beats, a bass note per bar. */
    private fun writeSong(file: File, bpm: Double, seconds: Double, rate: Int = 44100, startOffsetSec: Double = 0.0) {
        val frames = (seconds * rate).toInt()
        val beat = 60.0 / bpm
        val rnd = Random(7)
        val out = FloatArray(frames)
        var phase = 0.0
        for (n in 0 until frames) {
            val t = n.toDouble() / rate - startOffsetSec
            if (t < 0) continue
            val beatIx = Math.floor(t / beat).toInt()
            val tb = t - beatIx * beat
            val accent = if (beatIx % 4 == 0) 1.0 else 0.75
            phase += 2 * PI * (50.0 + 90.0 * exp(-tb * 40.0)) / rate
            var v = accent * 0.8 * sin(phase) * exp(-tb * 9.0)
            val th = (t + beat / 2) % beat  // hat on the off-beat
            if (th < 0.05) v += 0.25 * (rnd.nextFloat() * 2 - 1) * exp(-th * 90.0)
            val bar = Math.floor(t / (4 * beat)).toInt()
            v += 0.12 * sin(2 * PI * (if (bar % 2 == 0) 55.0 else 65.41) * n / rate)
            out[n] = v.toFloat()
        }
        val bb = ByteBuffer.allocate(44 + frames * 4).order(ByteOrder.LITTLE_ENDIAN)
        bb.put("RIFF".toByteArray()).putInt(36 + frames * 4).put("WAVEfmt ".toByteArray()).putInt(16).putShort(1).putShort(2)
            .putInt(rate).putInt(rate * 4).putShort(4).putShort(16).put("data".toByteArray()).putInt(frames * 4)
        for (n in 0 until frames) {
            val s = (out[n].coerceIn(-1f, 1f) * 32767f).toInt().toShort()
            bb.putShort(s).putShort(s)
        }
        file.writeBytes(bb.array())
    }

    @Test fun demoSongsAreAnalysedWithTheirTempoAndKey() {
        val dj = controller()
        assertEquals(4, dj.addDemoTracks())
        dj.awaitIdle()
        assertEquals(4, dj.tracks.count { it.ready }, dj.tracks.map { it.status to it.error }.toString())
        val expected = listOf(118.0 to "8A", 122.0 to "9A", 126.0 to "8B", 124.0 to "7A")
        for ((i, e) in expected.withIndex()) {
            val a = checkNotNull(dj.tracks[i].analysis)
            println("demo $i: ${dj.tracks[i].name} bpm=${a.bpm} key=${a.keyName} ${a.camelot} grid=${a.gridConfidence} mixIn=${a.mixInSec} mixOut=${a.mixOutSec} sections=${a.sections.map { it.label }}")
            assertEquals(e.first, a.bpm, 0.6, "bpm of demo $i")
            assertEquals(e.second, a.camelot, "key of demo $i")
            assertTrue(a.gridConfidence > 0.6, "grid confidence of demo $i = ${a.gridConfidence}")
            assertNotNull(dj.tracks[i].overview, "overview of demo $i")
        }
        assertEquals(0, dj.addDemoTracks(), "the same demo songs are not added twice")
    }

    @Test fun aWavFileOnDiskGoesThroughDecodeAndAnalysis() {
        val f = File(dir, "House 128.wav")
        writeSong(f, 128.0, 30.0)
        val dj = controller()
        assertEquals(1, dj.importSources(listOf(f.path)))
        dj.awaitIdle()
        val t = dj.tracks.single()
        assertEquals(TrackStatus.Ready, t.status, t.error)
        assertEquals("House 128", t.name)
        val a = checkNotNull(t.analysis)
        println("file: bpm=${a.bpm} grid=${a.gridConfidence} offset=${a.gridOffsetSec} duration=${a.durationSec}")
        assertEquals(128.0, a.bpm, 0.6)
        assertEquals(30.0, a.durationSec, 0.05)
        assertTrue(a.gridConfidence > 0.6, "grid confidence ${a.gridConfidence}")
        assertEquals(0.0, minOf(a.gridOffsetSec % a.beatSec, a.beatSec - a.gridOffsetSec % a.beatSec), 0.02)
    }

    /** The controller as DesktopHost builds it (default dispatchers, so Dispatchers.Main): imports must not stay queued. */
    @Test fun importsFinishWithTheDefaultMainDispatcher() {
        val f = File(dir, "House 124.wav")
        writeSong(f, 124.0, 20.0)
        val dj = DjController(engine, DesktopTrackDecoder(afconvert = null, ffmpeg = null), MemoryLibraryStore(), scope)
        dj.importSources(listOf(f.path))
        dj.addDemoTracks()
        dj.awaitIdle()
        assertTrue(dj.tracks.all { it.status == TrackStatus.Ready }, dj.tracks.map { it.name to it.status to it.error }.toString())
        assertEquals(5, dj.tracks.size)
    }

    @Test fun gridOffsetFollowsASongThatStartsLate() {
        val f = File(dir, "late.wav")
        writeSong(f, 120.0, 30.0, startOffsetSec = 0.23)
        val dj = controller()
        dj.importSources(listOf(f.path))
        dj.awaitIdle()
        val a = checkNotNull(dj.tracks.single().analysis)
        assertEquals(120.0, a.bpm, 0.6)
        val beat = a.beatSec
        val off = (a.gridOffsetSec - 0.23).mod(beat)
        assertTrue(minOf(off, beat - off) < 0.025, "grid is ${off * 1000} ms away from the first kick")
    }

    @Test fun anUnreadableFileFailsWithAMessageAndTheQueueContinues() {
        val bad = File(dir, "bad.mp3").apply { writeText("definitely not an mp3") }
        val good = File(dir, "good.wav")
        writeSong(good, 124.0, 20.0)
        val dj = controller()
        assertEquals(2, dj.importSources(listOf(bad.path, good.path)))
        dj.awaitIdle()
        assertEquals(TrackStatus.Failed, dj.tracks[0].status)
        assertTrue(dj.tracks[0].error!!.isNotBlank())
        assertEquals(TrackStatus.Ready, dj.tracks[1].status, dj.tracks[1].error)
    }

    @Test fun theLibrarySurvivesARestartWithoutReanalysis() {
        val f = File(dir, "persist.wav")
        writeSong(f, 126.0, 20.0)
        val storeFile = File(dir, "lib/library.json")
        val first = controller(JsonLibraryStore(storeFile))
        first.importSources(listOf(f.path))
        first.awaitIdle()
        val before = checkNotNull(first.tracks.single().analysis)
        Thread.sleep(300)  // persist() writes on a worker
        assertTrue(storeFile.isFile, "library.json written")

        val second = controller(JsonLibraryStore(storeFile))
        val t = second.tracks.single()
        assertEquals(TrackStatus.Ready, t.status)
        assertEquals(before.bpm, checkNotNull(t.analysis).bpm, 1e-9)
        assertEquals(before.camelot, t.analysis!!.camelot)
        assertEquals(before.sections.size, t.analysis!!.sections.size)
        assertNotNull(t.overview)
        // and the audio can be decoded again on demand
        var err = ""
        val id = second.loadTrackBlocking(t) { err = it }
        assertNotNull(id, err)
        engine.trackRelease(id)
    }

    @Test fun aLibrarySongBecomesAGranularSource() {
        val f = File(dir, "grain.wav")
        writeSong(f, 124.0, 12.0)
        val dj = controller()
        dj.importSources(listOf(f.path))
        dj.awaitIdle()
        val item: TrackItem = dj.tracks.single()
        var slotLoaded = -1
        dj.onGranularSlotLoaded = { slotLoaded = it }
        var result: Pair<Int?, String?>? = null
        dj.sendToGranular(item) { slot, error -> result = slot to error }
        val end = System.currentTimeMillis() + 20_000
        while (result == null && System.currentTimeMillis() < end) Thread.sleep(20)
        assertEquals(4, result?.first, result?.second)
        assertEquals(4, slotLoaded)
        assertEquals("grain", dj.granularSlots.single().name)
        val info = assertNotNull(engine.sourceInfo(4))
        assertEquals(12.0, info.seconds.toDouble(), 0.1)
        // the same song again reuses the slot
        var again = -1
        dj.sendToGranular(item) { slot, _ -> again = slot ?: -2 }
        assertEquals(4, again)
        assertTrue(abs(info.seconds - 12.0) < 0.1)
    }

    @Test fun removingATrackDropsItFromTheLibrary() {
        val dj = controller()
        dj.addDemoTracks()
        dj.awaitIdle()
        val id = dj.tracks[1].id
        dj.remove(id)
        assertEquals(3, dj.tracks.size)
        assertTrue(dj.tracks.none { it.id == id })
    }
}
