package com.autodj.app.dj

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

/** The dark "DJ" look of the library, decks and mix screens. The beige instrument screen keeps its own palette. */
object DjColors {
    val bg = Color(0xFF0E0F12)
    val panel = Color(0xFF17191E)
    val panelRaised = Color(0xFF1F2229)
    val line = Color(0xFF2B2F38)
    val text = Color(0xFFE7E8EC)
    val muted = Color(0xFF8B909B)
    val deckA = Color(0xFF4FC3F7)
    val deckB = Color(0xFFF06292)
    val good = Color(0xFF58C27D)
    val warn = Color(0xFFE0A440)
    val bad = Color(0xFFE5604D)
    val wave = Color(0xFFB7BCC8)
}

private val DjScheme = darkColorScheme(
    primary = DjColors.deckA,
    onPrimary = Color.Black,
    secondary = DjColors.deckB,
    onSecondary = Color.Black,
    background = DjColors.bg,
    onBackground = DjColors.text,
    surface = DjColors.panel,
    onSurface = DjColors.text,
    surfaceVariant = DjColors.panelRaised,
    onSurfaceVariant = DjColors.muted,
    outline = DjColors.line,
    error = DjColors.bad,
)

@Composable
fun DjTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = DjScheme, content = content)
}

/** Camelot wheel colour: the number picks the hue, B (major) is lighter than A (minor). */
fun camelotColor(camelot: String): Color {
    if (camelot.length < 2) return DjColors.muted
    val n = camelot.dropLast(1).toIntOrNull() ?: return DjColors.muted
    val major = camelot.last() == 'B'
    return hsv(((n - 1) * 30f + 200f) % 360f, if (major) 0.45f else 0.65f, if (major) 0.95f else 0.80f)
}

/** HSV (h in degrees) to a Compose colour without pulling in platform colour APIs. */
fun hsv(h: Float, s: Float, v: Float): Color {
    val c = v * s
    val x = c * (1f - kotlin.math.abs((h / 60f) % 2f - 1f))
    val m = v - c
    val (r, g, b) = when {
        h < 60f -> Triple(c, x, 0f)
        h < 120f -> Triple(x, c, 0f)
        h < 180f -> Triple(0f, c, x)
        h < 240f -> Triple(0f, x, c)
        h < 300f -> Triple(x, 0f, c)
        else -> Triple(c, 0f, x)
    }
    return Color(r + m, g + m, b + m)
}
