package com.autodj.app.ui.dj

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autodj.app.dj.DjController
import com.autodj.app.dj.LengthChoice
import com.autodj.app.dj.MixController
import com.autodj.app.dj.MixState
import com.autodj.app.dj.OrderChoice
import com.autodj.app.dj.PlanInfo
import com.autodj.app.dj.TrackItem
import com.autodj.app.dj.TransitionChoice
import com.autodj.app.dj.TransitionInfoK
import com.autodj.app.design.AutoDjSpace
import com.autodj.app.design.AutoDjType
import com.autodj.app.design.Btn
import com.autodj.app.design.Display
import com.autodj.app.design.Ds
import com.autodj.app.design.KeyChip
import com.autodj.app.design.ProgressBar
import com.autodj.app.design.Segment
import com.autodj.app.design.StatusKind
import com.autodj.app.design.Tile
import com.autodj.app.design.TileKind
import com.autodj.app.design.Txt
import com.autodj.app.design.riseIn
import java.util.Locale

/**
 * The Auto-Mix tab: put songs in a list, let the app plan the transitions, start the mix, tune it, export it. The list shows songs and,
 * between them, the transition the planner chose; the timeline underneath shows both decks over the whole mix.
 * [onExport] opens the platform's "save as" dialog for the given bit depth and calls [MixController.export] with the path.
 */
@Composable
fun MixScreen(dj: DjController, onGoToLibrary: () -> Unit, onExport: (bits: Int) -> Unit, modifier: Modifier = Modifier) {
    val mix = dj.mix
    BoxWithConstraints(modifier.fillMaxSize()) {
        val wide = maxWidth >= 860.dp
        val margin = if (wide) AutoDjSpace.margin else AutoDjSpace.phoneMargin
        val gap = if (wide) AutoDjSpace.gap else AutoDjSpace.phoneGap
        if (wide) {
            Row(Modifier.fillMaxSize().padding(horizontal = margin).padding(bottom = margin), horizontalArrangement = Arrangement.spacedBy(gap)) {
                Column(Modifier.weight(7f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(gap)) {
                    LazyColumn(Modifier.weight(1f).fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(AutoDjSpace.rowGap)) {
                        mixRows(dj, mix, onGoToLibrary)
                    }
                    ListActions(dj, mix)
                    val plan = mix.runningPlan ?: mix.plan
                    if (plan != null) Timeline(plan, if (mix.state == MixState.Playing) mix.snapshot.plan.timeSec else null, Modifier.fillMaxWidth().height(150.dp))
                }
                Column(Modifier.weight(5f).fillMaxHeight().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(gap)) {
                    RunTile(mix, Modifier.fillMaxWidth().riseIn(0))
                    OptionsTile(mix, Modifier.fillMaxWidth().riseIn(1))
                    ExportTile(mix, onExport, Modifier.fillMaxWidth().riseIn(2))
                }
            }
        } else {
            LazyColumn(Modifier.fillMaxSize().padding(horizontal = margin), verticalArrangement = Arrangement.spacedBy(gap)) {
                item { RunTile(mix, Modifier.fillMaxWidth()) }
                mixRows(dj, mix, onGoToLibrary)
                item { ListActions(dj, mix) }
                val plan = mix.runningPlan ?: mix.plan
                if (plan != null) item { Timeline(plan, if (mix.state == MixState.Playing) mix.snapshot.plan.timeSec else null, Modifier.fillMaxWidth().height(130.dp)) }
                item { OptionsTile(mix, Modifier.fillMaxWidth()) }
                item { ExportTile(mix, onExport, Modifier.fillMaxWidth()) }
                item { androidx.compose.foundation.layout.Spacer(Modifier.height(margin)) }
            }
        }
    }
}

// ---- list ---------------------------------------------------------------------------------------------------

private fun LazyListScope.mixRows(dj: DjController, mix: MixController, onGoToLibrary: () -> Unit) {
    val plan = mix.runningPlan ?: mix.plan
    val from = if (mix.runningPlan != null) mix.runningFrom else 0
    val current = mix.currentIndex
    if (mix.list.isEmpty()) {
        item {
            Tile(Modifier.fillMaxWidth().riseIn(0), padding = 24.dp) {
                Txt(if (dj.tracks.any { it.ready }) "Noch keine Songs in der Liste" else "Noch keine Songs in der Bibliothek", style = AutoDjType.head)
                Txt(
                    "Füge Songs aus der Bibliothek hinzu oder nimm gleich alle. Die App ordnet die Übergänge selbst: Das Tempo wird angeglichen, die Takte liegen aufeinander.",
                    style = AutoDjType.body, second = true, modifier = Modifier.padding(top = 8.dp),
                )
                Row(Modifier.padding(top = 16.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Btn("Alle Songs hinzufügen", onClick = { mix.addAll() }, primary = true, enabled = dj.tracks.any { it.ready })
                    Btn("Zur Bibliothek", onClick = onGoToLibrary)
                }
            }
        }
        return
    }
    itemsIndexed(mix.list, key = { _, id -> id }) { i, id ->
        val item = dj.item(id)
        val planIndex = i - from
        Column(verticalArrangement = Arrangement.spacedBy(AutoDjSpace.rowGap)) {
            if (i > 0) {
                val t = if (planIndex >= 1 && plan != null) plan.transitions.getOrNull(planIndex - 1) else null
                val running = mix.state == MixState.Playing && mix.snapshot.plan.transition == planIndex - 1 && planIndex >= 1
                val progress = if (running && t != null && t.lengthSec > 0) ((mix.snapshot.plan.timeSec - t.startSec) / t.lengthSec).toFloat().coerceIn(0f, 1f) else 0f
                TransitionRow(t, progress, running)
            }
            SongRow(
                index = i, item = item, locked = mix.locked, playing = i == current,
                startsAt = if (plan != null && planIndex in plan.items.indices) plan.items[planIndex].startSec else null,
                onUp = { mix.move(i, -1) }, onDown = { mix.move(i, 1) }, onRemove = { mix.removeAt(i) }, onPlayFromHere = { mix.start(i) },
                canPlay = mix.state != MixState.Loading && mix.state != MixState.Stopping && mix.exportProgress == null,
            )
        }
    }
}

@Composable
private fun SongRow(
    index: Int, item: TrackItem?, locked: Boolean, playing: Boolean, startsAt: Double?, canPlay: Boolean,
    onUp: () -> Unit, onDown: () -> Unit, onRemove: () -> Unit, onPlayFromHere: () -> Unit,
) {
    val a = item?.analysis
    val c = Ds.c
    Tile(Modifier.fillMaxWidth().height(AutoDjSpace.row), kind = if (playing) TileKind.Ink else TileKind.Plain, padding = 0.dp) {
        Row(Modifier.fillMaxSize().padding(horizontal = 16.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            if (playing) Box(Modifier.size(10.dp).clip(RoundedCornerShape(2.dp)).background(c.accentLit))
            Column(Modifier.weight(1f)) {
                Txt(item?.name ?: "(entfernt)", style = AutoDjType.row, maxLines = 1)
                Txt(
                    buildString {
                        if (a != null) append(formatTime(a.durationSec))
                        if (startsAt != null) append(" · ab ").append(formatTime(startsAt))
                    },
                    style = AutoDjType.small, second = true, maxLines = 1,
                )
            }
            if (a != null) {
                Txt(if (a.hasTempo) String.format(Locale.US, "%.1f", a.bpm) else "–", style = AutoDjType.bodyMedium, maxLines = 1)
                if (a.hasKey) KeyChip(a.camelot) else Box(Modifier.width(1.dp))
            }
            RowAction("▶", "Ab hier spielen", enabled = canPlay && item != null, onClick = onPlayFromHere)
            RowAction("↑", "Nach oben", enabled = !locked && index > 0, onClick = onUp)
            RowAction("↓", "Nach unten", enabled = !locked, onClick = onDown)
            RowAction("✕", "Entfernen", enabled = !locked, onClick = onRemove)
        }
    }
}

@Composable
private fun RowAction(glyph: String, description: String, enabled: Boolean, onClick: () -> Unit) {
    Btn(glyph, onClick = onClick, enabled = enabled, small = true, modifier = Modifier.width(36.dp), description = description)
}

/** The transition between two songs: a quality square, the type, beats and tempo change, and the planner's reason. It fills while it runs. */
@Composable
private fun TransitionRow(t: TransitionInfoK?, progress: Float, running: Boolean) {
    val c = Ds.c
    Row(Modifier.fillMaxWidth().padding(start = 28.dp)) {
        Box(Modifier.fillMaxWidth().height(44.dp).clip(com.autodj.app.design.AutoDjShape).background(c.tile2)) {
            if (running) Box(Modifier.fillMaxHeight().fillMaxWidth(progress).background(c.tile3))
            Row(Modifier.fillMaxSize().padding(horizontal = 14.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                val kind = when {
                    t == null -> StatusKind.Idle
                    t.note.contains("nicht") -> StatusKind.Warn
                    else -> StatusKind.Good
                }
                Box(Modifier.size(10.dp).clip(RoundedCornerShape(2.dp)).background(when (kind) { StatusKind.Good -> c.accent; StatusKind.Warn -> c.warn; StatusKind.Bad -> c.bad; StatusKind.Idle -> c.tile3 }))
                if (t == null) {
                    Txt("Wird geplant …", style = AutoDjType.small, second = true)
                } else {
                    Txt(t.label, style = AutoDjType.bodyMedium, maxLines = 1)
                    Txt(
                        "${t.lengthBeats.toInt()} Beats" + (if (t.tempoRamp) String.format(Locale.US, " · %.0f → %.0f BPM", t.bpmFrom, t.bpmTo) else ""),
                        style = AutoDjType.small, maxLines = 1,
                    )
                    Box(Modifier.weight(1f))
                    if (t.note.isNotEmpty()) Txt(t.note, style = AutoDjType.small, second = true, maxLines = 1)
                }
            }
        }
    }
}

@Composable
private fun ListActions(dj: DjController, mix: MixController) {
    val ready = dj.tracks.count { it.ready }
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Btn("Alle Songs hinzufügen", onClick = { mix.addAll() }, primary = true, enabled = !mix.locked && ready > 0 && mix.list.size < ready)
        Btn("Leeren", onClick = { mix.clear() }, enabled = !mix.locked && mix.list.isNotEmpty())
    }
}

// ---- running, options, export -------------------------------------------------------------------------------

/** The green tile: what runs, how long it is still, stop / next; before the start the big "Mix starten". */
@Composable
private fun RunTile(mix: MixController, modifier: Modifier) {
    val playing = mix.state == MixState.Playing
    val c = Ds.c
    val s = mix.snapshot.plan
    Tile(modifier, kind = TileKind.Accent) {
        val cur = mix.currentIndex
        val name = mix.list.getOrNull(cur)?.let { mix.runningPlan?.items?.getOrNull(cur - mix.runningFrom)?.name }
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.Bottom) {
            Column(Modifier.weight(1f)) {
                Txt(
                    when {
                        playing -> "Läuft"
                        mix.state == MixState.Loading -> "Lädt"
                        mix.state == MixState.Finished -> "Fertig"
                        mix.list.isEmpty() -> "Bereit"
                        else -> "${mix.list.size} Songs"
                    },
                    style = AutoDjType.small,
                )
                Txt(
                    when {
                        playing && name != null -> name
                        mix.state == MixState.Loading -> mix.status ?: "Lade …"
                        mix.state == MixState.Finished -> "Der Mix ist durchgelaufen"
                        else -> mix.plan?.let { "${formatTime(it.totalSec)} Spielzeit" } ?: "Noch kein Mix"
                    },
                    style = AutoDjType.head, maxLines = 2,
                )
            }
            if (playing) Txt(formatTime((s.totalSec - s.timeSec).coerceAtLeast(0.0)), style = AutoDjType.display, maxLines = 1)
        }
        if (playing || mix.state == MixState.Loading) {
            val total = if (s.totalSec > 0) s.totalSec else 1.0
            ProgressBar(
                if (playing) (s.timeSec / total).toFloat() else 0f, Modifier.padding(top = 14.dp),
                track = c.onAccent.copy(alpha = .25f), fill = c.onAccent,
            )
        }
        Row(Modifier.fillMaxWidth().padding(top = 14.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            if (playing || mix.state == MixState.Stopping) {
                Btn("Stopp", onClick = { mix.stop() }, enabled = playing, on = false, danger = false)
                Btn("Nächster Song", onClick = { mix.skipToNext() }, enabled = playing && mix.currentIndex in 0 until mix.list.size - 1)
                Box(Modifier.weight(1f))
                if (s.waiting >= 0) Txt("wartet auf den nächsten Song …", style = AutoDjType.small, maxLines = 1)
                else mix.runningPlan?.transitions?.getOrNull(s.transition + 1)?.let { Txt("Übergang in ${formatTime((it.startSec - s.timeSec).coerceAtLeast(0.0))}", style = AutoDjType.small, maxLines = 1) }
            } else {
                Btn(
                    if (mix.state == MixState.Finished) "Noch einmal" else "Mix starten", onClick = { mix.start(0) },
                    enabled = mix.list.isNotEmpty() && mix.state != MixState.Loading && mix.exportProgress == null,
                )
            }
        }
        mix.error?.let { e ->
            Row(Modifier.padding(top = 10.dp), verticalAlignment = Alignment.CenterVertically) {
                Txt(e, style = AutoDjType.small, modifier = Modifier.weight(1f))
                Btn("OK", onClick = { mix.error = null }, small = true)
            }
        }
    }
}

@Composable
private fun OptionsTile(mix: MixController, modifier: Modifier) {
    Tile(modifier) {
        Txt("Übergänge", style = AutoDjType.lead)
        Segment(
            TransitionChoice.entries.map { it.label }, TransitionChoice.entries.indexOf(mix.transition),
            { if (!mix.locked) mix.chooseTransition(TransitionChoice.entries[it]) }, Modifier.padding(top = 10.dp), compact = true,
        )
        Txt(mix.transition.hint + ".", style = AutoDjType.small, second = true, modifier = Modifier.padding(top = 8.dp))
        Row(Modifier.fillMaxWidth().padding(top = 16.dp), verticalAlignment = Alignment.CenterVertically) {
            Txt("Länge", style = AutoDjType.lead, modifier = Modifier.weight(1f))
            Segment(
                LengthChoice.entries.map { it.label.removeSuffix(" Beats") }, LengthChoice.entries.indexOf(mix.length),
                { if (!mix.locked && mix.transition != TransitionChoice.Cut) mix.chooseLength(LengthChoice.entries[it]) },
            )
        }
        Row(Modifier.fillMaxWidth().padding(top = 12.dp), verticalAlignment = Alignment.CenterVertically) {
            Txt("Reihenfolge", style = AutoDjType.lead, modifier = Modifier.weight(1f))
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                val ok = !mix.locked && mix.list.size > 1
                Btn("Passend", onClick = { mix.optimiseOrder(OrderChoice.Smart) }, enabled = ok, small = true)
                Btn("Tempo ↑", onClick = { mix.optimiseOrder(OrderChoice.TempoUp) }, enabled = ok, small = true)
                Btn("Tempo ↓", onClick = { mix.optimiseOrder(OrderChoice.TempoDown) }, enabled = ok, small = true)
            }
        }
        Txt("Sortiert die Liste einmalig, danach änderst du sie von Hand.", style = AutoDjType.small, second = true, modifier = Modifier.padding(top = 6.dp))
    }
}

@Composable
private fun ExportTile(mix: MixController, onExport: (Int) -> Unit, modifier: Modifier) {
    var bits by remember { mutableIntStateOf(16) }
    Tile(modifier) {
        val p = mix.exportProgress
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Column(Modifier.weight(1f)) {
                Txt("Als Datei exportieren", style = AutoDjType.lead)
                Txt("Rendert den ganzen Mix schneller als Echtzeit als WAV, dazu eine Tracklist mit Zeiten.", style = AutoDjType.small, second = true)
            }
            if (p == null) {
                Segment(listOf("16 Bit", "24 Bit"), if (bits == 16) 0 else 1, { bits = if (it == 0) 16 else 24 }, small = true)
                Btn("Exportieren", onClick = { onExport(bits) }, primary = true, enabled = mix.list.isNotEmpty() && !mix.locked)
            }
        }
        if (p != null) {
            ProgressBar(p, Modifier.padding(top = 12.dp))
            Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                Txt(String.format(Locale.US, "%.0f %%", p * 100), style = AutoDjType.body, modifier = Modifier.weight(1f))
                Btn("Abbrechen", onClick = { mix.cancelExport() }, small = true)
            }
        }
        mix.exportResult?.let { Txt("Gespeichert: $it", style = AutoDjType.small, second = true, modifier = Modifier.padding(top = 8.dp)) }
    }
}

// ---- timeline -----------------------------------------------------------------------------------------------

/** Both decks as two lanes over the whole mix: song blocks in the decks' light colours, the overlaps as bands, the playhead. */
@Composable
private fun Timeline(plan: PlanInfo, now: Double?, modifier: Modifier) {
    val c = Ds.c
    val measurer = rememberTextMeasurer()
    Display(modifier) {
        Canvas(Modifier.fillMaxSize()) {
            val total = plan.totalSec.coerceAtLeast(1.0)
            val pad = 12.dp.toPx()
            val w = size.width - 2 * pad
            val laneH = (size.height - 2 * pad - 8.dp.toPx()) / 2f
            fun x(sec: Double) = pad + (sec / total * w).toFloat()
            plan.transitions.forEach { t -> drawRect(c.screen3, Offset(x(t.startSec), 0f), Size((t.lengthSec / total * w).toFloat().coerceAtLeast(2f), size.height)) }
            plan.items.forEach { it ->
                val lit = if (it.deck == 0) c.deckALit else c.deckBLit
                val y = pad + it.deck * (laneH + 8.dp.toPx())
                drawRoundRect(lit, Offset(x(it.startSec), y), Size(((it.endSec - it.startSec) / total * w).toFloat().coerceAtLeast(2f), laneH), CornerRadius(3.dp.toPx()))
                drawText(measurer, it.name, Offset(x(it.startSec) + 8.dp.toPx(), y + (laneH - 16.dp.toPx()) / 2f), TextStyle(fontFamily = AutoDjType.family, fontSize = 13.sp, color = Color(0xFF2A1D17)))
            }
            if (now != null) drawRect(Color.White, Offset(x(now) - 1.dp.toPx(), 0f), Size(2.dp.toPx(), size.height))
        }
    }
}
