package com.autodj.app.design

import androidx.compose.animation.core.CubicBezierEasing
import androidx.compose.animation.core.spring
import androidx.compose.foundation.IndicationNodeFactory
import androidx.compose.foundation.LocalIndication
import androidx.compose.foundation.interaction.InteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.background
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.lerp
import androidx.compose.ui.node.DelegatableNode
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

// The design language of the app (docs/design/DESIGN.md): a board of flat tiles in bone, chocolate and green, one typeface,
// one radius, no borders or shadows. Components only know the roles of AutoDjColors, never hex values.

/** All colours as roles. */
data class AutoDjColors(
    val ground: Color,        // window
    val tile: Color,          // every tile
    val tile2: Color,         // track of faders, segment background, resting button
    val tile3: Color,         // hover, bar bands, switched-off beat dots
    val ink: Color,           // text, knobs, "selected"
    val ink2: Color,          // second text (>= 4.5:1 on tile)
    val onInk: Color,         // text on ink
    val screen: Color,        // dark displays: waveform, sequencer, timeline, granular cloud
    val screen2: Color,       // bar band on a display
    val screen3: Color,       // beat-1 column / playhead column
    val screenInk: Color,
    val screenDim: Color,
    val accent: Color,        // action and "running"
    val accentLit: Color,     // accent on a display
    val onAccent: Color,
    val deckALit: Color, val deckAField: Color,
    val deckBLit: Color, val deckBField: Color,
    val padKick: Color, val padSnare: Color, val padHat: Color, val padPerc: Color, val padFx: Color,
    val padKickLit: Color, val padSnareLit: Color, val padHatLit: Color, val padPercLit: Color, val padFxLit: Color,
    val warn: Color, val bad: Color,
)

val TagColors = AutoDjColors(
    ground = Color(0xFFE5E1D8), tile = Color(0xFFDAD5CA), tile2 = Color(0xFFCFC9BC), tile3 = Color(0xFFC3BDAF),
    ink = Color(0xFF2A1D17), ink2 = Color(0xFF5C4D44), onInk = Color(0xFFF2EEE6),
    screen = Color(0xFF1A1310), screen2 = Color(0xFF251B16), screen3 = Color(0xFF34271F),
    screenInk = Color(0xFFEDE7DB), screenDim = Color(0xFF9A8D82),
    accent = Color(0xFF456F3A), accentLit = Color(0xFF9CCB86), onAccent = Color(0xFFFFFFFF),
    deckALit = Color(0xFFE9DFC8), deckAField = Color(0xFF3A2A22), deckBLit = Color(0xFF9DBEE0), deckBField = Color(0xFF3F5F7F),
    padKick = Color(0xFF3A2A22), padSnare = Color(0xFF456F3A), padHat = Color(0xFF3F5F7F), padPerc = Color(0xFF766652), padFx = Color(0xFFC9B58A),
    padKickLit = Color(0xFFE9DFC8), padSnareLit = Color(0xFF9CCB86), padHatLit = Color(0xFF9DBEE0), padPercLit = Color(0xFFC7B79F), padFxLit = Color(0xFFE4CF9C),
    warn = Color(0xFFE0B25A), bad = Color(0xFFB5483A),
)

val NachtColors = TagColors.copy(
    ground = Color(0xFF141010), tile = Color(0xFF1F1916), tile2 = Color(0xFF2B231E), tile3 = Color(0xFF392E27),
    ink = Color(0xFFEDE7DB), ink2 = Color(0xFFA09386), onInk = Color(0xFF1A1310),
    screen = Color(0xFF0B0807), screen2 = Color(0xFF17110E), screen3 = Color(0xFF261D18), screenDim = Color(0xFF8C8076),
    accent = Color(0xFF8FBF7A), onAccent = Color(0xFF14100E),
)

/** The app-wide design state: day or night, reduced motion. Plain snapshot state, so composables and draw blocks both follow it. */
object Ds {
    var night by mutableStateOf(false)
    var reduceMotion by mutableStateOf(false)
    val c: AutoDjColors get() = if (night) NachtColors else TagColors
}

/** The music's position in its bar. One state, written by the host once per status poll; read by the few things that breathe in time. */
object Beat {
    var phase by mutableFloatStateOf(0f)
    var index by mutableIntStateOf(0)
    var playing by mutableStateOf(false)
    var fromMusic by mutableStateOf(false)

    fun update(phase: Float, index: Int, playing: Boolean, fromMusic: Boolean) {
        this.phase = phase
        this.index = index
        this.playing = playing
        this.fromMusic = fromMusic
    }

    /** 0..1: strong on the downbeat, falling away within each beat. Read it only inside draw blocks. */
    val pulse: Float get() = pulse(phase, index, playing, Ds.reduceMotion)
}

/** Colour of a camelot key chip (data colour only). Minor (A) is richer and darker than major (B); text on it is always #2A1D17. */
fun camelotChip(camelot: String): Color {
    val n = camelot.dropLast(1).toIntOrNull() ?: return Color.Gray
    val major = camelot.last() == 'B'
    return hslToColor(((n - 1) * 30f + 200f) % 360f, if (major) .34f else .40f, if (major) .80f else .72f)
}

private fun hslToColor(h: Float, s: Float, l: Float): Color {
    val c = (1f - kotlin.math.abs(2f * l - 1f)) * s
    val x = c * (1f - kotlin.math.abs((h / 60f) % 2f - 1f))
    val m = l - c / 2f
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

/** Hanken Grotesk (OFL), Regular and Medium only; the platforms install it at start-up. Numbers are always tabular. */
object AutoDjType {
    var family: FontFamily = FontFamily.SansSerif

    private fun style(size: Int, lh: Float, track: Float, w: FontWeight) =
        TextStyle(fontFamily = family, fontSize = size.sp, lineHeight = (size * lh).sp, letterSpacing = (size * track).sp, fontWeight = w, fontFeatureSettings = "tnum")

    // five sizes, nothing else
    val display get() = style(64, .9f, -.04f, FontWeight.Medium)
    val displayPhone get() = style(52, .9f, -.04f, FontWeight.Medium)
    val head get() = style(36, 1f, -.035f, FontWeight.Medium)
    val headPhone get() = style(30, 1f, -.035f, FontWeight.Medium)
    val lead get() = style(22, 1.15f, -.02f, FontWeight.Medium)
    val body get() = style(15, 1.45f, -.005f, FontWeight.Normal)
    val bodyMedium get() = style(15, 1.45f, -.005f, FontWeight.Medium)
    val row get() = style(17, 1.3f, -.005f, FontWeight.Medium)
    val small get() = style(13, 1.4f, 0f, FontWeight.Normal)
}

object AutoDjSpace {
    val margin = 12.dp
    val gap = 8.dp
    val tilePad = 16.dp
    val rowGap = 4.dp
    val control = 44.dp
    val controlSm = 36.dp
    val bar = 56.dp
    val row = 60.dp
    val phoneMargin = 8.dp
    val phoneGap = 6.dp
    val phonePad = 12.dp
}

/** The one radius. Knobs and dots are circles. */
val AutoDjShape = RoundedCornerShape(6.dp)

object AutoDjMotion {
    val out = CubicBezierEasing(.2f, 0f, 0f, 1f)
    const val instant = 90
    const val fast = 160
    const val base = 260
    const val enter = 320
    const val stagger = 45
    const val rise = 16
    val pressSpring = spring<Float>(dampingRatio = .7f, stiffness = 800f)
    const val pressScale = .97f

    /** Duration with reduced motion applied: tone changes become instant. */
    fun ms(d: Int): Int = if (Ds.reduceMotion) 0 else d
}

/** pulse(): (1 - phase)^3, full on the downbeat, 55 % on the other beats; 0 when nothing plays or motion is reduced. */
fun pulse(beatPhase: Float, beatIndex: Int, playing: Boolean, reduceMotion: Boolean): Float {
    if (!playing || reduceMotion) return 0f
    val d = 1f - beatPhase.coerceIn(0f, 1f)
    return d * d * d * if (beatIndex == 0) 1f else .55f
}

/** Tile colour under pulse: up to 6 % towards ink. */
fun pulseTile(base: Color, ink: Color, p: Float): Color = lerp(base, ink, p * .06f)

/** Text and second-text colour of the tile the content sits on. */
val LocalInk = compositionLocalOf<Color?> { null }
val LocalInk2 = compositionLocalOf<Color?> { null }

/** No ripples: pressing is a small scale and a tone change, done by the components themselves. */
private object NoIndication : IndicationNodeFactory {
    override fun create(interactionSource: InteractionSource): DelegatableNode = object : Modifier.Node() {}
    override fun hashCode(): Int = -1
    override fun equals(other: Any?): Boolean = other === this
}

/** The one theme of the app: ground, ink, one typeface, no ripples. */
@Composable
fun AutoDjDesignTheme(content: @Composable () -> Unit) {
    val c = Ds.c
    val scheme = if (Ds.night) {
        darkColorScheme(
            primary = c.accent, onPrimary = c.onAccent, secondary = c.ink, onSecondary = c.onInk, background = c.ground, onBackground = c.ink,
            surface = c.tile, onSurface = c.ink, surfaceVariant = c.tile2, onSurfaceVariant = c.ink2, outline = c.tile3, error = c.bad, onError = c.onAccent,
        )
    } else {
        lightColorScheme(
            primary = c.accent, onPrimary = c.onAccent, secondary = c.ink, onSecondary = c.onInk, background = c.ground, onBackground = c.ink,
            surface = c.tile, onSurface = c.ink, surfaceVariant = c.tile2, onSurfaceVariant = c.ink2, outline = c.tile3, error = c.bad, onError = c.onAccent,
        )
    }
    val type = Typography(
        bodyLarge = AutoDjType.body, bodyMedium = AutoDjType.body, bodySmall = AutoDjType.small,
        titleLarge = AutoDjType.lead, titleMedium = AutoDjType.bodyMedium, titleSmall = AutoDjType.bodyMedium,
        labelLarge = AutoDjType.bodyMedium, labelMedium = AutoDjType.small, labelSmall = AutoDjType.small,
        headlineMedium = AutoDjType.head, headlineSmall = AutoDjType.lead, displayMedium = AutoDjType.display,
    )
    MaterialTheme(colorScheme = scheme, typography = type, shapes = Shapes(AutoDjShape, AutoDjShape, AutoDjShape, AutoDjShape, AutoDjShape)) {
        CompositionLocalProvider(LocalIndication provides NoIndication, LocalContentColor provides c.ink) {
            Box(Modifier.fillMaxSize().background(c.ground)) { content() }
        }
    }
}
