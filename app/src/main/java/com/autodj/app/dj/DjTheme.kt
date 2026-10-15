package com.autodj.app.dj

import com.autodj.app.design.AutoDjDesignTheme
import com.autodj.app.design.Ds
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

/** Old colour names of the DJ screens, mapped onto the roles of the design language (see design/Design.kt). New code uses `Ds.c`. */
object DjColors {
    val bg: Color get() = Ds.c.ground
    val panel: Color get() = Ds.c.tile
    val panelRaised: Color get() = Ds.c.tile2
    val line: Color get() = Ds.c.tile3
    val text: Color get() = Ds.c.ink
    val muted: Color get() = Ds.c.ink2
    val deckA: Color get() = Ds.c.deckAField
    val deckB: Color get() = Ds.c.deckBField
    val good: Color get() = Ds.c.accent
    val warn: Color get() = Ds.c.warn
    val bad: Color get() = Ds.c.bad
    val wave: Color get() = Ds.c.screenInk
}

@Composable
fun DjTheme(content: @Composable () -> Unit) = AutoDjDesignTheme(content)

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
