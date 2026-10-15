package com.autodj.app.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autodj.app.dj.PATTERN_REST
import com.autodj.app.dj.PATTERN_STEPS
import com.autodj.app.dj.PatternPresets
import kotlin.math.roundToInt

private const val SYNC_DIV_OF_STEP_DIV_16TH = 2  // "1/16" in the sync enum
private val SyncDivOfStepDiv = intArrayOf(-1, -1, -1, 0, 1, 2)  // step 2 bars .. 1/16 -> sync 1/4, 1/8, 1/16 where there is one

/** Length of one step in milliseconds at [bpm], for the step division with [index] (see native stepDivisionBeats). */
fun stepMillis(index: Int, bpm: Float): Float {
    val beats = floatArrayOf(8f, 4f, 2f, 1f, 0.5f, 0.25f)[index.coerceIn(0, 5)]
    return beats * 60_000f / bpm.coerceAtLeast(20f)
}

/** What the 16 steps add up to, in words: "1 Takt", "2 Takte", "4 Takte" ... */
fun patternLengthText(stepDivIndex: Int): String {
    val bars = floatArrayOf(8f, 4f, 2f, 1f, 0.5f, 0.25f)[stepDivIndex.coerceIn(0, 5)] * PATTERN_STEPS / 4f
    return when {
        bars == 1f -> "1 Takt"
        bars == 0.5f -> "1/2 Takt"
        else -> "${bars.roundToInt()} Takte"
    }
}

/**
 * The beat starter: sets the voice up so that a pattern is crisp and a beginner hears a rhythm at once. Drums as source (unless
 * the voice already plays a file of the user), 16 cells, one grain per step with a short fade (no smear, no click), the pattern
 * "Synkope", and playing.
 */
fun startBeat(host: InstrumentHost, p: InstrumentParams, voice: Int, userSourceSlot: Boolean) {
    if (!userSourceSlot) p.source.set(1f)  // drums
    p.mode.set(1f)                         // step
    p.cellCount.set(1f)                    // 16 cells
    p.stepDiv.set(4f)                      // 1/8: a step is half a beat, two bars for the 16 steps
    p.seqOn.set(1f)
    p.syncMode.set(1f)                     // grains exactly on the grid
    p.syncDiv.set(1f)                      // 1/8: one grain per step
    val bpm = host.spec("transport.bpm")?.let { host.paramState(it).value } ?: 120f
    p.sizeMs.set(stepMillis(4, bpm).coerceIn(p.sizeMs.spec.min, p.sizeMs.spec.max))
    p.position.set(0f)                     // start of the slice
    p.sprayMs.set(0f)
    p.jitter.set(0f)
    p.envShape.set(4f)                     // "square": 3 % fades, hits stay punchy and do not click
    p.pitch.set(0f)
    host.pattern?.load(voice, PatternPresets[3], 16)
    p.gate.set(1f)
}

/**
 * The beat builder: a grid of 16 steps (columns) by the cells of the source (rows). A dot says "in this step play this slice of the
 * file"; a column without a dot is a rest. Presets give a start, a tap changes a dot, the playing step is lit.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun BeatPanel(host: InstrumentHost, p: InstrumentParams, voice: Int, cells: Int, userSourceSlot: Boolean, modifier: Modifier = Modifier) {
    val pattern = host.pattern ?: return
    val bpmSpec = host.spec("transport.bpm")
    val bpm = bpmSpec?.let { host.paramState(it).value } ?: 120f
    val shape = RoundedCornerShape(14.dp)
    val stepNames = p.stepDiv.spec.enumNames
    val stepIdx = p.stepDiv.index.coerceIn(0, stepNames.size - 1)
    val playing = p.gate.value >= 0.5f
    Column(
        modifier.fillMaxWidth().clip(shape).border(1.dp, Palette.outline, shape).padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Column(Modifier.weight(1f)) {
                Text("Beat bauen", color = Palette.ink, fontSize = 16.sp, fontWeight = FontWeight.SemiBold)
                Text(
                    "Jede Spalte ist ein Schritt, jede Zeile ein Stück deiner Datei. Tippe Punkte ins Raster.",
                    color = Palette.muted, fontSize = 12.sp, lineHeight = 16.sp,
                )
            }
            OutlinedPill(
                if (playing) "■ Stopp" else "▶ Abspielen", onClick = { p.gate.set(if (playing) 0f else 1f) }, selected = playing,
                help = InstrumentHelp.Play,
            )
        }
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp), itemVerticalAlignment = Alignment.CenterVertically) {
            OutlinedPill("Beat-Start", onClick = { startBeat(host, p, voice, userSourceSlot) }, selected = false, help = InstrumentHelp.BeatStarter)
            for (preset in PatternPresets) {
                OutlinedPill(
                    preset.name, onClick = { pattern.load(voice, preset, cells) }, selected = pattern.name(voice) == preset.name,
                    help = "${preset.name}: ${preset.hint}. ${InstrumentHelp.BeatPresets}",
                )
            }
        }
        FlowRow(horizontalArrangement = Arrangement.spacedBy(10.dp), verticalArrangement = Arrangement.spacedBy(8.dp), itemVerticalAlignment = Alignment.CenterVertically) {
            Text("Schritt", color = Palette.muted, fontSize = 13.sp)
            Stepper(
                stepNames[stepIdx],
                onPrev = { p.stepDiv.set(((stepIdx + stepNames.size - 1) % stepNames.size).toFloat()) },
                onNext = { p.stepDiv.set(((stepIdx + 1) % stepNames.size).toFloat()) },
                help = InstrumentHelp.BeatStep,
            )
            Text("16 Schritte = ${patternLengthText(stepIdx)}", color = Palette.muted, fontSize = 13.sp)
            if (bpmSpec != null) {
                Text("Tempo", color = Palette.muted, fontSize = 13.sp, modifier = Modifier.padding(start = 6.dp))
                if (host.followsMusic) {
                    Text("folgt der Musik", color = Palette.good, fontSize = 13.sp)
                } else {
                    Stepper(
                        "${bpm.roundToInt()}",
                        onPrev = { host.setParam(bpmSpec, bpm - 2f) },
                        onNext = { host.setParam(bpmSpec, bpm + 2f) },
                        help = InstrumentHelp.BeatTempo,
                    )
                }
            }
        }
        StepGrid(host, voice, cells)
        if (pattern.isEmpty(voice)) {
            Text("Das Raster ist leer: Tippe einen Punkt, oder wähle oben eine Vorlage.", color = Palette.warn, fontSize = 12.sp)
        }
    }
}

@Composable
private fun StepGrid(host: InstrumentHost, voice: Int, cells: Int) {
    val pattern = host.pattern ?: return
    val measurer = rememberTextMeasurer()
    val rowHeight = if (cells > 8) 13.dp else 22.dp
    val labelStyle = TextStyle(color = Palette.muted, fontSize = 10.sp)
    val color = VoiceColors[voice.coerceIn(0, VoiceColors.size - 1)]
    val activeStep = host.viz.step[voice.coerceIn(0, 3)]
    val toggle by rememberUpdatedState({ step: Int, cell: Int -> pattern.toggle(voice, step, cell) })
    val gutter = 22.dp
    // read the cells in composition, so that the grid redraws when the pattern changes
    val picks = IntArray(PATTERN_STEPS) { pattern.cell(voice, it).let { c -> if (c == PATTERN_REST) PATTERN_REST else c % cells } }
    Canvas(
        Modifier
            .fillMaxWidth()
            .height(rowHeight * cells + 16.dp)
            .semantics { contentDescription = "Beat-Raster, 16 Schritte mal $cells Stücke" }
            .pointerInput(cells, voice) {
                detectTapGestures { pos ->
                    val g = gutter.toPx()
                    val top = 16.dp.toPx()
                    val cw = (size.width - g) / PATTERN_STEPS
                    val rh = rowHeight.toPx()
                    val step = ((pos.x - g) / cw).toInt()
                    val cell = ((pos.y - top) / rh).toInt()
                    if (step in 0 until PATTERN_STEPS && cell in 0 until cells) toggle(step, cell)
                }
            },
    ) {
        val g = gutter.toPx()
        val top = 16.dp.toPx()
        val cw = (size.width - g) / PATTERN_STEPS
        val rh = rowHeight.toPx()
        for (s in 0 until PATTERN_STEPS) {
            val x = g + s * cw
            // beat numbers above every fourth column
            if (s % 4 == 0) {
                val label = "${s / 4 + 1}"
                drawText(measurer, label, Offset(x + 2f, 0f), labelStyle)
            }
            val shade = if ((s / 4) % 2 == 0) Palette.bodySunken.copy(alpha = 0.55f) else Palette.bodySunken.copy(alpha = 0.25f)
            drawRoundRect(shade, Offset(x + 1f, top), Size(cw - 2f, rh * cells), CornerRadius(3f))
            if (s == activeStep) drawRoundRect(Palette.ink.copy(alpha = 0.18f), Offset(x + 1f, top), Size(cw - 2f, rh * cells), CornerRadius(3f))
        }
        for (r in 0 until cells) {
            if (cells <= 8 || r % 4 == 0) drawText(measurer, "${r + 1}", Offset(2f, top + r * rh + (rh - 12f) / 2f), labelStyle)
            drawLine(Palette.outline.copy(alpha = 0.25f), Offset(g, top + r * rh), Offset(size.width, top + r * rh), 1f)
        }
        for (s in 0 until PATTERN_STEPS) {
            val c = picks[s]
            if (c == PATTERN_REST) continue
            val x = g + s * cw
            drawRoundRect(if (s == activeStep) Color.White else color, Offset(x + 2f, top + c * rh + 1.5f), Size(cw - 4f, rh - 3f), CornerRadius(4f))
        }
    }
}
