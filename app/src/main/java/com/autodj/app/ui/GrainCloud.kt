package com.autodj.app.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.background
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.autodj.app.audio.GrainVizSnapshot
import kotlin.math.PI
import kotlin.math.sin

/**
 * Live picture of the grain cloud: x = read position in the source, one lane per voice, bar width ~ grain
 * duration, brightness follows the grain envelope, vertical offset follows the stereo position.
 */
@Composable
fun GrainCloud(viz: GrainVizSnapshot, selectedVoice: Int, modifier: Modifier = Modifier) {
    val bg = Palette.screen
    Canvas(
        modifier
            .fillMaxWidth()
            .height(150.dp)
            .clip(RoundedCornerShape(12.dp))
            .background(bg),
    ) {
        val lanes = GrainVizSnapshot.MAX_VOICES
        val laneH = size.height / lanes
        for (v in 0 until lanes) {
            val y0 = v * laneH
            drawRect(
                color = Color.White.copy(alpha = if (v == selectedVoice) 0.07f else 0.025f),
                topLeft = Offset(0f, y0 + 1f),
                size = Size(size.width, laneH - 2f),
            )
            val ph = viz.playhead[v]
            val lvl = viz.voiceLevel[v].coerceIn(0f, 1f)
            if (lvl > 0.01f) {
                drawRect(
                    color = VoiceColors[v].copy(alpha = 0.35f + 0.5f * lvl),
                    topLeft = Offset(ph.coerceIn(0f, 1f) * size.width - 1.5f, y0 + 2f),
                    size = Size(3f, laneH - 4f),
                )
            }
        }
        for (g in viz.grains) {
            val v = g.voice.coerceIn(0, lanes - 1)
            val x = g.pos.coerceIn(0f, 1f) * size.width
            val w = (g.size * size.width * 0.08f).coerceIn(2f, size.width * 0.25f)
            val env = sin(PI.toFloat() * g.age.coerceIn(0f, 1f))
            val cy = v * laneH + laneH * (0.5f + 0.35f * g.pan.coerceIn(-1f, 1f))
            drawRect(
                color = VoiceColors[v].copy(alpha = (0.15f + 0.8f * env).coerceIn(0f, 1f)),
                topLeft = Offset(x - w / 2f, cy - laneH * 0.18f * (0.4f + env)),
                size = Size(w, laneH * 0.36f * (0.4f + env)),
            )
        }
    }
}
