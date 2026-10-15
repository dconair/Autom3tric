package com.autodj.app.ui.dj

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
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
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autodj.app.dj.DjColors
import com.autodj.app.ui.TabTip
import com.autodj.app.dj.DjController
import com.autodj.app.dj.DjTheme
import com.autodj.app.dj.LengthChoice
import com.autodj.app.dj.MixController
import com.autodj.app.dj.MixState
import com.autodj.app.dj.OrderChoice
import com.autodj.app.dj.PlanInfo
import com.autodj.app.dj.TrackItem
import com.autodj.app.dj.TransitionChoice
import com.autodj.app.dj.camelotColor
import java.util.Locale

/**
 * The Auto-Mix tab: put songs in a list, let the app plan the transitions, start the mix, tune it, export it.
 * [onExport] opens the platform's "save as" dialog for the given bit depth and calls [MixController.export] with the path.
 */
@Composable
fun MixScreen(dj: DjController, onGoToLibrary: () -> Unit, onExport: (bits: Int) -> Unit, modifier: Modifier = Modifier) {
    val mix = dj.mix
    DjTheme {
        BoxWithConstraints(modifier.fillMaxSize().background(DjColors.bg)) {
            val wide = maxWidth >= 860.dp
            if (wide) {
                Row(Modifier.fillMaxSize().padding(16.dp), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                    Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                        TabTip(
                            "mix", "Der Auto-Mix spielt die Liste nacheinander und blendet selbst über: Tempo und Takt werden angeglichen. Songs hinzufügen, ▶ drücken, fertig.",
                            lesson = com.autodj.app.ui.Guides.AUTO_DJ, step = 1,
                        )
                        ListHeader(dj, mix, onGoToLibrary, narrow = false)
                        LazyColumn(Modifier.weight(1f).fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(2.dp)) { mixRows(dj, mix) }
                    }
                    Column(Modifier.width(360.dp).fillMaxHeight().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                        TransportPanel(mix)
                        OptionsPanel(mix)
                        ExportPanel(mix, onExport)
                    }
                }
            } else {
                LazyColumn(Modifier.fillMaxSize().padding(horizontal = 12.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    item { Spacer(Modifier.height(2.dp)) }
                    item {
                        TabTip(
                            "mix", "Der Auto-Mix spielt die Liste nacheinander und blendet selbst über. Songs hinzufügen, ▶ drücken, fertig.",
                            lesson = com.autodj.app.ui.Guides.AUTO_DJ, step = 1,
                        )
                    }
                    item { TransportPanel(mix) }
                    item { ListHeader(dj, mix, onGoToLibrary, narrow = true) }
                    mixRows(dj, mix)
                    item { OptionsPanel(mix) }
                    item { ExportPanel(mix, onExport) }
                    item { Spacer(Modifier.height(12.dp)) }
                }
            }
        }
    }
}

// ---- list ---------------------------------------------------------------------------------------------------

@Composable
private fun ListHeader(dj: DjController, mix: MixController, onGoToLibrary: () -> Unit, narrow: Boolean) {
    val total = mix.plan?.totalSec
    val ready = dj.tracks.count { it.ready }
    val title: @Composable (Modifier) -> Unit = { m ->
        Column(m) {
            Text("Auto-Mix", color = DjColors.text, fontSize = 22.sp, fontWeight = FontWeight.SemiBold)
            Text(
                when {
                    mix.list.isEmpty() -> "Noch keine Songs in der Liste"
                    total != null -> "${mix.list.size} Songs · ${formatTime(total)} Spielzeit"
                    else -> "${mix.list.size} Songs"
                },
                color = DjColors.muted, fontSize = 13.sp,
            )
        }
    }
    val buttons: @Composable () -> Unit = {
        OutlinedButton(onClick = { mix.addAll() }, enabled = !mix.locked && ready > 0 && mix.list.size < ready) {
            Text("Alle Songs hinzufügen", color = DjColors.text, maxLines = 1)
        }
        TextButton(onClick = { mix.clear() }, enabled = !mix.locked && mix.list.isNotEmpty()) { Text("Leeren", color = DjColors.muted, maxLines = 1) }
        TextButton(onClick = onGoToLibrary) { Text("Bibliothek", color = DjColors.deckA, maxLines = 1) }
    }
    if (narrow) {
        Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            title(Modifier)
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) { buttons() }
        }
    } else {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            title(Modifier.weight(1f))
            buttons()
        }
    }
}

private fun LazyListScope.mixRows(dj: DjController, mix: MixController) {
    val plan = mix.runningPlan ?: mix.plan
    val from = if (mix.runningPlan != null) mix.runningFrom else 0
    val current = mix.currentIndex
    if (mix.list.isEmpty()) {
        item {
            Column(
                Modifier.fillMaxWidth().clip(RoundedCornerShape(10.dp)).background(DjColors.panel).padding(24.dp),
                horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Text("Die Mix-Liste ist leer", color = DjColors.text, fontSize = 17.sp)
                Text(
                    "Füge Songs aus der Bibliothek hinzu („In die Mix-Liste“) oder nimm gleich alle. " +
                        "Die App ordnet die Übergänge selbst: Tempo wird angeglichen, die Takte liegen aufeinander.",
                    color = DjColors.muted, fontSize = 13.sp,
                )
                Button(onClick = { mix.addAll() }, enabled = dj.tracks.any { it.ready }) { Text("Alle Songs aus der Bibliothek hinzufügen") }
            }
        }
        return
    }
    itemsIndexed(mix.list, key = { _, id -> id }) { i, id ->
        val item = dj.item(id)
        val planIndex = i - from
        Column {
            if (i > 0) {
                val t = if (planIndex >= 1 && plan != null) plan.transitions.getOrNull(planIndex - 1) else null
                TransitionStrip(
                    if (t == null) null else "${t.label} · ${t.lengthBeats.toInt()} Beats" +
                        (if (t.tempoRamp) String.format(Locale.US, " · %.0f → %.0f BPM", t.bpmFrom, t.bpmTo) else ""),
                    t?.note,
                    active = mix.state == MixState.Playing && mix.snapshot.plan.transition == planIndex - 1 && planIndex >= 1,
                )
            }
            MixRow(
                index = i, item = item, locked = mix.locked, playing = i == current,
                startsAt = if (plan != null && planIndex in plan.items.indices) plan.items[planIndex].startSec else null,
                onUp = { mix.move(i, -1) }, onDown = { mix.move(i, 1) }, onRemove = { mix.removeAt(i) }, onPlayFromHere = { mix.start(i) },
                canPlay = mix.state != MixState.Loading && mix.state != MixState.Stopping && mix.exportProgress == null,
            )
        }
    }
}

@Composable
private fun TransitionStrip(text: String?, note: String?, active: Boolean) {
    Row(Modifier.fillMaxWidth().padding(start = 40.dp, top = 2.dp, bottom = 2.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Box(Modifier.width(3.dp).height(14.dp).background(if (active) DjColors.warn else DjColors.line))
        Text(text ?: "…", color = if (active) DjColors.warn else DjColors.muted, fontSize = 12.sp, fontWeight = if (active) FontWeight.SemiBold else FontWeight.Normal)
        if (!note.isNullOrEmpty()) Text(note, color = DjColors.muted.copy(alpha = 0.7f), fontSize = 11.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

@Composable
private fun MixRow(
    index: Int, item: TrackItem?, locked: Boolean, playing: Boolean, startsAt: Double?, canPlay: Boolean,
    onUp: () -> Unit, onDown: () -> Unit, onRemove: () -> Unit, onPlayFromHere: () -> Unit,
) {
    val a = item?.analysis
    Row(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(8.dp)).background(if (playing) DjColors.panelRaised else DjColors.panel)
            .border(1.dp, if (playing) DjColors.deckA else DjColors.line, RoundedCornerShape(8.dp)).padding(horizontal = 10.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Text(if (playing) "▶" else "${index + 1}", color = if (playing) DjColors.deckA else DjColors.muted, fontSize = 14.sp, modifier = Modifier.width(22.dp))
        Column(Modifier.weight(1f)) {
            Text(item?.name ?: "(entfernt)", color = DjColors.text, fontSize = 14.sp, fontWeight = FontWeight.Medium, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text(
                buildString {
                    if (a != null) append(formatTime(a.durationSec))
                    if (startsAt != null) append(" · ab ").append(formatTime(startsAt))
                },
                color = DjColors.muted, fontSize = 11.sp,
            )
        }
        if (a != null) {
            if (a.hasKey) {
                val c = camelotColor(a.camelot)
                Text(a.camelot, color = c, fontSize = 13.sp, fontWeight = FontWeight.Bold)
            }
            Text(if (a.hasTempo) String.format(Locale.US, "%.1f", a.bpm) else "–", color = DjColors.text, fontSize = 14.sp, modifier = Modifier.width(44.dp))
        }
        SmallButton("▶", enabled = canPlay && item != null, onClick = onPlayFromHere)
        SmallButton("↑", enabled = !locked && index > 0, onClick = onUp)
        SmallButton("↓", enabled = !locked, onClick = onDown)
        SmallButton("✕", enabled = !locked, onClick = onRemove)
    }
}

@Composable
private fun SmallButton(label: String, enabled: Boolean, onClick: () -> Unit) {
    Box(
        Modifier.width(30.dp).height(30.dp).clip(RoundedCornerShape(6.dp)).background(DjColors.panelRaised)
            .clickable(enabled = enabled, role = Role.Button, onClick = onClick),
        contentAlignment = Alignment.Center,
    ) { Text(label, color = if (enabled) DjColors.text else DjColors.line, fontSize = 14.sp) }
}

// ---- transport + timeline -------------------------------------------------------------------------------------

@Composable
private fun TransportPanel(mix: MixController) {
    Panel {
        val playing = mix.state == MixState.Playing
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            if (playing || mix.state == MixState.Stopping) {
                Button(onClick = { mix.stop() }, enabled = mix.state == MixState.Playing, colors = ButtonDefaults.buttonColors(containerColor = DjColors.bad)) { Text("■  Stopp") }
                OutlinedButton(onClick = { mix.skipToNext() }, enabled = playing && mix.currentIndex in 0 until mix.list.size - 1) { Text("Nächster Song", color = DjColors.text) }
            } else {
                Button(onClick = { mix.start(0) }, enabled = mix.list.isNotEmpty() && mix.state != MixState.Loading && mix.exportProgress == null) {
                    Text(if (mix.state == MixState.Finished) "▶  Noch einmal" else "▶  Mix starten")
                }
            }
            Spacer(Modifier.weight(1f))
            val s = mix.snapshot.plan
            if (playing) {
                Text("${formatTime(s.timeSec)} / ${formatTime(s.totalSec)}", color = DjColors.text, fontSize = 14.sp, maxLines = 1, softWrap = false)
            }
        }
        when {
            mix.state == MixState.Loading -> {
                Text(mix.status ?: "Lade …", color = DjColors.muted, fontSize = 12.sp)
                LinearProgressIndicator(Modifier.fillMaxWidth().height(3.dp), color = DjColors.deckA, trackColor = DjColors.line)
            }
            playing -> {
                val s = mix.snapshot.plan
                val total = if (s.totalSec > 0) s.totalSec else 1.0
                LinearProgressIndicator(progress = { (s.timeSec / total).toFloat().coerceIn(0f, 1f) }, modifier = Modifier.fillMaxWidth().height(3.dp), color = DjColors.deckA, trackColor = DjColors.line)
                val cur = mix.currentIndex
                val name = mix.list.getOrNull(cur)?.let { id -> mix.runningPlan?.items?.getOrNull(cur - mix.runningFrom)?.name }
                Text(
                    buildString {
                        if (name != null) append("Läuft: ").append(name)
                        if (s.bpm > 0) append(String.format(Locale.US, "  ·  %.1f BPM", s.bpm))
                        if (s.waiting >= 0) append("  ·  wartet auf den nächsten Song …")
                    },
                    color = DjColors.muted, fontSize = 12.sp, maxLines = 2, overflow = TextOverflow.Ellipsis,
                )
            }
            mix.state == MixState.Finished -> Text("Der Mix ist durchgelaufen.", color = DjColors.good, fontSize = 12.sp)
        }
        val plan = mix.runningPlan ?: mix.plan
        if (plan != null) {
            Timeline(plan, if (playing) mix.snapshot.plan.timeSec else null, Modifier.fillMaxWidth().height(64.dp))
        }
        mix.error?.let { e ->
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(e, color = DjColors.warn, fontSize = 12.sp, modifier = Modifier.weight(1f))
                TextButton(onClick = { mix.error = null }) { Text("OK", color = DjColors.muted) }
            }
        }
    }
}

/** Both decks as two lanes over the whole mix: song blocks, the overlaps highlighted, the playhead. */
@Composable
private fun Timeline(plan: PlanInfo, now: Double?, modifier: Modifier) {
    val total = plan.totalSec.coerceAtLeast(1.0)
    BoxWithConstraints(modifier.clip(RoundedCornerShape(6.dp)).background(Color(0xFF0A0B0D))) {
        fun x(sec: Double): Dp = maxWidth * (sec / total).toFloat()
        val laneH = (maxHeight - 6.dp) / 2
        plan.transitions.forEach { t ->
            Box(Modifier.offset(x = x(t.startSec)).width(x(t.lengthSec).coerceAtLeast(2.dp)).fillMaxHeight().background(DjColors.warn.copy(alpha = 0.16f)))
        }
        plan.items.forEach { it ->
            val c = if (it.deck == 0) DjColors.deckA else DjColors.deckB
            Box(
                Modifier.offset(x = x(it.startSec), y = 2.dp + (laneH + 2.dp) * it.deck).width((x(it.endSec - it.startSec) - 1.dp).coerceAtLeast(1.dp)).height(laneH)
                    .clip(RoundedCornerShape(3.dp)).background(c.copy(alpha = 0.55f)),
            ) { Text(it.name, color = Color.Black, fontSize = 9.sp, maxLines = 1, overflow = TextOverflow.Clip, modifier = Modifier.padding(horizontal = 3.dp, vertical = 1.dp)) }
        }
        if (now != null) Box(Modifier.offset(x = x(now)).width(2.dp).fillMaxHeight().background(Color.White))
    }
}

// ---- options + export ---------------------------------------------------------------------------------------

@Composable
private fun OptionsPanel(mix: MixController) {
    Panel {
        Text("Übergänge", color = DjColors.text, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
        ChipRow(TransitionChoice.entries, mix.transition, { it.label }, !mix.locked) { mix.chooseTransition(it) }
        Text(mix.transition.hint, color = DjColors.muted, fontSize = 11.sp)
        Text("Länge der Überblendung", color = DjColors.text, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
        ChipRow(LengthChoice.entries, mix.length, { it.label }, !mix.locked && mix.transition != TransitionChoice.Cut) { mix.chooseLength(it) }
        Text("Reihenfolge optimieren", color = DjColors.text, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
        ChipRow(OrderChoice.entries, null, { it.label }, !mix.locked && mix.list.size > 1) { mix.optimiseOrder(it) }
        Text("Sortiert die Liste einmalig; danach kannst du sie von Hand ändern.", color = DjColors.muted, fontSize = 11.sp)
    }
}

@OptIn(androidx.compose.foundation.layout.ExperimentalLayoutApi::class)
@Composable
private fun <T> ChipRow(options: List<T>, selected: T?, label: (T) -> String, enabled: Boolean, onSelect: (T) -> Unit) {
    androidx.compose.foundation.layout.FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        options.forEach { o ->
            val on = o == selected
            Box(
                Modifier.clip(RoundedCornerShape(50)).background(if (on) DjColors.deckA.copy(alpha = 0.22f) else DjColors.panelRaised)
                    .border(1.dp, if (on) DjColors.deckA else DjColors.line, RoundedCornerShape(50))
                    .clickable(enabled = enabled, role = Role.RadioButton) { onSelect(o) }.padding(horizontal = 12.dp, vertical = 6.dp),
            ) { Text(label(o), color = if (!enabled) DjColors.muted.copy(alpha = 0.5f) else if (on) DjColors.text else DjColors.muted, fontSize = 12.sp) }
        }
    }
}

@Composable
private fun ExportPanel(mix: MixController, onExport: (Int) -> Unit) {
    var bits by remember { mutableIntStateOf(16) }
    Panel {
        Text("Als Datei exportieren", color = DjColors.text, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
        Text("Rendert den ganzen Mix schneller als in Echtzeit in eine WAV-Datei, dazu eine Tracklist mit Zeiten.", color = DjColors.muted, fontSize = 11.sp)
        val p = mix.exportProgress
        if (p == null) {
            ChipRow(listOf(16, 24), bits, { "$it Bit" }, true) { bits = it }
            Button(onClick = { onExport(bits) }, enabled = mix.list.isNotEmpty() && !mix.locked) { Text("WAV exportieren …") }
            mix.exportResult?.let { Text("Gespeichert: $it", color = DjColors.good, fontSize = 12.sp) }
        } else {
            LinearProgressIndicator(progress = { p }, modifier = Modifier.fillMaxWidth().height(4.dp), color = DjColors.deckA, trackColor = DjColors.line)
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(String.format(Locale.US, "%.0f %%", p * 100), color = DjColors.text, fontSize = 13.sp, modifier = Modifier.weight(1f))
                TextButton(onClick = { mix.cancelExport() }) { Text("Abbrechen", color = DjColors.muted) }
            }
        }
    }
}

@Composable
private fun Panel(content: @Composable androidx.compose.foundation.layout.ColumnScope.() -> Unit) {
    Column(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(10.dp)).background(DjColors.panel).border(1.dp, DjColors.line, RoundedCornerShape(10.dp)).padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp), content = content,
    )
}
