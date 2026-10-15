// Referenz-Tokens der Auto-DJ-Designsprache "Kabine" (v2). Compose, gemeinsam für Android und Desktop.
// Nur Vorlage zum Übernehmen: nicht Teil des Gradle-Builds. Werte 1:1 aus DESIGN.md §3 und mockups/prototype.html.
// Rollennamen sind identisch zu v1 (Ds.c.tile, Ds.c.ink ...): der Umbau ändert vor allem Werte und Formen.
// Neu: tileSunk, screenBand, deckA/deckB/signal, good, Archivo statt Hanken, Kerbe statt Radius 6, KEINE Tag/Nacht-Variante.
package com.autodj.app.design

import androidx.compose.animation.core.CubicBezierEasing
import androidx.compose.animation.core.spring
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.foundation.shape.CutCornerShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.lerp
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/** Alle Farben als Rollen. Komponenten kennen nur Rollen, nie Hex-Werte. */
data class AutoDjColors(
    val ground: Color,        // Fenster
    val tile: Color,          // Kachel
    val tileSunk: Color,      // NEU: vertiefte Fläche (Scheiben-Bühne, Segment-Hintergrund)
    val tile2: Color,         // Button ruhend, Fader-Spur, Zellen
    val tile3: Color,         // Hover, aufgeklappte Zeile, Wertbogen-Spur
    val ink: Color,           // Text, "gewählt", globale Aktion (Play, Hauptbutton)
    val ink2: Color,          // Zweittext (6.5:1 auf tile)
    val onInk: Color,         // Text auf ink, Orange und Pads
    val screen: Color,        // Displays: Wellenformen, Sequencer, Zeitleiste, Granular
    val screen2: Color,       // Sequencer-Zellen
    val screen3: Color,       // Beat-Spalten, Playhead-Spalte
    val screenBand: Color,    // NEU: Taktbänder in Wellenformen
    val screenInk: Color,
    val screenDim: Color,
    val accent: Color,        // Alias von ink, bis alle Stellen auf ink umgestellt sind
    val accentLit: Color,
    val onAccent: Color,
    val deckA: Color,         // NEU: Deck A (Knochen): Buchstabe, Ring, Bogen, Regler
    val deckB: Color,         // NEU: Deck B (Signalorange)
    val signal: Color,        // NEU: "läuft/laut" (= deckB)
    val deckALit: Color, val deckAField: Color,   // v1-Namen, bleiben bis zur Umstellung
    val deckBLit: Color, val deckBField: Color,
    val padKick: Color, val padSnare: Color, val padHat: Color, val padPerc: Color, val padFx: Color,        // Pad-Flächen (= Licht)
    val padKickLit: Color, val padSnareLit: Color, val padHatLit: Color, val padPercLit: Color, val padFxLit: Color,
    val good: Color, val warn: Color, val bad: Color, // nur kleine Quadrate, Tags, Kill
)

val KabineColors = AutoDjColors(
    ground = Color(0xFF0A0A0C), tile = Color(0xFF131316), tileSunk = Color(0xFF1B1B20),
    tile2 = Color(0xFF26262C), tile3 = Color(0xFF34343C),
    ink = Color(0xFFF1EEE6), ink2 = Color(0xFF9B9BA5), onInk = Color(0xFF0A0A0C),
    screen = Color(0xFF050507), screen2 = Color(0xFF1B1B20), screen3 = Color(0xFF26262C), screenBand = Color(0xFF0E0E12),
    screenInk = Color(0xFFF1EEE6), screenDim = Color(0xFF9B9BA5),
    accent = Color(0xFFF1EEE6), accentLit = Color(0xFFF1EEE6), onAccent = Color(0xFF0A0A0C),
    deckA = Color(0xFFF1EEE6), deckB = Color(0xFFFF5A1F), signal = Color(0xFFFF5A1F),
    deckALit = Color(0xFFF1EEE6), deckAField = Color(0xFF26262C), deckBLit = Color(0xFFFF5A1F), deckBField = Color(0xFF26262C),
    padKick = Color(0xFFFF5A1F), padSnare = Color(0xFFF1EEE6), padHat = Color(0xFF8A97A6), padPerc = Color(0xFFA8C23A), padFx = Color(0xFFFFC857),
    // Gedrückt: Helligkeit +18 %, Skala 0.965 (in der Pad-Komponente), die Lit-Werte gelten für Schritte auf dem Display
    padKickLit = Color(0xFFFF5A1F), padSnareLit = Color(0xFFF1EEE6), padHatLit = Color(0xFF8A97A6), padPercLit = Color(0xFFA8C23A), padFxLit = Color(0xFFFFC857),
    good = Color(0xFF8FE388), warn = Color(0xFFFFC857), bad = Color(0xFFE5484D),
)

val LocalAutoDjColors = staticCompositionLocalOf { KabineColors }

/** Kontextfarbe eines Decks (Regler, Bogen, Ring, gewählte Segmente). Außerhalb eines Decks: ink. */
val LocalDeckColor = staticCompositionLocalOf { KabineColors.ink }

/** Camelot-Tonart als Datenfarbe (Chip, Tonartenrad). hsl((n-1)*30+200, Dur 55/Moll 70 %, Dur 74/Moll 64 %). Text darauf immer onInk. */
fun camelotChip(camelot: String): Color {
    val n = camelot.dropLast(1).toIntOrNull() ?: return Color.Gray
    val major = camelot.last() == 'B'
    return hslToColor(((n - 1) * 30f + 200f) % 360f, if (major) .55f else .70f, if (major) .74f else .64f)
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

/**
 * Schrift: Archivo (SIL OFL). Dateien: fonts/Archivo-Regular.ttf (400), Archivo-Medium.ttf (500), Archivo-Expanded-Bold.ttf (700, breit).
 * Zahlen immer "tnum". Keine Monospace, keine Großbuchstaben-Etiketten.
 * Desktop:  Font(resource = "fonts/Archivo-Regular.ttf", FontWeight.Normal) usw.
 * Android:  res/font/archivo_regular.ttf, archivo_medium.ttf, archivo_expanded_bold.ttf -> Font(R.font.archivo_regular, FontWeight.Normal) ...
 */
object AutoDjType {
    lateinit var text: FontFamily      // Regular + Medium
    lateinit var wide: FontFamily      // Expanded Bold (eine Datei, Gewicht Bold)
    private fun text(size: Int, lh: Float, track: Float, w: FontWeight) =
        TextStyle(fontFamily = text, fontSize = size.sp, lineHeight = (size * lh).sp, letterSpacing = (size * track).sp, fontWeight = w, fontFeatureSettings = "tnum")
    private fun wide(size: Int, lh: Float, track: Float) =
        TextStyle(fontFamily = wide, fontSize = size.sp, lineHeight = (size * lh).sp, letterSpacing = (size * track).sp, fontWeight = FontWeight.Bold, fontFeatureSettings = "tnum")
    // Fünf Stufen (Desktop-Werte; Telefon: display 44, head 24).
    val display get() = wide(56, .95f, -.02f)                // NUR Zahlen: BPM an der Scheibe, Restzeit, große Kopfzahl
    val head get() = wide(28, 1f, -.015f)                    // Songname am Deck, Effekt-Titel, "Songs hinzufügen"
    val lead get() = text(18, 1.2f, 0f, FontWeight.Medium)   // Untertitel, Listennamen, Segment-Titel
    val body get() = text(15, 1.4f, 0f, FontWeight.Normal)   // Buttons, Werte (Medium für Songnamen in Zeilen)
    val small get() = text(13, 1.4f, 0f, FontWeight.Normal)  // Zweittext. Nie kleiner als 13.
    val bpmRow get() = wide(20, 1f, 0f)                      // BPM in Listen und Mix-Zeilen
    val brand get() = wide(17, 1f, 0f)                       // "AUTO·DJ" mit orangem Punkt
}

object AutoDjSpace {
    val margin = 12.dp; val gap = 8.dp; val tilePad = 16.dp; val rowGap = 4.dp
    val control = 44.dp; val controlSm = 34.dp; val bar = 56.dp; val row = 64.dp; val touchMin = 44.dp
    val phoneMargin = 8.dp; val phoneGap = 6.dp; val phoneTilePad = 12.dp
}

/** Kachel: Kerbe oben rechts (Telefon 10 dp). Flache Kacheln (Displays, Sequencer) und alles Bedienbare: 3 dp. Knöpfe sind Kreise. */
val AutoDjTileShape = CutCornerShape(topEnd = 14.dp)
val AutoDjTileShapePhone = CutCornerShape(topEnd = 10.dp)
val AutoDjShape = RoundedCornerShape(3.dp)
val AutoDjChipShape = RoundedCornerShape(2.dp)
// Keine Schatten, keine Verläufe, keine Linien, kein Glas, kein Glühen. Fläche trennt sich durch Tonstufe (tile -> tileSunk -> tile2 -> tile3).

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
    const val padPressScale = .965f
}

/**
 * Der Puls: eine einzige Takt-Phase treibt alles, was im Takt atmet (aus Beat/DjStatus, nie aus einer eigenen Uhr).
 * beatPhase = Position im aktuellen Schlag (0..1), beatIndex = 0..3. Steht nichts auf Play oder "Bewegung reduzieren": 0.
 */
fun pulse(beatPhase: Float, beatIndex: Int, playing: Boolean, reduceMotion: Boolean): Float {
    if (!playing || reduceMotion) return 0f
    val d = 1f - beatPhase
    return d * d * d * if (beatIndex == 0) 1f else .55f
}
/** Kachelfarbe unter Puls: bis zu 6 % Richtung ink (Nabe der Scheibe: 8 %). Nur Kacheln mit Rolle "pulse". */
fun pulseTile(base: Color, ink: Color, p: Float, strength: Float = .06f): Color = lerp(base, ink, p * strength)

/**
 * Taktscheibe: Winkel der Drehung in Grad. Genau eine Umdrehung pro Takt, vom Engine-Status getrieben.
 * Bei "Bewegung reduzieren" oder Pause: der Aufrufer zeichnet den Ring ohne Drehung (Winkel 0).
 */
fun platterAngle(beatPhase: Float, beatIndex: Int): Float = 360f * (beatIndex + beatPhase) / 4f

@Composable
fun AutoDjTheme(content: @Composable () -> Unit) =
    CompositionLocalProvider(LocalAutoDjColors provides KabineColors, content = content)
