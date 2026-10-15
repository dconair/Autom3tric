// Referenz-Tokens der Auto-DJ-Designsprache "Kabine" (v2). Compose, gemeinsam für Android und Desktop.
// Nur Vorlage zum Übernehmen: nicht Teil des Gradle-Builds. Werte 1:1 aus DESIGN.md §3 und mockups/prototype.html.
// Rollennamen sind identisch zu v1 (Ds.c.tile, Ds.c.ink ...): der Umbau ändert vor allem Werte und Formen.
// Neu: tileSunk, screenBand, deckA/deckB/signal, good, Archivo statt Hanken, Kerbe statt Radius 6.
// v2.1: sechs Farbschemata (Schemes), drei Looks (AutoDjLook), Wellen-Stil, Deck-Ansicht, Modul-Raster. Siehe DESIGN.md §10.
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
    val wave1: Color = Color(0xFFFF5A1F), val wave2: Color = Color(0xFFF1EEE6), val wave3: Color = Color(0xFF8A97A6), // Wellen-Stil "3 Bänder": Bass, Mitten, Höhen
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

// Weitere Schemata (§10.4). Gleiche Rollen, andere Werte. Displays (screen*) bleiben dunkel.
val EisColors = KabineColors.copy(
    ground = Color(0xFF05080D), tile = Color(0xFF0C131C), tileSunk = Color(0xFF131E2B), tile2 = Color(0xFF1D2C3F), tile3 = Color(0xFF2B4059),
    ink = Color(0xFFE8F2FF), ink2 = Color(0xFF8FA6BF), onInk = Color(0xFF04080D),
    screen = Color(0xFF02050A), screen2 = Color(0xFF131E2B), screen3 = Color(0xFF1D2C3F), screenBand = Color(0xFF08101A), screenInk = Color(0xFFE8F2FF), screenDim = Color(0xFF8FA6BF),
    accent = Color(0xFFE8F2FF), accentLit = Color(0xFFE8F2FF), onAccent = Color(0xFF04080D),
    deckA = Color(0xFF4DB8FF), deckB = Color(0xFF5CF2C0), signal = Color(0xFF5CF2C0),
    deckALit = Color(0xFF4DB8FF), deckBLit = Color(0xFF5CF2C0), deckAField = Color(0xFF1D2C3F), deckBField = Color(0xFF1D2C3F),
    padKick = Color(0xFF4DB8FF), padSnare = Color(0xFFE8F2FF), padHat = Color(0xFF7C93AD), padPerc = Color(0xFF5CF2C0), padFx = Color(0xFFFFD166),
    padKickLit = Color(0xFF4DB8FF), padSnareLit = Color(0xFFE8F2FF), padHatLit = Color(0xFF7C93AD), padPercLit = Color(0xFF5CF2C0), padFxLit = Color(0xFFFFD166),
    good = Color(0xFF5CF2C0), warn = Color(0xFFFFD166), bad = Color(0xFFFF5C6C),
    wave1 = Color(0xFF1E6FD9), wave2 = Color(0xFF4DB8FF), wave3 = Color(0xFFE8F2FF),
)
val AmberColors = KabineColors.copy(
    ground = Color(0xFF0B0804), tile = Color(0xFF15100A), tileSunk = Color(0xFF1E170E), tile2 = Color(0xFF2C2214), tile3 = Color(0xFF40321D),
    ink = Color(0xFFFFE9C7), ink2 = Color(0xFFA8957A), onInk = Color(0xFF0B0804),
    screen = Color(0xFF050301), screen2 = Color(0xFF1E170E), screen3 = Color(0xFF2C2214), screenBand = Color(0xFF0D0905), screenInk = Color(0xFFFFE9C7), screenDim = Color(0xFFA8957A),
    accent = Color(0xFFFFE9C7), accentLit = Color(0xFFFFE9C7), onAccent = Color(0xFF0B0804),
    deckA = Color(0xFFFFB000), deckB = Color(0xFFFF6A3D), signal = Color(0xFFFF6A3D),
    deckALit = Color(0xFFFFB000), deckBLit = Color(0xFFFF6A3D), deckAField = Color(0xFF2C2214), deckBField = Color(0xFF2C2214),
    padKick = Color(0xFFFF6A3D), padSnare = Color(0xFFFFE9C7), padHat = Color(0xFFA8957A), padPerc = Color(0xFFFFB000), padFx = Color(0xFFE8D23A),
    padKickLit = Color(0xFFFF6A3D), padSnareLit = Color(0xFFFFE9C7), padHatLit = Color(0xFFA8957A), padPercLit = Color(0xFFFFB000), padFxLit = Color(0xFFE8D23A),
    good = Color(0xFFB7E36B), warn = Color(0xFFFFB000), bad = Color(0xFFFF4D3D),
    wave1 = Color(0xFFFF6A3D), wave2 = Color(0xFFFFB000), wave3 = Color(0xFFFFE9C7),
)
val NeonColors = KabineColors.copy(
    ground = Color(0xFF0B0912), tile = Color(0xFF14111F), tileSunk = Color(0xFF1D1830), tile2 = Color(0xFF2B2447), tile3 = Color(0xFF3E3566),
    ink = Color(0xFFF4F0FF), ink2 = Color(0xFFA59BC4), onInk = Color(0xFF0B0912),
    screen = Color(0xFF05040A), screen2 = Color(0xFF1D1830), screen3 = Color(0xFF2B2447), screenBand = Color(0xFF0D0A18), screenInk = Color(0xFFF4F0FF), screenDim = Color(0xFFA59BC4),
    accent = Color(0xFFF4F0FF), accentLit = Color(0xFFF4F0FF), onAccent = Color(0xFF0B0912),
    deckA = Color(0xFFC6FF3D), deckB = Color(0xFFFF3DAE), signal = Color(0xFFFF3DAE),
    deckALit = Color(0xFFC6FF3D), deckBLit = Color(0xFFFF3DAE), deckAField = Color(0xFF2B2447), deckBField = Color(0xFF2B2447),
    padKick = Color(0xFFFF3DAE), padSnare = Color(0xFFF4F0FF), padHat = Color(0xFF7CE3FF), padPerc = Color(0xFFC6FF3D), padFx = Color(0xFFFFD23D),
    padKickLit = Color(0xFFFF3DAE), padSnareLit = Color(0xFFF4F0FF), padHatLit = Color(0xFF7CE3FF), padPercLit = Color(0xFFC6FF3D), padFxLit = Color(0xFFFFD23D),
    good = Color(0xFF7CF59A), warn = Color(0xFFFFD23D), bad = Color(0xFFFF4D6D),
    wave1 = Color(0xFFFF3DAE), wave2 = Color(0xFFC6FF3D), wave3 = Color(0xFF7CE3FF),
)
val PapierColors = KabineColors.copy(   // hell; Displays bleiben dunkel
    ground = Color(0xFFE4E0D6), tile = Color(0xFFF3F0E9), tileSunk = Color(0xFFE5E0D4), tile2 = Color(0xFFD5CFC1), tile3 = Color(0xFFBFB8A8),
    ink = Color(0xFF1A1714), ink2 = Color(0xFF5E574E), onInk = Color(0xFFF6F3EC),
    screen = Color(0xFF14120F), screen2 = Color(0xFF26221D), screen3 = Color(0xFF383229), screenBand = Color(0xFF1C1915), screenInk = Color(0xFFF1EEE6), screenDim = Color(0xFFA39B8E),
    accent = Color(0xFF1A1714), accentLit = Color(0xFF1A1714), onAccent = Color(0xFFF6F3EC),
    deckA = Color(0xFF2F5FE0), deckB = Color(0xFFC0390E), signal = Color(0xFFC0390E),
    deckALit = Color(0xFF2F5FE0), deckBLit = Color(0xFFC0390E), deckAField = Color(0xFFD5CFC1), deckBField = Color(0xFFD5CFC1),
    padKick = Color(0xFFFF7A4D), padSnare = Color(0xFFFFFFFF), padHat = Color(0xFF9DB0C4), padPerc = Color(0xFF9BCB4E), padFx = Color(0xFFFFC84A),
    padKickLit = Color(0xFFFF7A4D), padSnareLit = Color(0xFFFFFFFF), padHatLit = Color(0xFF9DB0C4), padPercLit = Color(0xFF9BCB4E), padFxLit = Color(0xFFFFC84A),
    good = Color(0xFF2E9E4F), warn = Color(0xFFB97A00), bad = Color(0xFFD3322F),
    wave1 = Color(0xFFFF7A4D), wave2 = Color(0xFFF1EEE6), wave3 = Color(0xFF7DA2FF),
)
val MonoColors = KabineColors.copy(
    ground = Color(0xFF0C0C0C), tile = Color(0xFF161616), tileSunk = Color(0xFF1F1F1F), tile2 = Color(0xFF2B2B2B), tile3 = Color(0xFF3A3A3A),
    ink = Color(0xFFFFFFFF), ink2 = Color(0xFF9A9A9A), onInk = Color(0xFF0C0C0C),
    screen = Color(0xFF050505), screen2 = Color(0xFF1F1F1F), screen3 = Color(0xFF2B2B2B), screenBand = Color(0xFF0E0E0E), screenInk = Color(0xFFFFFFFF), screenDim = Color(0xFF9A9A9A),
    accent = Color(0xFFFFFFFF), accentLit = Color(0xFFFFFFFF), onAccent = Color(0xFF0C0C0C),
    deckA = Color(0xFFFFFFFF), deckB = Color(0xFFFFD400), signal = Color(0xFFFFD400),
    deckALit = Color(0xFFFFFFFF), deckBLit = Color(0xFFFFD400), deckAField = Color(0xFF2B2B2B), deckBField = Color(0xFF2B2B2B),
    padKick = Color(0xFFFFD400), padSnare = Color(0xFFFFFFFF), padHat = Color(0xFF8C8C8C), padPerc = Color(0xFFBFBFBF), padFx = Color(0xFFE0A800),
    padKickLit = Color(0xFFFFD400), padSnareLit = Color(0xFFFFFFFF), padHatLit = Color(0xFF8C8C8C), padPercLit = Color(0xFFBFBFBF), padFxLit = Color(0xFFE0A800),
    good = Color(0xFF9BE08A), warn = Color(0xFFFFD400), bad = Color(0xFFFF5348),
    wave1 = Color(0xFFFFD400), wave2 = Color(0xFFFFFFFF), wave3 = Color(0xFF8C8C8C),
)

enum class AutoDjScheme(val label: String, val colors: AutoDjColors) {
    Kabine("Kabine", KabineColors), Eis("Eis", EisColors), Amber("Amber", AmberColors),
    Neon("Neon", NeonColors), Papier("Papier", PapierColors), Mono("Mono", MonoColors),
}

/** Look: Form, Dichte, Schrift und Standard-Deckansicht (§10.3). Komponenten lesen daraus statt fester dp-Werte. */
data class AutoDjLook(
    val name: String,
    val cut: androidx.compose.ui.unit.Dp,           // Kerbe an Kacheln
    val radius: androidx.compose.ui.unit.Dp,        // Bedienelemente
    val tileRadius: androidx.compose.ui.unit.Dp,    // Kacheln
    val gap: androidx.compose.ui.unit.Dp, val margin: androidx.compose.ui.unit.Dp, val tilePad: androidx.compose.ui.unit.Dp,
    val control: androidx.compose.ui.unit.Dp,       // Höhe Button, Tab
    val displayBold: Boolean,                       // true: Archivo Expanded Bold für Zahlen/Überschriften, false: Archivo Medium
    val defaultView: DeckView,
)
enum class DeckView { Platter, Wave, Both }
enum class WaveStyle { Solid, Bands, Heat }

val KabineLook = AutoDjLook("Kabine", 14.dp, 3.dp, 0.dp, 8.dp, 12.dp, 16.dp, 44.dp, true, DeckView.Platter)
val PultLook = AutoDjLook("Pult", 0.dp, 0.dp, 0.dp, 3.dp, 6.dp, 12.dp, 40.dp, false, DeckView.Wave)
val BuehneLook = AutoDjLook("Bühne", 0.dp, 14.dp, 26.dp, 12.dp, 16.dp, 20.dp, 52.dp, false, DeckView.Platter)
val LocalAutoDjLook = staticCompositionLocalOf { KabineLook }

/** Modul-Raster (§10.6): 12 × 8 Felder. Zelle = [x, y, Breite, Höhe]. Gespeichert pro Seite, nie in Pixeln. */
const val GRID_COLS = 12
const val GRID_ROWS = 8
data class Cell(val x: Int, val y: Int, val w: Int, val h: Int) {
    fun overlaps(o: Cell) = x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h
}
/** Das bewegte Modul behält seinen Platz, getroffene rutschen nach unten. null = passt nicht ins Raster, Bewegung verwerfen. */
fun resolveGrid(layout: Map<String, Cell>, active: String): Map<String, Cell>? {
    val out = layout.toMutableMap(); val placed = mutableListOf(out.getValue(active))
    for (k in layout.keys.filter { it != active }.sortedWith(compareBy({ layout.getValue(it).y }, { layout.getValue(it).x }))) {
        var m = out.getValue(k); var hit = placed.firstOrNull { m.overlaps(it) }
        while (hit != null) { m = m.copy(y = hit.y + hit.h); hit = placed.firstOrNull { m.overlaps(it) } }
        if (m.y + m.h > GRID_ROWS) return null
        out[k] = m; placed += m
    }
    val a = out.getValue(active); return if (a.y + a.h <= GRID_ROWS) out else null
}

/** Einstellungen der Seite "Aussehen" (§10.7), pro Gerät gespeichert. */
data class AppearancePrefs(
    val look: String = "Kabine", val scheme: AutoDjScheme = AutoDjScheme.Kabine,
    val wave: WaveStyle = WaveStyle.Solid, val view: DeckView = DeckView.Platter,
    val order: List<String> = listOf("deckA", "deckB", "mixer", "over", "lib", "fx", "pads"),
    val layouts: Map<String, Map<String, Cell>> = emptyMap(),   // Seite -> Modul -> Zelle
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
fun AutoDjTheme(scheme: AutoDjScheme = AutoDjScheme.Kabine, look: AutoDjLook = KabineLook, content: @Composable () -> Unit) =
    CompositionLocalProvider(LocalAutoDjColors provides scheme.colors, LocalAutoDjLook provides look, content = content)
