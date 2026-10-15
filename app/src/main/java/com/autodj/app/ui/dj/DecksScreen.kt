package com.autodj.app.ui.dj

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
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
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.semantics.ProgressBarRangeInfo
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.progressBarRangeInfo
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.setProgress
import androidx.compose.ui.unit.dp
import com.autodj.app.audio.DeckSnapshot
import com.autodj.app.dj.DeckController
import com.autodj.app.dj.DjController
import com.autodj.app.dj.MixState
import com.autodj.app.dj.TrackItem
import com.autodj.app.design.AutoDjMotion
import com.autodj.app.design.AutoDjSpace
import com.autodj.app.design.AutoDjType
import com.autodj.app.design.BarFader
import com.autodj.app.design.Btn
import com.autodj.app.design.CapFader
import com.autodj.app.design.Display
import com.autodj.app.design.Ds
import com.autodj.app.design.KeyChip
import com.autodj.app.design.LevelStrip
import com.autodj.app.design.Segment
import com.autodj.app.design.SongWave
import com.autodj.app.design.Tile
import com.autodj.app.design.TileKind
import com.autodj.app.design.Txt
import com.autodj.app.design.riseIn
import com.autodj.app.ui.Knob
import com.autodj.app.ui.TabTip
import java.util.Locale
import kotlin.math.abs
import kotlin.math.roundToInt

/**
 * The manual mixer: two decks (head with letter, song and tempo; waveform display; controls) and the crossfader below. Songs come
 * from the library ("A" / "B" there) or from "Song wählen" on the display. On a phone one deck at a time, picked by a segment.
 */
@Composable
fun DecksScreen(dj: DjController, onGoToLibrary: () -> Unit, modifier: Modifier = Modifier) {
    val decks = dj.decks
    val busy = dj.mix.state == MixState.Playing || dj.mix.state == MixState.Loading
    BoxWithConstraints(modifier.fillMaxSize()) {
        val wide = maxWidth >= 900.dp
        val margin = if (wide) AutoDjSpace.margin else AutoDjSpace.phoneMargin
        val gap = if (wide) AutoDjSpace.gap else AutoDjSpace.phoneGap
        Column(Modifier.fillMaxSize().padding(horizontal = margin).padding(bottom = margin), verticalArrangement = Arrangement.spacedBy(gap)) {
            val notices = busy || decks.error != null
            if (notices) {
                Tile(Modifier.fillMaxWidth().riseIn(0), kind = TileKind.Ink, padding = 12.dp) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Txt(
                            decks.error ?: "Der Auto-Mix läuft und belegt die Decks. Lautstärke, EQ und Filter kannst du mitregeln.",
                            style = AutoDjType.small, modifier = Modifier.weight(1f),
                        )
                        if (decks.error != null) Btn("OK", onClick = { decks.error = null }, small = true) else Btn("Mix stoppen", onClick = { dj.mix.stop() }, small = true)
                    }
                }
            }
            if (wide) {
                Row(Modifier.weight(1f).fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(gap)) {
                    DeckColumn(dj, 0, busy, Modifier.weight(1f).fillMaxHeight(), onGoToLibrary, wide = true)
                    DeckColumn(dj, 1, busy, Modifier.weight(1f).fillMaxHeight(), onGoToLibrary, wide = true)
                }
            } else {
                var which by rememberSaveable { mutableIntStateOf(0) }
                Segment(listOf("Deck A", "Deck B"), which, { which = it }, Modifier.fillMaxWidth(), equalWidth = true)
                Column(Modifier.weight(1f).fillMaxWidth().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(gap)) {
                    DeckColumn(dj, which, busy, Modifier.fillMaxWidth(), onGoToLibrary, wide = false)
                }
            }
            CrossfaderTile(dj, wide)
        }
    }
}

private fun deckField(d: Int, c: com.autodj.app.design.AutoDjColors) = if (d == 0) c.deckAField else c.deckBField
private fun deckLit(d: Int, c: com.autodj.app.design.AutoDjColors) = if (d == 0) c.deckALit else c.deckBLit

@Composable
private fun DeckColumn(dj: DjController, d: Int, busy: Boolean, modifier: Modifier, onGoToLibrary: () -> Unit, wide: Boolean) {
    val decks = dj.decks
    val u = decks.deck[d]
    val st = dj.snapshot.deck[d]
    val item = u.item
    val a = item?.analysis
    val c = Ds.c
    val gap = if (wide) AutoDjSpace.gap else AutoDjSpace.phoneGap
    Column(modifier, verticalArrangement = Arrangement.spacedBy(gap)) {
        // head: letter field, song, key chip, tempo
        Tile(Modifier.fillMaxWidth().riseIn(if (d == 0) 0 else 1), pulse = st.playing, padding = if (wide) 16.dp else 12.dp) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Box(Modifier.size(if (wide) 56.dp else 44.dp).clip(com.autodj.app.design.AutoDjShape).background(deckField(d, c)), contentAlignment = Alignment.Center) {
                    Txt(if (d == 0) "A" else "B", style = AutoDjType.lead, color = Color.White)
                }
                Column(Modifier.weight(1f)) {
                    Txt(
                        when {
                            u.loading -> "Lade …"
                            item != null -> item.name
                            else -> "Kein Song geladen"
                        },
                        style = AutoDjType.lead, maxLines = 1,
                    )
                    Txt(
                        if (item != null && a != null) "${formatTime(st.posSec)} / ${formatTime(a.durationSec)}" + (if (st.ended) "  ·  Ende" else "") else "Wähle einen Song",
                        style = AutoDjType.small, second = true, maxLines = 1,
                    )
                }
                if (a != null && a.hasKey) KeyChip(a.camelot)
                Txt(if (st.bpm > 0) String.format(Locale.US, "%.1f", st.bpm) else "–", style = if (wide) AutoDjType.display else AutoDjType.displayPhone, maxLines = 1)
            }
        }
        // waveform display
        Display(Modifier.fillMaxWidth().then(if (wide) Modifier.weight(1f) else Modifier.height(150.dp)).riseIn(if (d == 0) 1 else 2)) {
            DeckWaveform(item, st, deckLit(d, c), Modifier.fillMaxSize()) { sec -> decks.seek(d, sec) }
            Row(Modifier.align(Alignment.TopEnd).padding(8.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                SongPicker(dj, d, enabled = !busy)
                if (item != null) Btn("Auswerfen", onClick = { decks.eject(d) }, small = true, enabled = !busy)
            }
            if (item == null && !u.loading) {
                Column(Modifier.align(Alignment.Center), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Txt("Noch kein Song", style = AutoDjType.small, second = true)
                    Btn("Zur Bibliothek", onClick = onGoToLibrary, small = true)
                }
            }
        }
        // controls
        Tile(Modifier.fillMaxWidth().riseIn(if (d == 0) 2 else 3), padding = if (wide) 16.dp else 12.dp) {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                Btn(
                    if (st.playing) "Pause" else "Play", onClick = { decks.playPause(d) }, primary = true, enabled = item != null && !busy,
                    modifier = Modifier.weight(1f).height(52.dp), style = AutoDjType.bodyMedium,
                )
                Btn("Cue", onClick = { decks.cue(d) }, enabled = item != null && !busy)
                Btn("Sync", onClick = { decks.set(d, "sync", if (decks.param(d, "sync").value > 0.5f) 0f else 1f) }, on = decks.param(d, "sync").value > 0.5f, enabled = item != null)
                Btn("Key", onClick = { decks.set(d, "keylock", if (decks.param(d, "keylock").value > 0.5f) 0f else 1f) }, on = decks.param(d, "keylock").value > 0.5f)
            }
            val tempo by decks.param(d, "tempo_pct")
            Row(Modifier.fillMaxWidth().padding(top = 14.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Txt("Tempo", style = AutoDjType.small, second = true)
                BarFader(
                    (tempo + 16f) / 32f, { decks.set(d, "tempo_pct", (((it * 32f) - 16f) * 10f).roundToInt() / 10f) }, Modifier.weight(1f),
                    bipolar = true, label = "Tempo", valueText = String.format(Locale.US, "%+.1f Prozent", tempo), onReset = { decks.reset(d, "tempo_pct") },
                )
                Txt(String.format(Locale.US, "%+.1f %%", tempo), style = AutoDjType.small, modifier = Modifier.width(56.dp))
            }
            Row(Modifier.fillMaxWidth().padding(top = 14.dp), horizontalArrangement = Arrangement.spacedBy(if (wide) 12.dp else 8.dp), verticalAlignment = Alignment.Top) {
                EqBand(decks, d, "eq_high_db", "High", wide)
                EqBand(decks, d, "eq_mid_db", "Mid", wide)
                EqBand(decks, d, "eq_low_db", "Low", wide)
                FilterKnob(decks, d, wide, Modifier.weight(1f))
                Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Row(Modifier.height(if (wide) 132.dp else 112.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        LevelStrip(maxOf(st.peakL, st.peakR) / 1.2f, Modifier.fillMaxHeight())
                        CapFader(
                            decks.param(d, "volume").value, { decks.set(d, "volume", it) }, Modifier.fillMaxHeight(), label = "Level",
                            valueText = String.format(Locale.US, "%.0f Prozent", decks.param(d, "volume").value * 100f), onReset = { decks.reset(d, "volume") },
                        )
                    }
                    Txt("Level", style = AutoDjType.small, second = true)
                }
            }
        }
    }
}

@Composable
private fun EqBand(decks: DeckController, d: Int, name: String, label: String, wide: Boolean) {
    val db by decks.param(d, name)
    val killed = db <= -25.9f
    Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(6.dp)) {
        // -26 dB (kill) .. +6 dB; the notch at 0 dB is where a double tap resets
        CapFader(
            (db + 26f) / 32f, { decks.set(d, name, it * 32f - 26f) }, Modifier.height(if (wide) 132.dp else 112.dp),
            label = "EQ $label", valueText = if (killed) "Kill" else String.format(Locale.US, "%+.0f dB", db), killed = killed,
            onReset = { decks.reset(d, name) },
        )
        Txt(label, style = AutoDjType.small, second = true)
        Btn("Kill", onClick = { decks.set(d, name, if (killed) 0f else -26f) }, small = true, danger = killed, modifier = Modifier.width(if (wide) 46.dp else 44.dp), description = "Kill $label")
    }
}

@Composable
private fun FilterKnob(decks: DeckController, d: Int, wide: Boolean, modifier: Modifier) {
    val filter by decks.param(d, "filter")
    Column(modifier, horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Box(Modifier.height(if (wide) 132.dp else 112.dp), contentAlignment = Alignment.Center) {
            Knob(
                label = "", valueText = "", value = (filter + 1f) / 2f,
                onValueChange = { v -> val f = v * 2f - 1f; decks.set(d, "filter", if (abs(f) < 0.04f) 0f else f) },
                size = if (wide) 96.dp else 80.dp, onReset = { decks.reset(d, "filter") },
            )
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Txt("LP", style = AutoDjType.small, second = true)
            Txt("Filter", style = AutoDjType.small, second = true)
            Txt("HP", style = AutoDjType.small, second = true)
        }
    }
}

@Composable
private fun SongPicker(dj: DjController, d: Int, enabled: Boolean) {
    var open by remember { mutableStateOf(false) }
    Box {
        Btn("Song wählen", onClick = { open = true }, small = true, enabled = enabled)
        DropdownMenu(expanded = open, onDismissRequest = { open = false }) {
            val ready = dj.tracks.filter { it.ready }
            if (ready.isEmpty()) DropdownMenuItem(text = { Txt("Die Bibliothek ist leer", color = Ds.c.ink) }, onClick = { open = false })
            for (t in ready) {
                DropdownMenuItem(
                    text = { Txt(t.name + (t.analysis?.takeIf { it.hasTempo }?.let { String.format(Locale.US, "  ·  %.0f BPM", it.bpm) } ?: ""), color = Ds.c.ink) },
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
private fun DeckWaveform(item: TrackItem?, st: DeckSnapshot, lit: Color, modifier: Modifier, onSeek: (Double) -> Unit) {
    val a = item?.analysis
    var widthPx by remember { mutableStateOf(1f) }
    Box(
        modifier
            .onSizeChanged { widthPx = it.width.toFloat().coerceAtLeast(1f) }
            .pointerInput(a?.durationSec) {
                if (a != null) detectTapGestures { o -> onSeek((o.x / widthPx).coerceIn(0f, 1f) * a.durationSec) }
            }
            .pointerInput(a?.durationSec) {
                if (a != null) detectDragGestures { change, _ -> change.consume(); onSeek((change.position.x / widthPx).coerceIn(0f, 1f) * a.durationSec) }
            },
    ) {
        if (a != null && item != null) SongWave(item.overview, a, lit, Modifier.fillMaxSize(), playSec = st.posSec, revealKey = item.id)
    }
}

// ---- crossfader ---------------------------------------------------------------------------------------------------

@Composable
private fun CrossfaderTile(dj: DjController, wide: Boolean) {
    val decks = dj.decks
    val x by decks.param(-1, "crossfader")
    val curve by decks.param(-1, "crossfader_curve")
    val a = dj.snapshot.deck[0]
    val b = dj.snapshot.deck[1]
    Tile(Modifier.fillMaxWidth().riseIn(4), padding = if (wide) 16.dp else 12.dp) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            val sideA by androidx.compose.animation.core.animateFloatAsState(if (x > 0.2f) .55f else 1f, androidx.compose.animation.core.tween(AutoDjMotion.ms(AutoDjMotion.fast)), label = "side-a")
            val sideB by androidx.compose.animation.core.animateFloatAsState(if (x < -0.2f) .55f else 1f, androidx.compose.animation.core.tween(AutoDjMotion.ms(AutoDjMotion.fast)), label = "side-b")
            Txt("A", style = AutoDjType.lead, modifier = Modifier.alpha(sideA))
            Crossfader(x, { decks.set(-1, "crossfader", if (abs(it) < 0.03f) 0f else it) }, Modifier.weight(1f), onReset = { decks.set(-1, "crossfader", 0f) })
            Txt("B", style = AutoDjType.lead, modifier = Modifier.alpha(sideB))
        }
        Row(Modifier.fillMaxWidth().padding(top = 10.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Txt("Kurve", style = AutoDjType.small, second = true)
            Segment(listOf("Weich", "Gleiche Lautstärke", "Schnitt"), curve.roundToInt(), { decks.set(-1, "crossfader_curve", it.toFloat()) }, small = true)
            Box(Modifier.weight(1f))
            if (wide) BeatPhase(a, b, Modifier.width(220.dp).height(28.dp))
        }
    }
}

/** The crossfader: a 40 dp track with a 64 dp cap. */
@Composable
private fun Crossfader(value: Float, onChange: (Float) -> Unit, modifier: Modifier, onReset: () -> Unit) {
    val c = Ds.c
    val current by rememberUpdatedState(onChange)
    val v = value.coerceIn(-1f, 1f)
    val shown by androidx.compose.animation.core.animateFloatAsState(v, androidx.compose.animation.core.tween(AutoDjMotion.ms(AutoDjMotion.instant)), label = "xfade")
    Canvas(
        modifier
            .height(40.dp)
            .semantics(mergeDescendants = true) {
                contentDescription = "Crossfader"
                progressBarRangeInfo = ProgressBarRangeInfo((v + 1f) / 2f, 0f..1f)
                setProgress { t ->
                    current((t * 2f - 1f).coerceIn(-1f, 1f))
                    true
                }
            }
            .pointerInput(Unit) {
                val cap = 64.dp.toPx()
                fun at(x: Float) = (((x - cap / 2f) / (size.width - cap)) * 2f - 1f).coerceIn(-1f, 1f)
                detectDragGestures(onDragStart = { current(at(it.x)) }, onDrag = { ch, _ -> ch.consume(); current(at(ch.position.x)) })
            }
            .pointerInput(Unit) { detectTapGestures(onDoubleTap = { onReset() }) },
    ) {
        drawRoundRect(c.tile2, size = size, cornerRadius = CornerRadius(6.dp.toPx()))
        val cap = 64.dp.toPx()
        val x = (shown + 1f) / 2f * (size.width - cap)
        drawRoundRect(c.ink, Offset(x, 0f), Size(cap, size.height), CornerRadius(5.dp.toPx()))
        drawRect(c.onInk.copy(alpha = .6f), Offset(x + cap / 2f - 1.dp.toPx(), size.height * .25f), Size(2.dp.toPx(), size.height * .5f))
    }
}

/** Both decks' position within the beat as dots on one line: when the dots sit on top of each other the beats are aligned. */
@Composable
private fun BeatPhase(a: DeckSnapshot, b: DeckSnapshot, modifier: Modifier) {
    val c = Ds.c
    Canvas(modifier.clip(com.autodj.app.design.AutoDjShape)) {
        drawRect(c.screen)
        drawRect(c.screen3, Offset(0f, 0f), Size(2.dp.toPx(), size.height))
        drawRect(c.screen3, Offset(size.width / 2f - 1.dp.toPx(), 0f), Size(2.dp.toPx(), size.height))
        val r = 5.dp.toPx()
        if (a.bpm > 0 && a.playing) drawCircle(c.deckALit, r, Offset(size.width * a.beatPhase.toFloat().coerceIn(0f, 1f), size.height * .32f))
        if (b.bpm > 0 && b.playing) drawCircle(c.deckBLit, r, Offset(size.width * b.beatPhase.toFloat().coerceIn(0f, 1f), size.height * .68f))
    }
}
