package com.autodj.app.ui.dj

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autodj.app.dj.DjColors
import com.autodj.app.dj.DjController
import com.autodj.app.dj.DjTheme
import com.autodj.app.dj.PAD_COUNT
import com.autodj.app.dj.PadNames
import com.autodj.app.dj.PadPatterns
import com.autodj.app.dj.PerformanceController
import com.autodj.app.dj.STEP_COUNT
import com.autodj.app.dj.padKind
import java.util.Locale
import kotlin.math.roundToInt

/** Colour of a pad by kind: drums, hats, percussion, effects. */
internal fun padColor(pad: Int): Color = when (padKind(pad)) {
    0 -> Color(0xFFF06292)
    1 -> Color(0xFFFFD54F)
    2 -> Color(0xFF4FC3F7)
    else -> Color(0xFFB39DDB)
}

/**
 * Drum pads and the step sequencer. The sixteen pads play synthesised drum sounds when tapped; the sequencer plays the grid below
 * in time with the music (a deck, the auto mix) or, when nothing plays, with its own tempo. Step 1 is the first beat of a bar.
 */
@Composable
fun PadsScreen(dj: DjController, modifier: Modifier = Modifier) {
    val perf = dj.perf
    DjTheme {
        BoxWithConstraints(modifier.fillMaxSize().background(DjColors.bg)) {
            val wide = maxWidth >= 900.dp
            Column(
                Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(if (wide) 16.dp else 10.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                ClockBanner(dj)
                SequencerBar(perf)
                if (wide) {
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp), verticalAlignment = Alignment.Top) {
                        PadGrid(perf, Modifier.weight(0.4f))
                        StepGrid(dj, Modifier.weight(0.6f))
                    }
                } else {
                    PadGrid(perf, Modifier.fillMaxWidth())
                    StepGrid(dj, Modifier.fillMaxWidth())
                }
            }
        }
    }
}

@Composable
private fun SequencerBar(perf: PerformanceController) {
    val seqOn = perf.param("pads.seq_on").value >= 0.5f
    Column(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(12.dp)).background(DjColors.panel).border(1.dp, if (seqOn) DjColors.good.copy(alpha = 0.7f) else DjColors.line, RoundedCornerShape(12.dp)).padding(14.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Column(Modifier.weight(1f)) {
                Text("Sequencer", color = DjColors.text, fontSize = 17.sp, fontWeight = FontWeight.SemiBold)
                Text(
                    if (seqOn) "Läuft im Takt der Musik" else "Aus. Die Pads kannst du trotzdem von Hand spielen.",
                    color = DjColors.muted, fontSize = 12.sp,
                )
            }
            SwitchChip(if (seqOn) "Läuft" else "Start", seqOn, DjColors.good) { perf.toggle("pads.seq_on") }
        }
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Text("Muster", color = DjColors.muted, fontSize = 12.sp, modifier = Modifier.width(100.dp), maxLines = 1)
            Row(Modifier.weight(1f).horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                for (p in PadPatterns) {
                    val on = perf.patternName == p.name
                    Box(
                        Modifier.clip(RoundedCornerShape(50)).background(if (on) DjColors.good.copy(alpha = 0.22f) else DjColors.panelRaised)
                            .border(1.dp, if (on) DjColors.good else DjColors.line, RoundedCornerShape(50))
                            .clickable { perf.loadPattern(p) }.padding(horizontal = 12.dp, vertical = 5.dp),
                    ) { Text(p.name, color = if (on) DjColors.text else DjColors.muted, fontSize = 12.sp, maxLines = 1) }
                }
            }
        }
        FxSlider("Lautstärke", perf.param("pads.volume").value, 0f, 1.5f, DjColors.good, { "${(it * 100).roundToInt()} %" }) { perf.set("pads.volume", it) }
        FxSlider("Swing", perf.param("pads.swing").value, 0f, 0.6f, DjColors.good, { "${(it / 0.6f * 100).roundToInt()} %" }) { perf.set("pads.swing", it) }
        FxSlider("Freier Takt", perf.param("transport.bpm").value, 60f, 200f, DjColors.good, { String.format(Locale.US, "%.0f", it) }) { perf.set("transport.bpm", it.roundToInt().toFloat()) }
    }
}

@Composable
private fun PadGrid(perf: PerformanceController, modifier: Modifier) {
    Column(
        modifier.clip(RoundedCornerShape(12.dp)).background(DjColors.panel).border(1.dp, DjColors.line, RoundedCornerShape(12.dp)).padding(10.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        for (r in 0 until 4) {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                for (c in 0 until 4) Pad(perf, r * 4 + c, Modifier.weight(1f).aspectRatio(1f))
            }
        }
    }
}

@Composable
private fun Pad(perf: PerformanceController, pad: Int, modifier: Modifier) {
    val color = padColor(pad)
    var pressed by remember { mutableStateOf(false) }
    Box(
        modifier.clip(RoundedCornerShape(10.dp)).background(if (pressed) color else color.copy(alpha = 0.20f))
            .border(1.dp, color.copy(alpha = if (pressed) 1f else 0.6f), RoundedCornerShape(10.dp))
            .pointerInput(pad) {
                detectTapGestures(onPress = {
                    pressed = true
                    perf.hit(pad)
                    try {
                        tryAwaitRelease()
                    } finally {
                        pressed = false
                    }
                })
            },
        contentAlignment = Alignment.Center,
    ) {
        Text(PadNames[pad], color = if (pressed) Color.Black else DjColors.text, fontSize = 13.sp, fontWeight = FontWeight.Medium, maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.padding(horizontal = 4.dp))
    }
}

private val LabelWidth = 74.dp

@Composable
private fun StepGrid(dj: DjController, modifier: Modifier) {
    val perf = dj.perf
    Column(
        modifier.clip(RoundedCornerShape(12.dp)).background(DjColors.panel).border(1.dp, DjColors.line, RoundedCornerShape(12.dp)).padding(10.dp),
        verticalArrangement = Arrangement.spacedBy(2.dp),
    ) {
        Row(Modifier.fillMaxWidth()) {
            Box(Modifier.width(LabelWidth))
            for (s in 0 until STEP_COUNT) {
                Text(
                    "${s / 4 + 1}".takeIf { s % 4 == 0 } ?: "·",
                    color = if (s % 4 == 0) DjColors.text else DjColors.muted, fontSize = 10.sp, modifier = Modifier.weight(1f),
                    textAlign = androidx.compose.ui.text.style.TextAlign.Center,
                )
            }
        }
        Box {
            Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                for (pad in 0 until PAD_COUNT) StepRow(perf, pad)
            }
            Playhead(dj)
        }
    }
}

@Composable
private fun StepRow(perf: PerformanceController, pad: Int) {
    val color = padColor(pad)
    val mask = perf.rows[pad]
    Row(Modifier.fillMaxWidth().height(22.dp), verticalAlignment = Alignment.CenterVertically) {
        Text(
            PadNames[pad], color = if (mask != 0) DjColors.text else DjColors.muted, fontSize = 11.sp, maxLines = 1, overflow = TextOverflow.Ellipsis,
            modifier = Modifier.width(LabelWidth).clickable { perf.hit(pad) }.padding(end = 4.dp),
        )
        for (s in 0 until STEP_COUNT) {
            val on = (mask shr s) and 1 != 0
            Box(
                Modifier.weight(1f).fillMaxHeight().padding(horizontal = 1.dp)
                    .clip(RoundedCornerShape(3.dp))
                    .background(if (on) color else if ((s / 4) % 2 == 0) DjColors.panelRaised else Color(0xFF252932))
                    .clickable { perf.toggleStep(pad, s) },
            )
        }
    }
}

/** The moving column that shows the step the sequencer is on. Reads the status itself, so only this thin overlay recomposes. */
@Composable
private fun BoxScope.Playhead(dj: DjController) {
    val step by remember(dj) { derivedStateOf { dj.snapshot.padStep } }
    if (step < 0) return
    BoxWithConstraints(Modifier.matchParentSize().padding(start = LabelWidth)) {
        val cell = maxWidth / STEP_COUNT
        Box(Modifier.offset(x = cell * step).width(cell).fillMaxHeight().background(Color.White.copy(alpha = 0.16f)))
    }
}
