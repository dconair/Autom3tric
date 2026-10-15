package com.autodj.app.ui.dj

import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.autodj.app.design.AutoDjMotion
import com.autodj.app.design.AutoDjShape
import com.autodj.app.design.AutoDjSpace
import com.autodj.app.design.AutoDjType
import com.autodj.app.design.Btn
import com.autodj.app.design.Display
import com.autodj.app.design.Ds
import com.autodj.app.design.KeyChip
import com.autodj.app.design.ProgressBar
import com.autodj.app.design.SongWave
import com.autodj.app.design.StatusKind
import com.autodj.app.design.StatusTag
import com.autodj.app.design.Tile
import com.autodj.app.design.TileKind
import com.autodj.app.design.Txt
import com.autodj.app.design.camelotChip
import com.autodj.app.design.riseIn
import com.autodj.app.dj.DjController
import com.autodj.app.dj.TrackItem
import com.autodj.app.dj.TrackStatus
import java.util.Locale

/**
 * The track library: add songs, see tempo / key / structure per song, send a song to a deck or to the granular instrument.
 * [onPickFiles] opens the platform's file picker (it calls back into the controller itself).
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
    dropHint: Boolean = false,
) = LibraryContent(
    tracks = dj.tracks, onPickFiles = onPickFiles, onDemo = { dj.addDemoTracks() }, onRemove = { dj.remove(it.id) },
    modifier = modifier, onLoadDeck = onLoadDeck, onGranular = onGranular, onQueue = onQueue, isQueued = isQueued, footer = footer,
    dropHint = dropHint,
)

/** The stateless library (also what the screenshot tests render). [initialSelected] opens that row at first. */
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
    dropHint: Boolean = false,
    initialSelected: String? = null,
) {
    var selectedId by remember { mutableStateOf(initialSelected) }
    var keyFilter by remember { mutableStateOf<String?>(null) }
    val selected = tracks.firstOrNull { it.id == selectedId }
    val shown = if (keyFilter == null) tracks else tracks.filter { it.analysis?.camelot == keyFilter }
    BoxWithConstraints(modifier.fillMaxSize()) {
        val wide = maxWidth >= 860.dp
        val margin = if (wide) AutoDjSpace.margin else AutoDjSpace.phoneMargin
        val gap = if (wide) AutoDjSpace.gap else AutoDjSpace.phoneGap
        val rows: @Composable (Modifier) -> Unit = { m ->
            if (tracks.isEmpty()) {
                EmptyTile(onPickFiles, onDemo, m)
            } else {
                LazyColumn(m, verticalArrangement = Arrangement.spacedBy(AutoDjSpace.rowGap)) {
                    items(shown, key = { it.id }) { item ->
                        TrackRow(
                            item, wide = wide, open = item.id == selectedId,
                            onToggle = { selectedId = if (selectedId == item.id) null else item.id },
                            onRemove = { onRemove(item) },
                            onLoadDeck = onLoadDeck?.let { f -> { deck: Int -> f(item, deck) } },
                            onGranular = onGranular?.let { f -> { f(item) } },
                            onQueue = onQueue?.let { f -> { f(item) } },
                            queued = isQueued(item),
                        )
                    }
                    if (shown.isEmpty()) item { Txt("Kein Song in dieser Tonart.", style = AutoDjType.body, second = true, modifier = Modifier.padding(16.dp)) }
                    if (footer != null) item { Txt(footer, style = AutoDjType.small, second = true, modifier = Modifier.padding(vertical = 8.dp)) }
                }
            }
        }
        if (wide) {
            Row(Modifier.fillMaxSize().padding(horizontal = margin).padding(bottom = margin), horizontalArrangement = Arrangement.spacedBy(gap)) {
                rows(Modifier.weight(8f).fillMaxHeight())
                Column(Modifier.weight(4f).fillMaxHeight().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(gap)) {
                    AddTile(onPickFiles, onDemo, dropHint, Modifier.fillMaxWidth().riseIn(0))
                    StatsTile(tracks, Modifier.fillMaxWidth().riseIn(1))
                    KeysTile(selected?.analysis?.camelot, keyFilter, { keyFilter = if (keyFilter == it) null else it }, Modifier.fillMaxWidth().riseIn(2))
                }
            }
        } else {
            Column(Modifier.fillMaxSize().padding(horizontal = margin), verticalArrangement = Arrangement.spacedBy(gap)) {
                AddTile(onPickFiles, onDemo, dropHint, Modifier.fillMaxWidth(), compact = true)
                rows(Modifier.weight(1f).fillMaxWidth())
            }
        }
    }
}

// ---- right-hand tiles ----------------------------------------------------------------------------------------------

@Composable
private fun AddTile(onPickFiles: () -> Unit, onDemo: () -> Unit, dropHint: Boolean, modifier: Modifier, compact: Boolean = false) {
    Tile(modifier, kind = TileKind.Accent, padding = if (compact) 12.dp else 20.dp) {
        if (compact) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Txt("Songs hinzufügen", style = AutoDjType.lead, modifier = Modifier.weight(1f), maxLines = 1)
                Btn("Dateien wählen", onClick = onPickFiles, small = true)
                Btn("Demo", onClick = onDemo, small = true, description = "Demo-Songs laden")
            }
        } else {
            Txt("Songs hinzufügen", style = AutoDjType.head)
            Txt(
                "MP3, WAV, FLAC, M4A und OGG. Die App erkennt Tempo, Takt, Tonart und Aufbau jedes Songs." + if (dropHint) " Du kannst Dateien auch ins Fenster ziehen." else "",
                style = AutoDjType.body, modifier = Modifier.padding(top = 8.dp),
            )
            Row(Modifier.padding(top = 16.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Btn("Dateien wählen", onClick = onPickFiles)
                Btn("Demo-Songs", onClick = onDemo)
            }
        }
    }
}

@Composable
private fun StatsTile(tracks: List<TrackItem>, modifier: Modifier) {
    val ready = tracks.count { it.ready }
    val total = tracks.sumOf { it.analysis?.durationSec ?: 0.0 }
    Tile(modifier) {
        Txt("Bibliothek", style = AutoDjType.lead)
        Txt(tracks.size.toString(), style = AutoDjType.display, modifier = Modifier.padding(top = 4.dp))
        Txt(if (tracks.size == 1) "Song" else "Songs", style = AutoDjType.small, second = true)
        Row(Modifier.padding(top = 12.dp), horizontalArrangement = Arrangement.spacedBy(24.dp)) {
            Column {
                Txt(if (total > 0) formatLong(total) else "–", style = AutoDjType.lead)
                Txt("Spielzeit", style = AutoDjType.small, second = true)
            }
            Column {
                Txt(if (tracks.isEmpty()) "–" else "$ready von ${tracks.size}", style = AutoDjType.lead)
                Txt("analysiert", style = AutoDjType.small, second = true)
            }
        }
    }
}

/** The Camelot wheel as 2 × 12 chips: the keys that blend with the chosen song are lit; a tap filters the list. */
@Composable
private fun KeysTile(of: String?, filter: String?, onFilter: (String) -> Unit, modifier: Modifier) {
    val fits = of?.let(::compatibleKeys).orEmpty()
    Tile(modifier) {
        Txt("Passende Tonarten", style = AutoDjType.lead)
        Txt(
            if (of == null) "Wähle einen Song, dann leuchten die Tonarten, die dazu passen." else "Zu $of passen die hellen Felder. Tippe eines an, um die Liste zu filtern.",
            style = AutoDjType.small, second = true, modifier = Modifier.padding(top = 2.dp, bottom = 12.dp),
        )
        listOf('A', 'B').forEach { letter ->
            Row(Modifier.fillMaxWidth().padding(bottom = 4.dp), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                (1..12).forEach { n ->
                    val key = "$n$letter"
                    KeyCell(n.toString(), key, lit = of == null || key in fits, on = filter == key, onClick = { onFilter(key) }, modifier = Modifier.weight(1f))
                }
            }
        }
    }
}

@Composable
private fun KeyCell(label: String, key: String, lit: Boolean, on: Boolean, onClick: () -> Unit, modifier: Modifier) {
    val c = Ds.c
    Box(
        modifier.height(32.dp).clip(AutoDjShape)
            .background(if (on) c.ink else if (lit) camelotChip(key) else c.tile2)
            .semantics { contentDescription = "Tonart $key" }
            .clickable(indication = null, interactionSource = remember { MutableInteractionSource() }, role = Role.Button) { onClick() },
        contentAlignment = Alignment.Center,
    ) {
        Txt(label, style = AutoDjType.small, color = if (on) c.onInk else if (lit) Color(0xFF2A1D17) else c.ink2, textAlign = TextAlign.Center, maxLines = 1)
    }
}

/** Camelot keys that mix cleanly with [key]: same, one step either way, and the relative major/minor. */
internal fun compatibleKeys(key: String): Set<String> {
    val n = key.dropLast(1).toIntOrNull() ?: return emptySet()
    val l = key.last()
    val other = if (l == 'A') 'B' else 'A'
    fun wrap(i: Int) = ((i - 1 + 12) % 12) + 1
    return setOf("$n$l", "${wrap(n + 1)}$l", "${wrap(n - 1)}$l", "$n$other")
}

// ---- list ----------------------------------------------------------------------------------------------------------

@Composable
private fun EmptyTile(onPickFiles: () -> Unit, onDemo: () -> Unit, modifier: Modifier) {
    Tile(modifier.fillMaxWidth().riseIn(0), padding = 32.dp) {
        Txt("Noch keine Songs", style = AutoDjType.head)
        Txt(
            "Die App analysiert jeden Song: Tempo, Taktraster, Tonart und Aufbau. Mit eigenen Dateien oder mit den Demo-Songs kannst du sofort loslegen.",
            style = AutoDjType.body, second = true, modifier = Modifier.padding(top = 8.dp),
        )
        Row(Modifier.padding(top = 20.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Btn("Songs hinzufügen", onClick = onPickFiles, primary = true)
            Btn("Demo-Songs", onClick = onDemo)
        }
    }
}

@Composable
private fun TrackRow(
    item: TrackItem,
    wide: Boolean,
    open: Boolean,
    onToggle: () -> Unit,
    onRemove: () -> Unit,
    onLoadDeck: ((Int) -> Unit)?,
    onGranular: (() -> Unit)?,
    onQueue: (() -> Unit)?,
    queued: Boolean,
) {
    val a = item.analysis
    val ready = item.ready && a != null
    Tile(
        Modifier.fillMaxWidth().animateContentSize(tween(AutoDjMotion.ms(AutoDjMotion.base), easing = AutoDjMotion.out)),
        kind = if (open && ready) TileKind.Ink else TileKind.Plain, padding = 0.dp,
    ) {
        Row(
            Modifier.fillMaxWidth().height(AutoDjSpace.row)
                .clickable(enabled = ready, indication = null, interactionSource = remember { MutableInteractionSource() }, role = Role.Button) { onToggle() }
                .padding(horizontal = 16.dp),
            verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Column(Modifier.weight(1f)) {
                Txt(item.name, style = AutoDjType.row, maxLines = 1)
                if (!ready) Txt(subtitle(item), style = AutoDjType.small, second = true, maxLines = 1)
            }
            if (ready && a != null) {
                Txt(if (a.hasTempo) String.format(Locale.US, "%.1f", a.bpm) else "–", style = AutoDjType.bodyMedium, modifier = Modifier.width(52.dp), textAlign = TextAlign.End, maxLines = 1)
                Box(Modifier.width(44.dp), contentAlignment = Alignment.CenterStart) { if (a.hasKey) KeyChip(a.camelot) }
                if (wide) Txt(formatTime(a.durationSec), style = AutoDjType.body, second = true, modifier = Modifier.width(48.dp), textAlign = TextAlign.End, maxLines = 1)
            } else {
                when (item.status) {
                    TrackStatus.Failed -> {
                        StatusTag(StatusKind.Bad, "Fehlgeschlagen")
                        Btn("Entfernen", onClick = onRemove, small = true)
                    }
                    else -> {
                        if (item.progress >= 0f) ProgressBar(item.progress, Modifier.width(96.dp))
                        StatusTag(StatusKind.Idle, if (item.status == TrackStatus.Queued) "Wartet" else "Läuft")
                    }
                }
            }
        }
        if (open && ready && a != null) {
            Column(Modifier.fillMaxWidth().riseIn(0).padding(start = 16.dp, end = 16.dp, bottom = 16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Display(Modifier.fillMaxWidth().height(if (wide) 84.dp else 72.dp)) {
                    SongWave(item.overview, a, lit = Ds.c.deckALit, modifier = Modifier.fillMaxSize().padding(8.dp))
                }
                if (a.sections.isNotEmpty() && a.durationSec > 0) SectionBar(item)
                Txt(subtitle(item), style = AutoDjType.small, second = true)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
                    if (onLoadDeck != null) {
                        Btn("Auf Deck A", onClick = { onLoadDeck(0) })
                        Btn("Auf Deck B", onClick = { onLoadDeck(1) })
                    }
                    if (onQueue != null) Btn(if (queued) "In der Mix-Liste" else "In die Mix-Liste", onClick = onQueue, primary = !queued, enabled = !queued)
                    if (onGranular != null) Btn("Granular", onClick = onGranular)
                    Spacer(Modifier.weight(1f))
                    Btn("Entfernen", onClick = onRemove)
                }
            }
        }
    }
}

/** Intro, Teil, Break, Drop, Outro as blocks over the length of the song. */
@Composable
private fun SectionBar(item: TrackItem) {
    val a = item.analysis ?: return
    val c = Ds.c
    Row(Modifier.fillMaxWidth().height(28.dp), horizontalArrangement = Arrangement.spacedBy(2.dp)) {
        a.sections.forEach { s ->
            val w = ((s.endSec - s.startSec) / a.durationSec).toFloat().coerceAtLeast(0.02f)
            Box(Modifier.weight(w).fillMaxHeight().clip(AutoDjShape).background(c.onInk.copy(alpha = if (s.label == "drop") .34f else .16f)), contentAlignment = Alignment.CenterStart) {
                Txt(sectionLabel(s.label), style = AutoDjType.small, modifier = Modifier.padding(horizontal = 8.dp), maxLines = 1)
            }
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
            if (a.hasKey) append(" · ").append(a.keyName)
            if (a.hasTempo) append(String.format(Locale.US, " · Raster %d %%", (a.gridConfidence * 100).toInt()))
            else append(" · ohne erkennbares Tempo")
            if (a.hasTempo && a.gridConfidence < 0.5) append(" (unsicher)")
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

/** "1 h 12 min" / "23 min" for the library total. */
internal fun formatLong(sec: Double): String {
    val m = (sec / 60).toInt()
    return if (m >= 60) String.format(Locale.US, "%d h %02d min", m / 60, m % 60) else String.format(Locale.US, "%d min", m.coerceAtLeast(1))
}
