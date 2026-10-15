package com.autodj.app.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp

/**
 * The look of the instrument: a warm grey-beige "hardware" body, near-black round knobs and pads, one dark screen.
 * It is inspired by the granular instrument the project owner named as reference (structure and mood: quiet,
 * lower-case, almost monochrome); the palette, shapes and all drawing code are our own.
 */
object Palette {
    val body = Color(0xFFC4C0B1)        // main background
    val bodyRaised = Color(0xFFCFCBBD)  // cards, bars
    val bodySunken = Color(0xFFB9B5A5)  // selected segment, chips
    val ink = Color(0xFF22211D)         // knobs, text on body
    val inkSoft = Color(0xFF3B3934)
    val muted = Color(0xFF6D6960)       // labels and values under the knobs
    val outline = Color(0xFF8F8B7E)     // hairlines of the segmented controls and cards
    val pad = Color(0xFF262521)
    val padActive = Color(0xFF7C7875)
    val padText = Color(0xFFA9A59B)
    val screen = Color(0xFF0B0B0D)
    val screenGrid = Color(0xFF1D1D21)
    val screenText = Color(0xFF8F8F96)
    val wave = Color(0xFFD9D9DE)
    val copper = Color(0xFF8A6046)
    val good = Color(0xFF2C7A78)
    val warn = Color(0xFFA9701A)
    val bad = Color(0xFFA8412F)
}

/** One accent per voice, chosen to be readable on the dark screen. */
val VoiceColors = listOf(Color(0xFFE2B49C), Color(0xFFBCC6AB), Color(0xFF9DB4C8), Color(0xFFD9A3B5))

private val Scheme = lightColorScheme(
    primary = Palette.ink,
    onPrimary = Palette.body,
    secondary = Palette.copper,
    onSecondary = Palette.body,
    tertiary = Palette.good,
    background = Palette.body,
    onBackground = Palette.ink,
    surface = Palette.bodyRaised,
    onSurface = Palette.ink,
    surfaceVariant = Palette.bodySunken,
    onSurfaceVariant = Palette.muted,
    outline = Palette.outline,
    error = Palette.bad,
    onError = Palette.body,
)

private val Sans = FontFamily.SansSerif
private val AppTypography = Typography().let { t ->
    fun TextStyle.sans(size: Int, weight: FontWeight = FontWeight.Normal) =
        copy(fontFamily = Sans, fontSize = size.sp, fontWeight = weight, lineHeight = (size + 5).sp)
    t.copy(
        titleSmall = t.titleSmall.sans(13, FontWeight.Medium),
        bodyMedium = t.bodyMedium.sans(14),
        bodySmall = t.bodySmall.sans(12),
        labelMedium = t.labelMedium.sans(12),
        labelSmall = t.labelSmall.sans(11),
    )
}

@Composable
fun AutoDjTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = Scheme, typography = AppTypography, content = content)
}
