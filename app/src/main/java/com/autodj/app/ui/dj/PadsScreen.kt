package com.autodj.app.ui.dj

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.lerp
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autodj.app.dj.DjController
import com.autodj.app.dj.PAD_COUNT
import com.autodj.app.dj.PadNames
import com.autodj.app.dj.PadPatterns
import com.autodj.app.dj.PerformanceController
import com.autodj.app.dj.STEP_COUNT
import com.autodj.app.dj.padKind
import com.autodj.app.design.AutoDjColors
import com.autodj.app.design.AutoDjSpace
import com.autodj.app.design.AutoDjType
import com.autodj.app.design.Btn
import com.autodj.app.design.Display
import com.autodj.app.design.Ds
import com.autodj.app.design.Pad
import com.autodj.app.design.Segment
import com.autodj.app.design.Tile
import com.autodj.app.design.Txt
import com.autodj.app.design.riseIn
import com.autodj.app.ui.Knob
import kotlin.math.roundToInt

/** Keys of the pads on a computer keyboard, row by row (the layout is the same on every keyboard: 1234 / QWER / ASDF / YXCV). */
val PadKeys = listOf("1", "2", "3", "4", "Q", "W", "E", "R", "A", "S", "D", "F", "Y", "X", "C", "V")

/** Pad groups: 0 kick, 1 snare and clap, 2 hi-hats and shaker, 3 percussion, 4 effects. */
internal fun padGroup(pad: Int): Int = when (pad) {
    0, 1 -> 0
    2, 3 -> 1
    4, 5, 7 -> 2
    6, 8, 9, 10, 11 -> 3
    else -> 4
}

private fun groupField(g: Int, c: AutoDjColors) = when (g) { 0 -> c.padKick; 1 -> c.padSnare; 2 -> c.padHat; 3 -> c.padPerc; else -> c.padFx }
private fun groupLit(g: Int, c: AutoDjColors) = when (g) { 0 -> c.padKickLit; 1 -> c.padSnareLit; 2 -> c.padHatLit; 3 -> c.padPercLit; else -> c.padFxLit }
private fun groupText(g: Int, c: AutoDjColors) = if (g == 4) c.ink else Color.White

/**
 * Drum pads and the step sequencer. The sixteen pads play synthesised drum sounds when tapped; the sequencer plays the grid in time
 * with the music (a deck, the auto mix) or, when nothing plays, with its own tempo. Step 1 is the first beat of a bar.
 */
@Composable
fun PadsScreen(dj: DjController, modifier: Modifier = Modifier, showKeys: Boolean = false) {
    val perf = dj.perf
    BoxWithConstraints(modifier.fillMaxSize()) {
        val wide = maxWidth >= 900.dp
        val margin = if (wide) AutoDjSpace.margin else AutoDjSpace.phoneMargin
        val gap = if (wide) AutoDjSpace.gap else AutoDjSpace.phoneGap
        if (wide) {
            Column(Modifier.fillMaxSize().padding(horizontal = margin).padding(bottom = margin), verticalArrangement = Arrangement.spacedBy(gap)) {
                SequencerBar(perf, Modifier.fillMaxWidth().riseIn(0), wide = true)
                Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(gap)) {
                    PadGrid(perf, showKeys, gap, Modifier.weight(5f).fillMaxHeight().riseIn(1))
                    StepDisplay(dj, Modifier.weight(7f).fillMaxHeight().riseIn(2), labels = true)
                }
            }
        } else {
            Column(
                Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(horizontal = margin).padding(bottom = margin),
                verticalArrangement = Arrangement.spacedBy(gap),
            ) {
                SequencerBar(perf, Modifier.fillMaxWidth().riseIn(0), wide = false)
                PadGrid(perf, showKeys, gap, Modifier.fillMaxWidth().height(360.dp).riseIn(1))
                StepDisplay(dj, Modifier.fillMaxWidth().height(300.dp).riseIn(2), labels = false)
            }
        }
    }
}

@Composable
private fun SequencerBar(perf: PerformanceController, modifier: Modifier, wide: Boolean) {
    val seqOn = perf.param("pads.seq_on").value >= 0.5f
    Tile(modifier, padding = 12.dp) {
        val pattern: @Composable () -> Unit = { Segment(PadPatterns.map { it.name }, PadPatterns.indexOfFirst { it.name == perf.patternName }.let { if (it < 0) -1 else it }, { perf.loadPattern(PadPatterns[it]) }, small = false) }
        val knobs: @Composable () -> Unit = {
            Row(horizontalArrangement = Arrangement.spacedBy(20.dp), verticalAlignment = Alignment.CenterVertically) {
                val vol = perf.param("pads.volume").value
                SmallKnob("Lautstärke", "${(vol * 100).roundToInt()} %", vol / 1.5f, { perf.set("pads.volume", it * 1.5f) }, { perf.set("pads.volume", 0.5f) })
                val swing = perf.param("pads.swing").value
                SmallKnob("Swing", "${(swing / 0.6f * 100).roundToInt()} %", swing / 0.6f, { perf.set("pads.swing", it * 0.6f) }, { perf.set("pads.swing", 0f) })
                if (wide) {
                    val bpm = perf.param("transport.bpm").value
                    SmallKnob("Freier Takt", "${bpm.roundToInt()}", (bpm - 60f) / 140f, { perf.set("transport.bpm", (60f + it * 140f).roundToInt().toFloat()) }, { perf.set("transport.bpm", 120f) })
                }
            }
        }
        val start: @Composable (Modifier) -> Unit = { m ->
            Btn(if (seqOn) "Läuft" else "Start", onClick = { perf.toggle("pads.seq_on") }, primary = seqOn, on = false, modifier = m, description = "Sequencer ${if (seqOn) "stoppen" else "starten"}")
        }
        if (wide) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                pattern()
                Box(Modifier.weight(1f))
                knobs()
                start(Modifier.padding(start = 20.dp).width(112.dp))
            }
        } else {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState())) { pattern() }
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    knobs()
                    Box(Modifier.weight(1f))
                    start(Modifier.width(96.dp))
                }
            }
        }
    }
}

/** A 40 dp knob with its name and value next to it. */
@Composable
private fun SmallKnob(name: String, value: String, v: Float, onChange: (Float) -> Unit, onReset: () -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        Knob("", "", v.coerceIn(0f, 1f), onChange, size = 40.dp, onReset = onReset)
        Column {
            Txt(name, style = AutoDjType.small, second = true, maxLines = 1)
            Txt(value, style = AutoDjType.small, maxLines = 1)
        }
    }
}

@Composable
private fun PadGrid(perf: PerformanceController, showKeys: Boolean, gap: androidx.compose.ui.unit.Dp, modifier: Modifier) {
    val c = Ds.c
    Column(modifier, verticalArrangement = Arrangement.spacedBy(gap)) {
        for (r in 0 until 4) {
            Row(Modifier.weight(1f).fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(gap)) {
                for (col in 0 until 4) {
                    val pad = r * 4 + col
                    val g = padGroup(pad)
                    Pad(
                        PadNames[pad], if (showKeys) PadKeys[pad] else "", groupField(g, c), groupText(g, c),
                        onPress = { perf.hit(pad) }, onRelease = {}, modifier = Modifier.weight(1f).fillMaxHeight(),
                    )
                }
            }
        }
    }
}

/** The sequencer: 16 rows by 16 steps on a display. Bars of four steps alternate in tone; the step the sequencer is on is a lit column. */
@Composable
private fun StepDisplay(dj: DjController, modifier: Modifier, labels: Boolean) {
    val perf = dj.perf
    val measurer = rememberTextMeasurer()
    val c = Ds.c
    Display(modifier) {
        Canvas(
            Modifier
                .fillMaxSize()
                .semantics { contentDescription = "Sequencer, 16 Pads mal 16 Schritte" }
                .pointerInput(labels) {
                    detectTapGestures { pos ->
                        val left = if (labels) 104.dp.toPx() else 10.dp.toPx()
                        val top = 34.dp.toPx()
                        val cw = (size.width - left - 10.dp.toPx()) / STEP_COUNT
                        val rh = (size.height - top - 10.dp.toPx()) / PAD_COUNT
                        val s = ((pos.x - left) / cw).toInt()
                        val p = ((pos.y - top) / rh).toInt()
                        if (s in 0 until STEP_COUNT && p in 0 until PAD_COUNT) perf.toggleStep(p, s)
                    }
                },
        ) {
            val left = if (labels) 104.dp.toPx() else 10.dp.toPx()
            val top = 34.dp.toPx()
            val cw = (size.width - left - 10.dp.toPx()) / STEP_COUNT
            val rh = (size.height - top - 10.dp.toPx()) / PAD_COUNT
            val gap = 2.dp.toPx()
            val step = dj.snapshot.padStep
            val small = TextStyle(fontFamily = AutoDjType.family, fontSize = 13.sp, color = c.screenDim)
            for (b in 0 until 4) {
                val x = left + b * 4 * cw
                drawText(measurer, "${b + 1}", Offset(x + 2.dp.toPx(), 8.dp.toPx()), small.copy(color = if (step in b * 4 until b * 4 + 4) c.screenInk else c.screenDim))
            }
            for (p in 0 until PAD_COUNT) {
                val g = padGroup(p)
                val y = top + p * rh
                val mask = perf.rows[p]
                if (labels) {
                    drawText(
                        measurer, PadNames[p], Offset(10.dp.toPx(), y + (rh - 16.dp.toPx()) / 2f),
                        small.copy(color = if (mask != 0) c.screenInk else c.screenDim, fontSize = 13.sp),
                    )
                }
                for (s in 0 until STEP_COUNT) {
                    val x = left + s * cw
                    val on = (mask shr s) and 1 != 0
                    val band = if ((s / 4) % 2 == 0) c.screen3 else c.screen2
                    val here = s == step
                    val color = when {
                        on && here -> lerp(groupLit(g, c), Color.White, .6f)
                        on -> groupLit(g, c)
                        here -> lerp(band, c.screenInk, .16f)
                        else -> band
                    }
                    drawRoundRect(color, Offset(x + gap / 2f, y + gap / 2f), Size(cw - gap, rh - gap), CornerRadius(3.dp.toPx()))
                }
            }
        }
    }
}
