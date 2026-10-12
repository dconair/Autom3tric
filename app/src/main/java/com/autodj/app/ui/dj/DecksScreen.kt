package com.autodj.app.ui.dj

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
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
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autodj.app.audio.DeckSnapshot
import com.autodj.app.dj.DeckController
import com.autodj.app.dj.DjColors
import com.autodj.app.dj.DjController
import com.autodj.app.dj.DjTheme
import com.autodj.app.dj.MixState
import com.autodj.app.dj.TrackItem
import com.autodj.app.dj.camelotColor
import java.util.Locale
import kotlin.math.abs
import kotlin.math.roundToInt

/**
 * The manual mixer: two decks with waveform, play / cue / sync / key lock, tempo, EQ with kill switches, filter and level
 * fader, plus the crossfader in the middle. Songs come from the library ("A" / "B" buttons there, or the picker on the deck).
 */
@Composable
fun DecksScreen(dj: DjController, onGoToLibrary: () -> Unit, modifier: Modifier = Modifier) {
    val decks = dj.decks
    DjTheme {
        BoxWithConstraints(modifier.fillMaxSize().background(DjColors.bg)) {
            val wide = maxWidth >= 980.dp
            val busy = dj.mix.state == MixState.Playing || dj.mix.state == MixState.Loading
            Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(if (wide) 16.dp else 10.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                if (busy) {
                    Row(
                        Modifier.fillMaxWidth().clip(RoundedCornerShape(8.dp)).background(DjColors.warn.copy(alpha = 0.15f)).padding(horizontal = 12.dp, vertical = 8.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Text("Der Auto-Mix läuft und belegt die Decks. Du kannst Lautstärke, EQ und Filter mitregeln.", color = DjColors.warn, fontSize = 13.sp, modifier = Modifier.weight(1f))
                        TextButton(onClick = { dj.mix.stop() }) { Text("Mix stoppen", color = DjColors.text) }
                    }
                }
                decks.error?.let { e ->
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                        Text(e, color = DjColors.warn, fontSize = 12.sp, modifier = Modifier.weight(1f))
                        TextButton(onClick = { decks.error = null }) { Text("OK", color = DjColors.muted) }
                    }
                }
                if (wide) {
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        DeckPanel(dj, 0, onGoToLibrary, Modifier.weight(1f))
                        DeckPanel(dj, 1, onGoToLibrary, Modifier.weight(1f))
                    }
                } else {
                    DeckPanel(dj, 0, onGoToLibrary, Modifier.fillMaxWidth())
                    DeckPanel(dj, 1, onGoToLibrary, Modifier.fillMaxWidth())
                }
                CrossfaderPanel(dj)
            }
        }
    }
}

@Composable
private fun deckColor(d: Int) = if (d == 0) DjColors.deckA else DjColors.deckB

@Composable
private fun DeckPanel(dj: DjController, d: Int, onGoToLibrary: () -> Unit, modifier: Modifier) {
    val decks = dj.decks
    val u = decks.deck[d]
    val st = dj.snapshot.deck[d]
    val item = u.item
    val a = item?.analysis
    val color = deckColor(d)
    val busy = dj.mix.state == MixState.Playing || dj.mix.state == MixState.Loading || dj.mix.state == MixState.Stopping
    Column(
        modifier.clip(RoundedCornerShape(12.dp)).background(DjColors.panel).border(1.dp, DjColors.line, RoundedCornerShape(12.dp)).padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        // header: deck letter, song, tempo, key
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Box(Modifier.size(34.dp).clip(RoundedCornerShape(8.dp)).background(color), contentAlignment = Alignment.Center) {
                Text(if (d == 0) "A" else "B", color = Color.Black, fontWeight = FontWeight.Bold, fontSize = 18.sp)
            }
            Column(Modifier.weight(1f)) {
                Text(
                    when {
                        u.loading -> "Lade …"
                        item != null -> item.name
                        else -> "Kein Song geladen"
                    },
                    color = DjColors.text, fontSize = 15.sp, fontWeight = FontWeight.Medium, maxLines = 1, overflow = TextOverflow.Ellipsis,
                )
                Text(
                    if (item != null && a != null) "${formatTime(st.posSec)} / ${formatTime(a.durationSec)}" + (if (st.ended) "  ·  Ende" else "") else "Wähle einen Song aus der Bibliothek",
                    color = DjColors.muted, fontSize = 12.sp,
                )
            }
            if (a != null && a.hasKey) Text(a.camelot, color = camelotColor(a.camelot), fontSize = 16.sp, fontWeight = FontWeight.Bold)
            Column(horizontalAlignment = Alignment.End) {
                Text(if (st.bpm > 0) String.format(Locale.US, "%.1f", st.bpm) else "–", color = DjColors.text, fontSize = 22.sp, fontWeight = FontWeight.SemiBold)
                Text("BPM", color = DjColors.muted, fontSize = 10.sp)
            }
            SongPicker(dj, d, enabled = !busy)
        }

        // waveform with playhead; tap or drag to jump
        DeckWaveform(item, st, color, Modifier.fillMaxWidth().height(72.dp)) { sec -> decks.seek(d, sec) }

        // transport
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
            Button(
                onClick = { decks.playPause(d) }, enabled = item != null && !busy,
                colors = ButtonDefaults.buttonColors(containerColor = if (st.playing) DjColors.panelRaised else color, contentColor = if (st.playing) DjColors.text else Color.Black),
                modifier = Modifier.width(124.dp),
            ) { Text(if (st.playing) "❚❚ Pause" else "▶ Play", fontWeight = FontWeight.SemiBold, maxLines = 1, softWrap = false) }
            OutlinedButton(onClick = { decks.cue(d) }, enabled = item != null && !busy) { Text("Cue", color = DjColors.text) }
            Toggle("Sync", decks.param(d, "sync").value > 0.5f, color, enabled = item != null) { decks.set(d, "sync", if (it) 1f else 0f) }
            Toggle("Key", decks.param(d, "keylock").value > 0.5f, color, enabled = true) { decks.set(d, "keylock", if (it) 1f else 0f) }
            Spacer(Modifier.weight(1f))
            if (item != null) TextButton(onClick = { decks.eject(d) }, enabled = !busy) { Text("Auswerfen", color = DjColors.muted) }
        }

        // tempo
        val tempo by decks.param(d, "tempo_pct")
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Tempo", color = DjColors.muted, fontSize = 12.sp, modifier = Modifier.width(44.dp))
            Slider(
                value = tempo, onValueChange = { decks.set(d, "tempo_pct", (it * 10).roundToInt() / 10f) }, valueRange = -16f..16f,
                modifier = Modifier.weight(1f), colors = sliderColors(color),
            )
            Text(String.format(Locale.US, "%+.1f %%", tempo), color = DjColors.text, fontSize = 12.sp, modifier = Modifier.width(56.dp).clickable { decks.reset(d, "tempo_pct") })
        }

        // mixer strip: EQ x3 + volume faders, filter below
        Row(horizontalArrangement = Arrangement.spacedBy(14.dp), verticalAlignment = Alignment.Top) {
            EqBand(decks, d, "eq_high_db", "High", color)
            EqBand(decks, d, "eq_mid_db", "Mid", color)
            EqBand(decks, d, "eq_low_db", "Low", color)
            Spacer(Modifier.weight(1f))
            Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text("Level", color = DjColors.muted, fontSize = 11.sp)
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    Meter(st, Modifier.width(10.dp).height(120.dp))
                    Fader(decks.param(d, "volume").value, 0f, 1f, 1f, color, Modifier.width(34.dp).height(120.dp)) { decks.set(d, "volume", it) }
                }
                Text("Volume", color = DjColors.muted, fontSize = 11.sp)
            }
        }
        val filter by decks.param(d, "filter")
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Filter", color = DjColors.muted, fontSize = 12.sp, modifier = Modifier.width(44.dp))
            Text("LP", color = DjColors.muted, fontSize = 10.sp)
            Slider(value = filter, onValueChange = { decks.set(d, "filter", if (abs(it) < 0.04f) 0f else it) }, valueRange = -1f..1f, modifier = Modifier.weight(1f), colors = sliderColors(color))
            Text("HP", color = DjColors.muted, fontSize = 10.sp)
        }
    }
}

@Composable
private fun sliderColors(color: Color) = SliderDefaults.colors(thumbColor = color, activeTrackColor = color, inactiveTrackColor = DjColors.line)

@Composable
private fun Toggle(label: String, on: Boolean, color: Color, enabled: Boolean, onChange: (Boolean) -> Unit) {
    Box(
        Modifier.clip(RoundedCornerShape(8.dp)).background(if (on) color.copy(alpha = 0.25f) else DjColors.panelRaised)
            .border(1.dp, if (on) color else DjColors.line, RoundedCornerShape(8.dp))
            .clickable(enabled = enabled, role = Role.Switch) { onChange(!on) }.padding(horizontal = 14.dp, vertical = 9.dp),
    ) { Text(label, color = if (!enabled) DjColors.line else if (on) DjColors.text else DjColors.muted, fontSize = 13.sp, fontWeight = FontWeight.Medium) }
}

@Composable
private fun SongPicker(dj: DjController, d: Int, enabled: Boolean) {
    var open by remember { mutableStateOf(false) }
    Box {
        OutlinedButton(onClick = { open = true }, enabled = enabled) { Text("Song …", color = DjColors.text) }
        DropdownMenu(expanded = open, onDismissRequest = { open = false }) {
            val ready = dj.tracks.filter { it.ready }
            if (ready.isEmpty()) DropdownMenuItem(text = { Text("Die Bibliothek ist leer") }, onClick = { open = false })
            for (t in ready) {
                DropdownMenuItem(
                    text = { Text(t.name + (t.analysis?.takeIf { it.hasTempo }?.let { String.format(Locale.US, "  ·  %.0f BPM", it.bpm) } ?: "")) },
                    onClick = {
                        open = false
                        dj.decks.load(d, t)
                    },
                )
            }
        }
    }
}

@Composable
private fun EqBand(decks: DeckController, d: Int, name: String, label: String, color: Color) {
    val db by decks.param(d, name)
    val killed = db <= -25.9f
    Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Text(label, color = DjColors.muted, fontSize = 11.sp)
        // -26 dB (kill) .. +6 dB, the notch at 0 dB is where a double tap resets
        Fader(db, -26f, 6f, 0f, color, Modifier.width(34.dp).height(120.dp)) { decks.set(d, name, it) }
        Text(if (killed) "Kill" else String.format(Locale.US, "%+.0f", db), color = if (killed) DjColors.bad else DjColors.text, fontSize = 11.sp)
        Box(
            Modifier.clip(RoundedCornerShape(5.dp)).background(if (killed) DjColors.bad.copy(alpha = 0.3f) else DjColors.panelRaised).clickable { decks.set(d, name, if (killed) 0f else -26f) }
                .padding(horizontal = 8.dp, vertical = 3.dp),
        ) { Text("Kill", color = if (killed) DjColors.text else DjColors.muted, fontSize = 10.sp) }
    }
}

/** A vertical fader: drag anywhere on it, double tap to go back to [neutral]. */
@Composable
private fun Fader(value: Float, min: Float, max: Float, neutral: Float, color: Color, modifier: Modifier, onChange: (Float) -> Unit) {
    var heightPx by remember { mutableStateOf(1f) }
    fun at(y: Float) = (max - (y / heightPx).coerceIn(0f, 1f) * (max - min))
    Box(
        modifier.onSizeChanged { heightPx = it.height.toFloat().coerceAtLeast(1f) }.clip(RoundedCornerShape(6.dp)).background(Color(0xFF0A0B0D))
            .pointerInput(min, max) { detectTapGestures(onDoubleTap = { onChange(neutral) }, onPress = { o -> onChange(at(o.y)) }) }
            .pointerInput(min, max) { detectDragGestures { change, _ -> change.consume(); onChange(at(change.position.y)) } },
    ) {
        BoxWithConstraints(Modifier.fillMaxSize()) {
            val frac = ((value - min) / (max - min)).coerceIn(0f, 1f)
            val neutralFrac = ((neutral - min) / (max - min)).coerceIn(0f, 1f)
            Box(Modifier.align(Alignment.Center).width(3.dp).fillMaxHeight().background(DjColors.line))
            Box(Modifier.offset(y = maxHeight * (1f - neutralFrac)).fillMaxWidth().height(1.dp).background(DjColors.muted.copy(alpha = 0.5f)))
            Box(
                Modifier.offset(y = maxHeight * (1f - frac) - 6.dp).align(Alignment.TopCenter).fillMaxWidth().height(12.dp)
                    .clip(RoundedCornerShape(3.dp)).background(color),
            )
        }
    }
}

@Composable
private fun Meter(st: DeckSnapshot, modifier: Modifier) {
    val level = maxOf(st.peakL, st.peakR).coerceIn(0f, 1.2f)
    BoxWithConstraints(modifier.clip(RoundedCornerShape(3.dp)).background(Color(0xFF0A0B0D))) {
        val h = maxHeight * (level / 1.2f)
        Box(Modifier.align(Alignment.BottomCenter).fillMaxWidth().height(h).background(if (level > 1.0f) DjColors.bad else if (level > 0.7f) DjColors.warn else DjColors.good))
    }
}

@Composable
private fun DeckWaveform(item: TrackItem?, st: DeckSnapshot, color: Color, modifier: Modifier, onSeek: (Double) -> Unit) {
    val a = item?.analysis
    var widthPx by remember { mutableStateOf(1f) }
    Box(
        modifier.onSizeChanged { widthPx = it.width.toFloat().coerceAtLeast(1f) }
            .pointerInput(a?.durationSec) {
                if (a != null) {
                    detectTapGestures { o -> onSeek((o.x / widthPx).coerceIn(0f, 1f) * a.durationSec) }
                }
            }
            .pointerInput(a?.durationSec) {
                if (a != null) {
                    detectDragGestures { change, _ -> change.consume(); onSeek((change.position.x / widthPx).coerceIn(0f, 1f) * a.durationSec) }
                }
            },
    ) {
        if (a != null) {
            WaveformStrip(item.overview, a, Modifier.fillMaxSize())
            BoxWithConstraints(Modifier.fillMaxSize()) {
                val x = maxWidth * (st.posSec / a.durationSec.coerceAtLeast(0.001)).toFloat().coerceIn(0f, 1f)
                Box(Modifier.offset(x = x).width(2.dp).fillMaxHeight().background(color))
            }
        } else {
            Box(Modifier.fillMaxSize().clip(RoundedCornerShape(6.dp)).background(Color(0xFF0A0B0D)), contentAlignment = Alignment.Center) {
                Text("—", color = DjColors.line)
            }
        }
    }
}

@Composable
private fun CrossfaderPanel(dj: DjController) {
    val decks = dj.decks
    val x by decks.param(-1, "crossfader")
    val curve by decks.param(-1, "crossfader_curve")
    val a = dj.snapshot.deck[0]
    val b = dj.snapshot.deck[1]
    Column(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(12.dp)).background(DjColors.panel).border(1.dp, DjColors.line, RoundedCornerShape(12.dp)).padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Text("A", color = DjColors.deckA, fontWeight = FontWeight.Bold)
            Slider(
                value = x, onValueChange = { decks.set(-1, "crossfader", if (abs(it) < 0.03f) 0f else it) }, valueRange = -1f..1f, modifier = Modifier.weight(1f),
                colors = SliderDefaults.colors(thumbColor = DjColors.text, activeTrackColor = DjColors.line, inactiveTrackColor = DjColors.line),
            )
            Text("B", color = DjColors.deckB, fontWeight = FontWeight.Bold)
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
            Text("Kurve", color = DjColors.muted, fontSize = 12.sp)
            listOf("Weich", "Gleiche Lautstärke", "Schnitt").forEachIndexed { i, label ->
                val on = curve.roundToInt() == i
                Box(
                    Modifier.clip(RoundedCornerShape(50)).background(if (on) DjColors.deckA.copy(alpha = 0.22f) else DjColors.panelRaised)
                        .border(1.dp, if (on) DjColors.deckA else DjColors.line, RoundedCornerShape(50)).clickable { decks.set(-1, "crossfader_curve", i.toFloat()) }
                        .padding(horizontal = 12.dp, vertical = 5.dp),
                ) { Text(label, color = if (on) DjColors.text else DjColors.muted, fontSize = 12.sp) }
            }
            Spacer(Modifier.weight(1f))
            BeatPhase(a, b, Modifier.width(220.dp).height(28.dp))
        }
    }
}

/** Both decks' position within the beat as dots on one line: when the dots sit on top of each other the beats are aligned. */
@Composable
private fun BeatPhase(a: DeckSnapshot, b: DeckSnapshot, modifier: Modifier) {
    BoxWithConstraints(modifier.clip(RoundedCornerShape(6.dp)).background(Color(0xFF0A0B0D))) {
        Box(Modifier.align(Alignment.CenterStart).offset(x = 0.dp).width(1.dp).fillMaxHeight().background(DjColors.line))
        Box(Modifier.align(Alignment.Center).width(1.dp).fillMaxHeight().background(DjColors.line))
        if (a.bpm > 0 && a.playing) Box(Modifier.offset(x = maxWidth * a.beatPhase.toFloat().coerceIn(0f, 1f) - 5.dp, y = 4.dp).size(10.dp).clip(RoundedCornerShape(50)).background(DjColors.deckA))
        if (b.bpm > 0 && b.playing) Box(Modifier.offset(x = maxWidth * b.beatPhase.toFloat().coerceIn(0f, 1f) - 5.dp, y = 14.dp).size(10.dp).clip(RoundedCornerShape(50)).background(DjColors.deckB))
    }
}
