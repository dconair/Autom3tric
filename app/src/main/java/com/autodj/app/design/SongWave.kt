package com.autodj.app.design

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.clipRect
import androidx.compose.ui.unit.dp
import com.autodj.app.dj.Overview
import com.autodj.app.dj.TrackAnalysisInfo

/** How bright a section of a song is drawn in the section strip: drops are the loudest, intro and outro the quietest. */
private fun sectionTone(label: String): Float = when (label) {
    "drop" -> 1f
    "break" -> .45f
    "intro", "outro" -> .3f
    else -> .65f
}

/**
 * The waveform of a song on a display. Bars of music (four beats) alternate between two tones as bands instead of grid lines; the
 * part that has been played is in [lit], the rest at 38 %; the playhead is white. Underneath, a 6 dp strip shows the sections.
 * When a song is loaded the waveform draws itself from the left in 420 ms.
 *
 * [playSec] null: no playhead (library). [reveal] key: when it changes the reveal runs again.
 */
@Composable
fun SongWave(
    overview: Overview?,
    analysis: TrackAnalysisInfo,
    lit: Color,
    modifier: Modifier = Modifier,
    playSec: Double? = null,
    revealKey: Any? = analysis,
    showSections: Boolean = true,
    cues: List<Double> = emptyList(),
    loopFrom: Double = 0.0,
    loopTo: Double = 0.0,
) {
    val c = Ds.c
    val reveal = remember(revealKey) { Animatable(if (Ds.reduceMotion) 1f else 0f) }
    LaunchedEffect(revealKey) {
        if (reveal.value < 1f) reveal.animateTo(1f, tween(420, easing = AutoDjMotion.out))
    }
    Canvas(modifier.clipToBounds()) {
        val w = size.width
        val h = size.height
        val stripH = if (showSections) 6.dp.toPx() else 0f
        val gap = if (showSections) 3.dp.toPx() else 0f
        val waveH = h - stripH - gap
        val mid = waveH / 2f
        val dur = analysis.durationSec.coerceAtLeast(0.001)
        val playX = if (playSec != null) (playSec / dur * w).toFloat().coerceIn(0f, w) else -1f

        // bands: one tone per bar, alternating, only when the grid can be trusted
        if (analysis.hasTempo && analysis.gridConfidence >= 0.5 && analysis.barSec > 0.0) {
            var t = analysis.firstBarSec
            var k = 0
            val bandW = (analysis.barSec / dur * w).toFloat()
            while (t < dur) {
                val x = (t / dur * w).toFloat()
                if (k % 2 == 1) drawRect(c.screen2, Offset(x, 0f), Size(bandW, waveH))
                t += analysis.barSec
                k++
            }
        }
        clipRect(right = w * reveal.value) {
            if (overview != null && overview.columns > 0) {
                val n = overview.columns
                val stroke = (w / n * .72f).coerceAtLeast(1.2f)
                for (i in 0 until n) {
                    val x = (i + 0.5f) / n * w
                    val top = mid - overview.max[i] / 127f * mid
                    val bottom = mid - overview.min[i] / 127f * mid
                    val color = if (playSec != null && x > playX) lit.copy(alpha = .38f) else lit
                    drawLine(color, Offset(x, top), Offset(x, if (bottom - top < 1.5f) top + 1.5f else bottom), strokeWidth = stroke)
                }
            }
            if (showSections) {
                for (s in analysis.sections) {
                    val x0 = (s.startSec / dur * w).toFloat()
                    val x1 = (s.endSec / dur * w).toFloat()
                    val playing = playSec != null && playSec >= s.startSec && playSec < s.endSec
                    val tone = if (playing) 1f else sectionTone(s.label) * .8f
                    drawRect(
                        if (playing) c.accentLit else lit.copy(alpha = tone.coerceIn(.2f, 1f)),
                        Offset(x0 + 1.dp.toPx(), h - stripH), Size((x1 - x0 - 2.dp.toPx()).coerceAtLeast(1f), stripH),
                    )
                }
            }
        }
        if (loopTo > loopFrom) {
            val x0 = (loopFrom / dur * w).toFloat()
            val x1 = (loopTo / dur * w).toFloat()
            drawRect(c.accent.copy(alpha = .28f), Offset(x0, 0f), Size((x1 - x0).coerceAtLeast(2.dp.toPx()), waveH))
            drawRect(c.accent, Offset(x0, 0f), Size(2.dp.toPx(), waveH))
            drawRect(c.accent, Offset(x1 - 2.dp.toPx(), 0f), Size(2.dp.toPx(), waveH))
        }
        for (cue in cues) {
            val x = (cue / dur * w).toFloat()
            drawRect(c.accentLit, Offset(x - 1.dp.toPx(), 0f), Size(2.dp.toPx(), waveH * .38f))
        }
        if (playSec != null) drawRect(Color.White, Offset(playX - 1.dp.toPx(), 0f), Size(2.dp.toPx(), waveH))
    }
}
