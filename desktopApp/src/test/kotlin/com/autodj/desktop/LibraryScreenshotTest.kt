package com.autodj.desktop

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.ImageComposeScene
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.Density
import com.autodj.app.dj.Overview
import com.autodj.app.dj.SectionInfo
import com.autodj.app.dj.TrackAnalysisInfo
import com.autodj.app.dj.TrackItem
import com.autodj.app.dj.TrackStatus
import com.autodj.app.ui.dj.LibraryContent
import org.jetbrains.skia.EncodedImageFormat
import java.io.File
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.sin
import kotlin.test.Test
import kotlin.test.assertTrue

/** Renders the library list offscreen with made-up tracks in every state; writes build/screenshots/library_*.png. */
@OptIn(ExperimentalComposeUiApi::class)
class LibraryScreenshotTest {
    private fun analysis(bpm: Double, key: String, camelot: String, seconds: Double, confidence: Double = 0.9): TrackAnalysisInfo {
        val bar = 4 * 60.0 / bpm
        val bars = (seconds / bar).toInt()
        val energy = List(bars) { i -> if (i < 8) 0.35f else if (i in (bars / 2 - 4) until (bars / 2 + 4)) 0.3f else if (i >= bars - 8) 0.4f - 0.04f * (i - (bars - 8)) else 0.9f }
        val bounds = listOf(0, 8, bars / 2 - 4, bars / 2 + 4, bars - 8, bars)
        val labels = listOf("intro", "drop", "break", "drop", "outro")
        val sections = labels.indices.map { SectionInfo(bounds[it] * bar, bounds[it + 1] * bar, labels[it]) }
        return TrackAnalysisInfo(
            durationSec = seconds, bpm = bpm, beatSec = 60.0 / bpm, gridOffsetSec = 0.02, tempoConfidence = 0.9, gridConfidence = confidence,
            downbeatConfidence = 0.8, keyName = key, camelot = camelot, keyConfidence = 0.7, rmsDb = -14.0, peakDb = -1.0, trimDb = -2.0,
            barSec = bar, firstBarSec = 0.02, barEnergy = energy, sections = sections, mixInSec = 0.02, mixOutSec = bounds[4] * bar, json = "{}",
        )
    }

    private fun overview(a: TrackAnalysisInfo): Overview {
        val n = 512
        val mx = ByteArray(n)
        val mn = ByteArray(n)
        for (i in 0 until n) {
            val t = i.toDouble() / n * a.durationSec
            val bar = ((t - a.firstBarSec) / a.barSec).toInt().coerceIn(0, a.barEnergy.size - 1)
            val e = a.barEnergy[bar]
            val pulse = 0.55 + 0.45 * abs(sin(2 * PI * t / a.beatSec))
            val v = (127 * (0.15 + 0.8 * e) * pulse).toInt().coerceIn(2, 126)
            mx[i] = v.toByte()
            mn[i] = (-v).toByte()
        }
        return Overview(mn, mx)
    }

    private fun ready(i: Int, name: String, bpm: Double, key: String, camelot: String, seconds: Double, confidence: Double = 0.9): TrackItem {
        val a = analysis(bpm, key, camelot, seconds, confidence)
        return TrackItem("id$i", name, "/music/$name.mp3", TrackStatus.Ready, 1f, null, a, overview(a))
    }

    private val tracks = listOf(
        ready(1, "Night Drive (Extended Mix)", 124.0, "A minor", "8A", 372.0),
        ready(2, "Sunrise on the Roof", 126.0, "C major", "8B", 301.0),
        ready(3, "Slow Burn", 118.0, "E minor", "9A", 255.0, confidence = 0.42),
        TrackItem("id4", "Track being analysed", "/music/x.flac", TrackStatus.Analysing, 0.62f),
        TrackItem("id5", "Waiting in the queue", "/music/y.wav", TrackStatus.Queued, -1f),
        TrackItem("id6", "broken file", "/music/z.mp3", TrackStatus.Failed, 0f, error = "Das Format von „z.mp3“ wird nicht unterstützt"),
    )

    private fun render(name: String, items: List<TrackItem>, width: Int, height: Int) {
        val scene = ImageComposeScene(width = width, height = height, density = Density(1f)) {
            Box(Modifier.fillMaxSize()) {
                LibraryContent(items, onPickFiles = {}, onDemo = {}, onRemove = {}, onLoadDeck = { _, _ -> }, onGranular = {}, onQueue = {})
            }
        }
        try {
            val bytes = checkNotNull(scene.render().encodeToData(EncodedImageFormat.PNG)).bytes
            val dir = File(System.getProperty("autodj.screenshotDir") ?: "build/screenshots").apply { mkdirs() }
            File(dir, "$name.png").writeBytes(bytes)
            assertTrue(bytes.size > 8_000, "render of $name looks blank (${bytes.size} bytes)")
        } finally {
            scene.close()
        }
    }

    @Test fun libraryWithTracksInEveryState() = render("library_desktop", tracks, 1100, 900)

    @Test fun libraryOnAPhoneWidth() = render("library_phone", tracks, 412, 900)

    @Test fun emptyLibrary() = render("library_empty", emptyList(), 1100, 500)
}
