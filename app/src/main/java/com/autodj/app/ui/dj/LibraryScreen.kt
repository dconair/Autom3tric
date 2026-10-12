package com.autodj.app.ui.dj

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autodj.app.dj.DjColors
import com.autodj.app.dj.DjController
import com.autodj.app.dj.DjTheme
import com.autodj.app.dj.Overview
import com.autodj.app.dj.TrackAnalysisInfo
import com.autodj.app.dj.TrackItem
import com.autodj.app.dj.TrackStatus
import com.autodj.app.dj.camelotColor
import java.util.Locale

/**
 * The track library: add songs, see tempo / key / energy / structure per song, send a song to a deck or to the granular
 * instrument. [onPickFiles] opens the platform's file picker (it calls back into the controller itself).
 */
@Composable
fun LibraryScreen(
    dj: DjController,
    onPickFiles: () -> Unit,
    modifier: Modifier = Modifier,
    onLoadDeck: ((TrackItem, Int) -> Unit)? = null,
    onGranular: ((TrackItem) -> Unit)? = null,
    onQueue: ((TrackItem) -> Unit)? = null,
    isQueued: (TrackItem) -> Boolean = { false },
    footer: String? = null,
) = LibraryContent(
    tracks = dj.tracks, onPickFiles = onPickFiles, onDemo = { dj.addDemoTracks() }, onRemove = { dj.remove(it.id) },
    modifier = modifier, onLoadDeck = onLoadDeck, onGranular = onGranular, onQueue = onQueue, isQueued = isQueued, footer = footer,
)

/** The stateless library list (also what the screenshot tests render). */
@Composable
fun LibraryContent(
    tracks: List<TrackItem>,
    onPickFiles: () -> Unit,
    onDemo: () -> Unit,
    onRemove: (TrackItem) -> Unit,
    modifier: Modifier = Modifier,
    onLoadDeck: ((TrackItem, Int) -> Unit)? = null,
    onGranular: ((TrackItem) -> Unit)? = null,
    onQueue: ((TrackItem) -> Unit)? = null,
    isQueued: (TrackItem) -> Boolean = { false },
    footer: String? = null,
) {
    DjTheme {
        Column(modifier.fillMaxSize().background(DjColors.bg).padding(16.dp)) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                Column(Modifier.weight(1f)) {
                    Text("Bibliothek", color = DjColors.text, fontSize = 22.sp, fontWeight = FontWeight.SemiBold)
                    val n = tracks.size
                    val ready = tracks.count { it.ready }
                    Text(
                        if (n == 0) "Noch keine Songs" else "$ready von $n Songs analysiert",
                        color = DjColors.muted, fontSize = 13.sp,
                    )
                }
                OutlinedButton(onClick = onDemo) { Text("Demo-Songs", color = DjColors.text) }
                Button(onClick = onPickFiles) { Text("Songs hinzufügen") }
            }
            Spacer(Modifier.height(12.dp))
            if (tracks.isEmpty()) {
                EmptyState(Modifier.weight(1f))
            } else {
                LazyColumn(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    items(tracks, key = { it.id }) { item ->
                        TrackRow(
                            item,
                            onRemove = { onRemove(item) },
                            onLoadDeck = onLoadDeck?.let { f -> { deck: Int -> f(item, deck) } },
                            onGranular = onGranular?.let { f -> { f(item) } },
                            onQueue = onQueue?.let { f -> { f(item) } },
                            queued = isQueued(item),
                        )
                    }
                }
            }
            if (footer != null) {
                Spacer(Modifier.height(8.dp))
                Text(footer, color = DjColors.muted, fontSize = 12.sp)
            }
        }
    }
}

@Composable
private fun EmptyState(modifier: Modifier) {
    Box(modifier.fillMaxWidth(), contentAlignment = Alignment.Center) {
        Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text("Songs hinzufügen, um zu mixen", color = DjColors.text, fontSize = 18.sp)
            Text(
                "Die App analysiert jeden Song: Tempo, Beat-Raster, Tonart, Energie und Aufbau.\n" +
                    "MP3, WAV, FLAC, M4A und OGG werden unterstützt. Ohne eigene Dateien helfen die Demo-Songs.",
                color = DjColors.muted, fontSize = 13.sp,
            )
        }
    }
}

@Composable
private fun TrackRow(
    item: TrackItem,
    onRemove: () -> Unit,
    onLoadDeck: ((Int) -> Unit)?,
    onGranular: (() -> Unit)?,
    onQueue: (() -> Unit)?,
    queued: Boolean,
) {
    Column(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(10.dp)).background(DjColors.panel)
            .border(1.dp, DjColors.line, RoundedCornerShape(10.dp)).padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Column(Modifier.weight(1f)) {
                Text(item.name, color = DjColors.text, fontSize = 15.sp, fontWeight = FontWeight.Medium, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text(subtitle(item), color = if (item.status == TrackStatus.Failed) DjColors.bad else DjColors.muted, fontSize = 12.sp, maxLines = 2, overflow = TextOverflow.Ellipsis)
            }
            val a = item.analysis
            if (item.ready && a != null) {
                if (a.hasKey) KeyChip(a.camelot, a.keyName)
                BpmBlock(a)
            }
        }
        when (item.status) {
            TrackStatus.Queued, TrackStatus.Decoding, TrackStatus.Analysing -> {
                if (item.progress < 0f) LinearProgressIndicator(Modifier.fillMaxWidth().height(3.dp), color = DjColors.deckA, trackColor = DjColors.line)
                else LinearProgressIndicator(progress = { item.progress.coerceIn(0f, 1f) }, modifier = Modifier.fillMaxWidth().height(3.dp), color = DjColors.deckA, trackColor = DjColors.line)
            }
            TrackStatus.Ready -> item.analysis?.let { a -> WaveformStrip(item.overview, a, Modifier.fillMaxWidth().height(54.dp)) }
            TrackStatus.Failed -> Unit
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
            if (item.ready) {
                if (onLoadDeck != null) {
                    DeckButton("A", DjColors.deckA) { onLoadDeck(0) }
                    DeckButton("B", DjColors.deckB) { onLoadDeck(1) }
                }
                if (onQueue != null) {
                    TextButton(onClick = onQueue, enabled = !queued) {
                        Text(if (queued) "✓ In der Mix-Liste" else "In die Mix-Liste", color = if (queued) DjColors.good else DjColors.text)
                    }
                }
                if (onGranular != null) TextButton(onClick = onGranular) { Text("Granular", color = DjColors.text) }
            }
            Spacer(Modifier.weight(1f))
            TextButton(onClick = onRemove) { Text("Entfernen", color = DjColors.muted) }
        }
    }
}

@Composable
private fun DeckButton(label: String, color: Color, onClick: () -> Unit) {
    Button(
        onClick = onClick,
        colors = ButtonDefaults.buttonColors(containerColor = color, contentColor = Color.Black),
        modifier = Modifier.size(width = 44.dp, height = 34.dp),
        contentPadding = androidx.compose.foundation.layout.PaddingValues(0.dp),
    ) { Text(label, fontWeight = FontWeight.Bold) }
}

@Composable
private fun KeyChip(camelot: String, name: String) {
    val c = camelotColor(camelot)
    Column(
        Modifier.clip(RoundedCornerShape(8.dp)).background(c.copy(alpha = 0.18f)).border(1.dp, c.copy(alpha = 0.6f), RoundedCornerShape(8.dp)).padding(horizontal = 10.dp, vertical = 4.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(camelot, color = c, fontSize = 15.sp, fontWeight = FontWeight.Bold)
        Text(name, color = DjColors.muted, fontSize = 10.sp)
    }
}

@Composable
private fun BpmBlock(a: TrackAnalysisInfo) {
    Column(Modifier.width(76.dp), horizontalAlignment = Alignment.End) {
        if (a.hasTempo) {
            Text(String.format(Locale.US, "%.1f", a.bpm), color = DjColors.text, fontSize = 20.sp, fontWeight = FontWeight.SemiBold)
            Text("BPM", color = DjColors.muted, fontSize = 10.sp)
        } else {
            Text("–", color = DjColors.muted, fontSize = 20.sp)
            Text("kein Tempo", color = DjColors.muted, fontSize = 10.sp)
        }
    }
}

private fun subtitle(item: TrackItem): String = when (item.status) {
    TrackStatus.Queued -> "Wartet auf Import"
    TrackStatus.Decoding -> "Dekodiere …"
    TrackStatus.Analysing -> "Analysiere Tempo, Tonart und Aufbau …"
    TrackStatus.Failed -> "Import fehlgeschlagen: ${item.error ?: "unbekannter Fehler"}"
    TrackStatus.Ready -> {
        val a = item.analysis
        if (a == null) "" else buildString {
            append(formatTime(a.durationSec))
            if (a.hasTempo) append(String.format(Locale.US, " · Raster %d %%", (a.gridConfidence * 100).toInt()))
            else append(" · ohne erkennbares Tempo")
            if (a.hasTempo && a.gridConfidence < 0.5) append(" (unsicher)")
            if (a.sections.isNotEmpty()) append(" · ").append(a.sections.joinToString(" › ") { sectionLabel(it.label) })
        }
    }
}

internal fun sectionLabel(label: String): String = when (label) {
    "intro" -> "Intro"
    "drop" -> "Drop"
    "break" -> "Break"
    "outro" -> "Outro"
    else -> "Teil"
}

internal fun formatTime(sec: Double): String {
    val s = sec.toInt().coerceAtLeast(0)
    return String.format(Locale.US, "%d:%02d", s / 60, s % 60)
}

internal fun sectionColor(label: String): Color = when (label) {
    "intro" -> Color(0xFF5C7FA3)
    "drop" -> Color(0xFFE0A440)
    "break" -> Color(0xFF7E6BC4)
    "outro" -> Color(0xFF5C7FA3)
    else -> Color(0xFF6B7280)
}

/** Waveform overview with the section strip underneath and the bar energy as a faint ribbon behind it. */
@Composable
internal fun WaveformStrip(overview: Overview?, a: TrackAnalysisInfo, modifier: Modifier) {
    Canvas(modifier.clip(RoundedCornerShape(6.dp)).background(Color(0xFF0A0B0D))) {
        val w = size.width
        val h = size.height
        val stripH = 5f
        val waveH = h - stripH - 2f
        val mid = waveH / 2f
        val dur = a.durationSec.coerceAtLeast(0.001)

        // bar energy ribbon
        if (a.barEnergy.isNotEmpty() && a.barSec > 0.0) {
            val barW = (a.barSec / dur * w).toFloat()
            a.barEnergy.forEachIndexed { i, e ->
                val x = ((a.firstBarSec + i * a.barSec) / dur * w).toFloat()
                val bh = e * waveH
                drawRect(Color(0xFF1B2230), Offset(x, mid - bh / 2f), Size(barW.coerceAtLeast(1f), bh))
            }
        }
        // waveform
        if (overview != null && overview.columns > 0) {
            val n = overview.columns
            for (i in 0 until n) {
                val x = (i + 0.5f) / n * w
                val top = mid - overview.max[i] / 127f * mid
                val bottom = mid - overview.min[i] / 127f * mid
                drawLine(DjColors.wave, Offset(x, top), Offset(x, if (bottom - top < 1f) top + 1f else bottom), strokeWidth = (w / n).coerceAtLeast(1f))
            }
        }
        // beat grid: a tick on every downbeat, only if the grid is trustworthy
        if (a.hasTempo && a.gridConfidence >= 0.5 && a.barSec > 0.0) {
            var t = a.firstBarSec
            var k = 0
            while (t < dur) {
                val x = (t / dur * w).toFloat()
                if (k % 4 == 0) drawLine(Color(0x33FFFFFF), Offset(x, 0f), Offset(x, waveH), strokeWidth = 1f)
                t += a.barSec
                k++
            }
        }
        // sections
        for (s in a.sections) {
            val x0 = (s.startSec / dur * w).toFloat()
            val x1 = (s.endSec / dur * w).toFloat()
            drawRect(sectionColor(s.label), Offset(x0 + 1f, h - stripH), Size((x1 - x0 - 2f).coerceAtLeast(1f), stripH))
        }
        // mix points
        if (a.hasTempo) {
            drawLine(DjColors.good, Offset((a.mixInSec / dur * w).toFloat(), 0f), Offset((a.mixInSec / dur * w).toFloat(), waveH), strokeWidth = 2f)
            drawLine(DjColors.warn, Offset((a.mixOutSec / dur * w).toFloat(), 0f), Offset((a.mixOutSec / dur * w).toFloat(), waveH), strokeWidth = 2f)
        }
    }
}
