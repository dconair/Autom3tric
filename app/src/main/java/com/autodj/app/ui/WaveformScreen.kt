package com.autodj.app.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import com.autodj.app.design.AutoDjType
import com.autodj.app.design.Display
import com.autodj.app.design.Ds
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autodj.app.audio.GrainVizSnapshot
import com.autodj.app.audio.SourceInfo
import java.util.Locale
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.sin

/**
 * The dark screen of the instrument.
 *   top 55 %    grain cloud: one lane per voice, every grain a small bar at its read position in the source
 *   bottom 45 % waveform of the source with the cell grid; labels = the key of each cell
 *   full height playhead per voice (where new grains start) and a faint plate behind the cell that is playing
 *
 * Touch = play: down starts the voice, moving changes position / cell, lifting stops it ([onTouch] gets the
 * horizontal position 0..1 and whether it is the first contact).
 */
@Composable
fun WaveformScreen(
    peaks: FloatArray?,
    info: SourceInfo?,
    viz: GrainVizSnapshot,
    voice: Int,
    cells: Int,
    onTouch: (x: Float, first: Boolean) -> Unit,
    onRelease: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val measurer = rememberTextMeasurer()
    val touch by rememberUpdatedState(onTouch)
    val release by rememberUpdatedState(onRelease)
    val c = Ds.c
    val labelStyle = AutoDjType.small.copy(color = c.screenDim)
    val headerStyle = AutoDjType.small.copy(color = c.screenDim)
    val header = info?.let { String.format(Locale.US, "%s · %.1f s · %.1f kHz", it.name, it.seconds, it.sampleRate / 1000.0) } ?: "no source"

    Display(modifier.fillMaxWidth()) {
        Canvas(
            Modifier
                .fillMaxSize()
                .semantics { contentDescription = "source overview, touch to play, drag to move" }
                .pointerInput(Unit) {
                    awaitEachGesture {
                        val down = awaitFirstDown()
                        touch((down.position.x / size.width).coerceIn(0f, 1f), true)
                        do {
                            val event = awaitPointerEvent()
                            val c = event.changes.firstOrNull() ?: break
                            if (c.pressed) {
                                touch((c.position.x / size.width).coerceIn(0f, 1f), false)
                                c.consume()
                            }
                        } while (c.pressed)
                        release()
                    }
                },
        ) {
            val w = size.width
            val h = size.height
            val split = h * 0.55f
            val n = cells.coerceAtLeast(1)

            // cells as alternating bands (no grid lines), the one being read from is lit; labels = the key of each cell
            val activeCell = viz.cell[voice.coerceIn(0, GrainVizSnapshot.MAX_VOICES - 1)]
            for (i in 0 until n) {
                val x = w * i / n
                if (i % 2 == 1) drawRect(c.screen2, Offset(x, 0f), Size(w / n, h))
                if (i == activeCell) drawRect(c.screen3, Offset(x, 0f), Size(w / n, h))
                if (w / n >= 40.dp.toPx() || i % 2 == 0) drawText(measurer, cellNoteName(i), Offset(x + 4.dp.toPx(), split + 4.dp.toPx()), labelStyle)
            }

            // grain cloud (top region)
            val lanes = GrainVizSnapshot.MAX_VOICES
            val top = 26.dp.toPx() // header band
            val laneH = (split - top - 6.dp.toPx()) / lanes
            for (g in viz.grains) {
                val v = g.voice.toInt().coerceIn(0, lanes - 1)
                val env = sin(PI.toFloat() * g.age.coerceIn(0f, 1f))
                val gw = (g.size * w * 0.05f).coerceIn(2.dp.toPx(), w * 0.2f)
                val gh = laneH * (0.28f + 0.55f * env)
                val cy = top + v * laneH + laneH * (0.5f + 0.3f * g.pan.coerceIn(-1f, 1f))
                val a = (0.12f + 0.6f * env).coerceIn(0f, 1f) * (if (v == voice) 1f else 0.6f)
                drawRoundRect(
                    VoiceColors[v].copy(alpha = a),
                    topLeft = Offset(g.pos.coerceIn(0f, 1f) * w - gw / 2f, cy - gh / 2f),
                    size = Size(gw, gh),
                    cornerRadius = CornerRadius(2.dp.toPx()),
                )
            }

            // waveform (bottom region)
            val mid = split + (h - split) * 0.58f
            val half = (h - split) * 0.34f
            if (peaks != null && peaks.size >= 2) {
                val cols = peaks.size / 2
                var norm = 0f
                for (v in peaks) norm = max(norm, abs(v))
                val scale = if (norm > 1e-6f) half / norm else 0f
                val px = w.toInt().coerceAtLeast(1)
                for (x in 0 until px) {
                    val col = (x.toLong() * cols / px).toInt().coerceIn(0, cols - 1)
                    // widen to the next column when the screen has more pixels than columns
                    val lo = peaks[2 * col] * scale
                    val hi = peaks[2 * col + 1] * scale
                    drawLine(c.screenInk, Offset(x + 0.5f, mid - hi - 0.5f), Offset(x + 0.5f, mid - lo + 0.5f), strokeWidth = 1f)
                }
            }

            // header
            drawText(measurer, header, Offset(14.dp.toPx(), 8.dp.toPx()), headerStyle)

            // playheads
            for (v in 0 until lanes) {
                val lvl = viz.voiceLevel[v].coerceIn(0f, 1f)
                if (lvl < 0.01f) continue
                val x = viz.playhead[v].coerceIn(0f, 1f) * w
                val a = if (v == voice) 0.95f else 0.45f
                drawLine(VoiceColors[v].copy(alpha = a), Offset(x, 0f), Offset(x, h), strokeWidth = if (v == voice) 2.dp.toPx() else 1.dp.toPx())
            }
        }
    }
}
