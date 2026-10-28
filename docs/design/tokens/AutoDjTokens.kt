// Referenz-Tokens der Auto-DJ-Designsprache (Compose, gemeinsam für Android und Desktop nutzbar).
// Nur Vorlage zum Übernehmen: nicht Teil des Gradle-Builds. Werte 1:1 aus DESIGN.md §3 und mockups/prototype.html.
// Ziel im Code: DjColors/Palette (dj/DjTheme.kt, ui/Theme.kt) durch diese Datei ersetzen, ein Theme für alle Tabs.
package com.autodj.app.design

import androidx.compose.animation.core.CubicBezierEasing
import androidx.compose.animation.core.spring
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.lerp
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.foundation.shape.RoundedCornerShape

/** Alle Farben als Rollen. Komponenten kennen nur Rollen, nie Hex-Werte. */
data class AutoDjColors(
    val ground: Color,        // Fenster / Bildschirmhintergrund
    val tile: Color,          // Standardfläche
    val tile2: Color,         // Spur, gedrückt, Segment-Hintergrund
    val tile3: Color,         // Hover, Taktbänder, ausgeschaltete Beat-Punkte
    val ink: Color,           // Text, Drehknöpfe, "ausgewählt"-Füllung
    val ink2: Color,          // Zweittext (Kontrast >= 4.5 auf tile)
    val onInk: Color,         // Text auf ink
    val screen: Color,        // dunkle "Displays": Wellenform, Sequencer, Zeitleiste, Granular
    val screen2: Color,       // Taktband auf dem Display
    val screen3: Color,       // Beat-1-Spalte / Playhead-Spalte
    val screenInk: Color,
    val screenDim: Color,
    val accent: Color,        // Aktion und "läuft": Play, An, Hauptbutton
    val accentLit: Color,     // Akzent auf dunklem Display
    val onAccent: Color,
    val deckALit: Color, val deckAField: Color,
    val deckBLit: Color, val deckBField: Color,
    val padKick: Color, val padSnare: Color, val padHat: Color, val padPerc: Color, val padFx: Color,        // Pad-Flächen
    val padKickLit: Color, val padSnareLit: Color, val padHatLit: Color, val padPercLit: Color, val padFxLit: Color, // Schritte auf dem Display
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

val LocalAutoDjColors = staticCompositionLocalOf { TagColors }

/** Camelot-Tonart als Datenfarbe (nur Chip und Tonarten-Tile). Moll (A) satter und dunkler als Dur (B). */
fun camelotChip(camelot: String): Color {
    val n = camelot.dropLast(1).toIntOrNull() ?: return Color.Gray
    val major = camelot.last() == 'B'
    return hslToColor(((n - 1) * 30f + 200f) % 360f, if (major) .34f else .40f, if (major) .80f else .72f)   // Text darauf: immer #2A1D17
}
private fun hslToColor(h: Float, s: Float, l: Float): Color {
    val c = (1f - kotlin.math.abs(2f * l - 1f)) * s
    val x = c * (1f - kotlin.math.abs((h / 60f) % 2f - 1f))
    val m = l - c / 2f
    val (r, g, b) = when {
        h < 60f -> Triple(c, x, 0f); h < 120f -> Triple(x, c, 0f); h < 180f -> Triple(0f, c, x)
        h < 240f -> Triple(0f, x, c); h < 300f -> Triple(x, 0f, c); else -> Triple(c, 0f, x)
    }
    return Color(r + m, g + m, b + m)
}

/** Schrift: Hanken Grotesk (OFL), nur Regular und Medium. Dateien: fonts/. Zahlen immer mit "tnum". */
// Desktop: FontFamily(Font(resource = "fonts/HankenGrotesk-Regular.ttf", FontWeight.Normal), Font(resource = "fonts/HankenGrotesk-Medium.ttf", FontWeight.Medium))
// Android: res/font/hankengrotesk_regular.ttf, hankengrotesk_medium.ttf -> Font(R.font.hankengrotesk_regular, FontWeight.Normal) ...
object AutoDjType {
    lateinit var family: FontFamily
    private fun style(size: Int, lh: Float, track: Float, w: FontWeight) =
        TextStyle(fontFamily = family, fontSize = size.sp, lineHeight = (size * lh).sp, letterSpacing = (size * track).sp, fontWeight = w, fontFeatureSettings = "tnum")
    // Fünf Stufen, mehr gibt es nicht. Desktop-Werte; Telefon in Klammern.
    val display get() = style(64, .9f, -.04f, FontWeight.Medium)   // BPM an den Decks (Telefon 52)
    val head get() = style(36, 1f, -.035f, FontWeight.Medium)      // Kopfzahl im Transport, Kachel-Überschrift groß (Telefon 30)
    val lead get() = style(22, 1.15f, -.02f, FontWeight.Medium)    // Kachel-Titel, Songname am Deck
    val body get() = style(15, 1.45f, -.005f, FontWeight.Normal)   // Buttons, Listen. Zeilen 17sp Medium für Songnamen
    val small get() = style(13, 1.4f, 0f, FontWeight.Normal)       // Zweittext, Werte unter Knöpfen. Nie kleiner als 13.
}

object AutoDjSpace {
    val margin = 12.dp; val gap = 8.dp; val tilePad = 16.dp; val rowGap = 4.dp
    val control = 44.dp; val controlSm = 36.dp; val bar = 56.dp; val row = 60.dp; val touchMin = 44.dp
    val phoneMargin = 8.dp; val phoneGap = 6.dp
}
val AutoDjShape = RoundedCornerShape(6.dp)   // der einzige Radius; Knöpfe und Punkte sind Kreise
// Keine Schatten, keine Verläufe, keine Linien. Fläche trennt sich durch Tonstufe.

object AutoDjMotion {
    val out = CubicBezierEasing(.2f, 0f, 0f, 1f)          // Eintritt, Slider, alles Sichtbare
    const val instant = 90      // Druck
    const val fast = 160        // Tonwechsel, Toggle, Knopf
    const val base = 260        // Tab-Indikator, aufklappende Zeile
    const val enter = 320       // Einblenden der Kacheln
    const val stagger = 45      // je Kachel, höchstens 8 Stufen
    const val rise = 16         // dp, Weg beim Einblenden
    val pressSpring = spring<Float>(dampingRatio = .7f, stiffness = 800f)
    const val pressScale = .97f
}

/**
 * Der Puls: eine einzige Takt-Phase treibt alles, was im Takt atmet.
 * beatPhase = Position im aktuellen Schlag (0..1), beatIndex = 0..3 im Takt. Beides kommt aus dem Engine-Status.
 * Steht nichts auf Play oder ist "Bewegung reduzieren" an: 0.
 */
fun pulse(beatPhase: Float, beatIndex: Int, playing: Boolean, reduceMotion: Boolean): Float {
    if (!playing || reduceMotion) return 0f
    val d = 1f - beatPhase
    return d * d * d * if (beatIndex == 0) 1f else .55f
}
/** Kachelfarbe unter Puls: bis zu 6 % Richtung ink. Nur auf Kacheln mit Rolle "pulse" (Transport-BPM, Deck-Kopf, Beat-Punkte). */
fun pulseTile(base: Color, ink: Color, p: Float): Color = lerp(base, ink, p * .06f)

@Composable
fun AutoDjTheme(night: Boolean, content: @Composable () -> Unit) =
    CompositionLocalProvider(LocalAutoDjColors provides if (night) NachtColors else TagColors, content = content)
