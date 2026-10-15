package com.autodj.app.ui

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.scale
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextMeasurer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autodj.app.dj.DjColors
import com.autodj.app.dj.DjTheme
import com.autodj.app.dj.camelotColor
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.sin

// ---- content ----------------------------------------------------------------------------------------------------------

/** The animation that belongs to a step of the guide. */
enum class GuideScene { Library, Decks, AutoMix, Effects, Pads, PluginPlay, PluginLoad, PluginBeat, Knobs }

/** What the "try it now" button of a step does. The apps map these to their own navigation. */
enum class GuideTarget { LoadDemoSongs, Library, AutoMix, Decks, Effects, Pads, Plugin, PluginLoadFile, PluginBeatStart }

class GuideStep(val title: String, val text: String, val scene: GuideScene, val action: GuideTarget? = null, val actionLabel: String = "")

class GuideLesson(val name: String, val steps: List<GuideStep>)

/** The two lessons: the Auto-DJ as a whole, and the granular plugin. Plain words, no jargon without an explanation. */
object Guides {
    const val AUTO_DJ = 0
    const val PLUGIN = 1

    val autoDj = GuideLesson(
        "Auto-DJ",
        listOf(
            GuideStep(
                "1. Songs in die Bibliothek",
                "Drücke „Songs hinzufügen“ und wähle Musik von deinem Gerät. Die App hört jeden Song durch und erkennt Tempo, Takt, Tonart und Aufbau. " +
                    "Ohne eigene Musik probierst du es mit den Demo-Songs.",
                GuideScene.Library, GuideTarget.LoadDemoSongs, "Demo-Songs laden",
            ),
            GuideStep(
                "2. Der Auto-Mix",
                "Im Reiter „Auto-Mix“ legst du fest, welche Songs laufen sollen („Alle Songs hinzufügen“). Mit ▶ mischt die App selbst: " +
                    "Sie gleicht das Tempo an, legt die Takte aufeinander und blendet weich über. Mit „Als WAV speichern“ nimmst du den ganzen Mix auf.",
                GuideScene.AutoMix, GuideTarget.AutoMix, "Zum Auto-Mix",
            ),
            GuideStep(
                "3. Selbst mixen mit den Decks",
                "Lade in der Bibliothek einen Song auf Deck A und einen auf Deck B (Knöpfe „A“ und „B“). Starte beide und drücke „Sync“: " +
                    "Dann laufen sie im gleichen Takt. Den Übergang machst du mit dem Crossfader in der Mitte.",
                GuideScene.Decks, GuideTarget.Decks, "Zu den Decks",
            ),
            GuideStep(
                "4. Effekte",
                "Echo, Hall und Flanger schaltest du ein und aus. Beat Roll und Stutter wirken, solange du die Taste hältst. " +
                    "Alles richtet sich nach dem Takt der laufenden Musik und wirkt auf alles, was du hörst.",
                GuideScene.Effects, GuideTarget.Effects, "Zu den Effekten",
            ),
            GuideStep(
                "5. Eigene Beats mit den Pads",
                "Die 16 Pads spielen Drum-Klänge, wenn du sie antippst. Unten im Raster setzt du Punkte: Der Sequenzer spielt sie im Takt der Musik. " +
                    "So legst du einen eigenen Beat unter den Mix.",
                GuideScene.Pads, GuideTarget.Pads, "Zu den Pads",
            ),
        ),
    )

    val plugin = GuideLesson(
        "Granular-Plugin",
        listOf(
            GuideStep(
                "1. Das Plugin spielen",
                "Das Plugin zerlegt Musik in winzige Schnipsel („Körner“). Die dunkle Fläche zeigt deine Datei als Welle: " +
                    "Drücke darauf und ziehe, dann hörst du die Stelle unter deinem Finger. Loslassen beendet den Ton.",
                GuideScene.PluginPlay, GuideTarget.Plugin, "Zum Plugin",
            ),
            GuideStep(
                "2. Eigene Datei laden",
                "Mit „+ Datei laden“ wählst du einen Song oder ein Geräusch. Am Mac kannst du eine Datei auch einfach ins Fenster ziehen. " +
                    "Sie erscheint als Quelle und kann auf jede der vier Stimmen gelegt werden.",
                GuideScene.PluginLoad, GuideTarget.PluginLoadFile, "Datei wählen",
            ),
            GuideStep(
                "3. Einen Beat bauen",
                "Stelle auf „step“. Im Raster ist jede Spalte ein Schritt, jede Zeile ein Stück deiner Datei. Tippe einen Punkt, damit dieser Schritt dieses Stück spielt. " +
                    "„Beat-Start“ richtet alles passend ein und startet ein Muster, das du dann Punkt für Punkt änderst.",
                GuideScene.PluginBeat, GuideTarget.PluginBeatStart, "Beat-Start",
            ),
            GuideStep(
                "4. Regler und sauberer Klang",
                "Drehregler ziehst du mit der Maus oder dem Finger nach oben (Doppelklick = Standard). Für einen sauberen Klang: " +
                    "Hüllkurve „hann“ oder „gauss“, nicht zu viele Körner, „chaos“ niedrig, „output“ so, dass nichts übersteuert.",
                GuideScene.Knobs,
            ),
        ),
    )

    val all = listOf(autoDj, plugin)
}

// ---- state ------------------------------------------------------------------------------------------------------------

/**
 * Whether the guide is open, where it is, and which small tips the user has dismissed. [seen] is the stored "the first-start guide
 * was shown" flag; [onSeen] stores it. The guide opens on its own the first time only.
 */
class GuideState(seen: Boolean, private val onSeen: () -> Unit = {}) {
    var open by mutableStateOf(!seen)
    var lesson by mutableIntStateOf(Guides.AUTO_DJ)
    var step by mutableIntStateOf(0)
    val dismissedTips = mutableStateListOf<String>()

    fun show(lesson: Int = this.lesson, step: Int = 0) {
        this.lesson = lesson.coerceIn(0, Guides.all.size - 1)
        this.step = step.coerceIn(0, Guides.all[this.lesson].steps.size - 1)
        open = true
    }

    fun close() {
        open = false
        onSeen()
    }
}

val LocalGuide = staticCompositionLocalOf<GuideState?> { null }

// ---- the overlay ------------------------------------------------------------------------------------------------------

/**
 * The guide: a card over the app with an animated picture, a short text, and a "try it now" button per step. [phase] fixes the
 * animation at one moment (tests and screenshots); null lets it run.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun GuideOverlay(state: GuideState, onAction: (GuideTarget) -> Unit, modifier: Modifier = Modifier, phase: Float? = null) {
    if (!state.open) return
    DjTheme { GuideCard(state, onAction, modifier, phase) }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun GuideCard(state: GuideState, onAction: (GuideTarget) -> Unit, modifier: Modifier, phase: Float?) {
    val lessons = Guides.all
    val lessonIndex = state.lesson.coerceIn(0, lessons.size - 1)
    val lesson = lessons[lessonIndex]
    val stepIndex = state.step.coerceIn(0, lesson.steps.size - 1)
    val step = lesson.steps[stepIndex]
    val last = stepIndex == lesson.steps.size - 1
    val shape = RoundedCornerShape(16.dp)
    Box(
        modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.66f)).pointerInput(Unit) { detectTapGestures { } },
        contentAlignment = Alignment.Center,
    ) {
        Column(
            Modifier.padding(14.dp).widthIn(max = 640.dp).fillMaxWidth().clip(shape).background(DjColors.panel).border(1.dp, DjColors.line, shape)
                .verticalScroll(rememberScrollState()).padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    lessons.forEachIndexed { i, l -> LessonChip(l.name, i == lessonIndex) { state.show(i) } }
                }
                Text(
                    "✕", color = DjColors.muted, fontSize = 20.sp,
                    modifier = Modifier.clip(CircleShape).clickable(role = Role.Button, onClickLabel = "Anleitung schließen") { state.close() }
                        .semantics { contentDescription = "Anleitung schließen" }.padding(horizontal = 10.dp, vertical = 4.dp),
                )
            }
            val t = rememberGuidePhase(phase)
            val measurer = rememberTextMeasurer()
            Canvas(
                Modifier.fillMaxWidth().height(150.dp).clip(RoundedCornerShape(12.dp)).background(DjColors.bg)
                    .semantics { contentDescription = "Animation zu: ${step.title}" },
            ) { drawScene(step.scene, t, measurer) }
            Text(step.title, color = DjColors.text, fontSize = 20.sp, fontWeight = FontWeight.SemiBold)
            Text(step.text, color = DjColors.text.copy(alpha = 0.85f), fontSize = 14.sp, lineHeight = 21.sp)
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.Center) {
                lesson.steps.indices.forEach { i ->
                    Box(
                        Modifier.padding(horizontal = 4.dp).size(if (i == stepIndex) 9.dp else 7.dp).clip(CircleShape)
                            .background(if (i == stepIndex) DjColors.deckA else DjColors.line),
                    )
                }
            }
            FlowRow(
                Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp, Alignment.End),
                verticalArrangement = Arrangement.spacedBy(6.dp), itemVerticalAlignment = Alignment.CenterVertically,
            ) {
                if (stepIndex > 0) TextButton(onClick = { state.step = stepIndex - 1 }) { Text("Zurück", color = DjColors.muted) }
                val action = step.action
                if (action != null) {
                    OutlinedButton(onClick = {
                        state.close()
                        onAction(action)
                    }) { Text(step.actionLabel, color = DjColors.text) }
                }
                if (last) {
                    Button(onClick = { state.close() }) { Text("Fertig") }
                } else {
                    Button(onClick = { state.step = stepIndex + 1 }) { Text("Weiter") }
                }
            }
        }
    }
}

@Composable
private fun LessonChip(name: String, selected: Boolean, onClick: () -> Unit) {
    Text(
        name, color = if (selected) Color.Black else DjColors.text, fontSize = 13.sp, fontWeight = FontWeight.Medium,
        modifier = Modifier.clip(RoundedCornerShape(50)).background(if (selected) DjColors.deckA else DjColors.panelRaised)
            .clickable(role = Role.Tab, onClick = onClick).padding(horizontal = 14.dp, vertical = 7.dp),
    )
}

@Composable
private fun rememberGuidePhase(fixed: Float?): Float {
    if (fixed != null) return fixed
    val transition = rememberInfiniteTransition(label = "guide")
    val t by transition.animateFloat(
        0f, 1f, infiniteRepeatable(tween(6000, easing = LinearEasing), RepeatMode.Restart), label = "guide-phase",
    )
    return t
}

// ---- tips inside the tabs ---------------------------------------------------------------------------------------------

/**
 * A short, dismissible explanation at the top of a tab, with a link to the matching page of the guide. Only shown inside an app
 * that provides [LocalGuide].
 */
@Composable
fun TabTip(id: String, text: String, modifier: Modifier = Modifier, lesson: Int = Guides.AUTO_DJ, step: Int = 0) {
    val guide = LocalGuide.current ?: return
    if (id in guide.dismissedTips) return
    Row(
        modifier.fillMaxWidth().clip(RoundedCornerShape(10.dp)).background(DjColors.panelRaised).padding(start = 12.dp, top = 4.dp, bottom = 4.dp, end = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Text("ⓘ", color = DjColors.deckA, fontSize = 16.sp)
        Text(text, color = DjColors.text.copy(alpha = 0.85f), fontSize = 12.sp, lineHeight = 17.sp, modifier = Modifier.weight(1f))
        TextButton(onClick = { guide.show(lesson, step) }) { Text("Anleitung", color = DjColors.deckA, fontSize = 12.sp) }
        Text(
            "✕", color = DjColors.muted, fontSize = 14.sp,
            modifier = Modifier.clip(CircleShape).clickable(role = Role.Button) { guide.dismissedTips.add(id) }
                .semantics { contentDescription = "Hinweis ausblenden" }.padding(horizontal = 8.dp, vertical = 4.dp),
        )
    }
}

// ---- the animated pictures ----------------------------------------------------------------------------------------------

private fun seg(t: Float, a: Float, b: Float) = ((t - a) / (b - a)).coerceIn(0f, 1f)
private fun smooth(x: Float) = x * x * (3f - 2f * x)
private fun lerp(a: Float, b: Float, f: Float) = a + (b - a) * f
private fun Color.a(x: Float) = copy(alpha = x.coerceIn(0f, 1f))

// The pictures are drawn in dp units: the canvas is scaled by the density, W and H are its width and height in dp.
private val DrawScope.W get() = size.width / density
private val DrawScope.H get() = size.height / density

private fun DrawScope.txt(m: TextMeasurer, s: String, x: Float, y: Float, color: Color, size: Float = 11f, weight: FontWeight = FontWeight.Normal) {
    drawText(m, s, Offset(x, y), TextStyle(color = color, fontSize = (size / (density * fontScale)).sp, fontWeight = weight))
}

/** A whole picture fades out at the end of the loop, so that the restart is not a jump. */
private fun fadeOut(t: Float) = 1f - smooth(seg(t, 0.93f, 1f))

private fun DrawScope.drawScene(scene: GuideScene, t: Float, m: TextMeasurer) = scale(density, density, Offset.Zero) {
    when (scene) {
        GuideScene.Library -> sceneLibrary(t, m)
        GuideScene.Decks -> sceneDecks(t, m)
        GuideScene.AutoMix -> sceneAutoMix(t, m)
        GuideScene.Effects -> sceneEffects(t, m)
        GuideScene.Pads -> scenePads(t, m)
        GuideScene.PluginPlay -> scenePluginPlay(t, m)
        GuideScene.PluginLoad -> scenePluginLoad(t, m)
        GuideScene.PluginBeat -> scenePluginBeat(t, m)
        GuideScene.Knobs -> sceneKnobs(t, m)
    }
}

private fun DrawScope.sceneLibrary(t: Float, m: TextMeasurer) {
    val fade = fadeOut(t)
    val names = listOf("Sommer.mp3", "Nachtfahrt.wav", "Club.flac")
    val bpm = listOf("124", "128", "122")
    val keys = listOf("8A", "9A", "8B")
    val energy = listOf(0.55f, 0.85f, 0.7f)
    val rowH = (H - 54f) / 3f
    drawRoundRect(DjColors.panel.a(fade), Offset(12f, 10f), Size(W - 24f, H - 20f), CornerRadius(10f))
    for (i in 0 until 3) {
        val start = 0.05f + 0.22f * i
        val drop = smooth(seg(t, start, start + 0.16f))
        val y = lerp(-rowH, 20f + i * (rowH + 4f), drop)
        val al = drop * fade
        drawRoundRect(DjColors.panelRaised.a(al), Offset(20f, y), Size(W - 40f, rowH), CornerRadius(8f))
        txt(m, "♪ ${names[i]}", 30f, y + rowH / 2f - 7f, DjColors.text.a(al), 12f)
        val an = smooth(seg(t, start + 0.2f, start + 0.3f)) * fade
        txt(m, "${bpm[i]} BPM", W * 0.46f, y + rowH / 2f - 7f, DjColors.text.a(an), 12f)
        drawRoundRect(camelotColor(keys[i]).a(an), Offset(W * 0.62f, y + rowH / 2f - 9f), Size(34f, 18f), CornerRadius(9f))
        txt(m, keys[i], W * 0.62f + 8f, y + rowH / 2f - 7f, Color.Black.a(an), 11f, FontWeight.Bold)
        val bw = W * 0.18f
        drawRoundRect(DjColors.line.a(an), Offset(W * 0.74f, y + rowH / 2f - 3f), Size(bw, 6f), CornerRadius(3f))
        drawRoundRect(DjColors.good.a(an), Offset(W * 0.74f, y + rowH / 2f - 3f), Size(bw * energy[i], 6f), CornerRadius(3f))
    }
    txt(m, "wird analysiert: Tempo · Tonart · Energie", 24f, H - 26f, DjColors.muted.a(smooth(seg(t, 0.3f, 0.45f)) * fade), 10f)
}

private fun DrawScope.waveBars(top: Float, bandH: Float, color: Color, offset: Float, alpha: Float, played: Float) {
    val n = 56
    val bw = (W - 24f) / n
    val cy = top + bandH / 2f
    for (i in 0 until n) {
        val x = 12f + i * bw + offset
        if (x < 12f || x > W - 12f - bw) continue
        val amp = (0.25f + 0.75f * abs(sin(i * 0.7f) * cos(i * 0.23f + 1f))) * bandH / 2f
        val on = (x - 12f) / (W - 24f) <= played
        drawRoundRect(color.a(alpha * if (on) 0.55f else 1f), Offset(x, cy - amp), Size(bw * 0.62f, amp * 2f), CornerRadius(2f))
    }
}

private fun DrawScope.sceneDecks(t: Float, m: TextMeasurer) {
    val fade = fadeOut(t)
    val bandH = H * 0.28f
    val aTop = H * 0.08f
    val bTop = H * 0.40f
    val bw = (W - 24f) / 56f
    val offset = (1f - smooth(seg(t, 0.3f, 0.55f))) * bw * 2.6f
    val played = 0.1f + 0.8f * t
    waveBars(aTop, bandH, DjColors.deckA, 0f, fade, played)
    waveBars(bTop, bandH, DjColors.deckB, offset, fade, played)
    // beat lines: shifted for B until sync
    for (k in 0 until 7) {
        val x = 12f + k * bw * 8f
        drawLine(Color.White.a(0.35f * fade), Offset(x, aTop), Offset(x, aTop + bandH), 1.5f)
        drawLine(Color.White.a(0.35f * fade), Offset(x + offset, bTop), Offset(x + offset, bTop + bandH), 1.5f)
    }
    val px = 12f + (W - 24f) * played
    drawLine(Color.White.a(fade), Offset(px, aTop - 4f), Offset(px, bTop + bandH + 4f), 2f)
    txt(m, "A", 16f, aTop, DjColors.deckA.a(fade), 12f, FontWeight.Bold)
    txt(m, "B", 16f, bTop, DjColors.deckB.a(fade), 12f, FontWeight.Bold)
    val sync = smooth(seg(t, 0.5f, 0.6f)) * fade
    drawRoundRect(DjColors.good.a(sync), Offset(W - 76f, bTop + 4f), Size(58f, 20f), CornerRadius(10f))
    txt(m, "SYNC", W - 64f, bTop + 7f, Color.Black.a(sync), 11f, FontWeight.Bold)
    // crossfader
    val y = H - 24f
    drawRoundRect(DjColors.line.a(fade), Offset(W * 0.3f, y), Size(W * 0.4f, 6f), CornerRadius(3f))
    val kx = lerp(W * 0.34f, W * 0.66f, smooth(seg(t, 0.62f, 0.92f)))
    drawRoundRect(DjColors.text.a(fade), Offset(kx - 9f, y - 8f), Size(18f, 22f), CornerRadius(4f))
    txt(m, "Crossfader", W * 0.3f - 62f, y - 3f, DjColors.muted.a(fade), 10f)
}

private fun DrawScope.sceneAutoMix(t: Float, m: TextMeasurer) {
    val fade = fadeOut(t)
    val w = W
    val laneH = 34f
    val y0 = 20f
    val y1 = y0 + laneH + 14f
    data class Block(val from: Float, val to: Float, val lane: Int, val color: Color, val name: String)
    val blocks = listOf(
        Block(0.04f, 0.50f, 0, DjColors.deckA, "Song 1"),
        Block(0.40f, 0.78f, 1, DjColors.deckB, "Song 2"),
        Block(0.68f, 0.96f, 0, DjColors.deckA, "Song 3"),
    )
    blocks.forEachIndexed { i, b ->
        val appear = smooth(seg(t, 0.03f + 0.08f * i, 0.13f + 0.08f * i)) * fade
        val y = if (b.lane == 0) y0 else y1
        drawRoundRect(b.color.a(0.30f * appear), Offset(w * b.from, y), Size(w * (b.to - b.from), laneH), CornerRadius(8f))
        drawRoundRect(b.color.a(appear), Offset(w * b.from, y), Size(w * (b.to - b.from), laneH), CornerRadius(8f), Stroke(1.5f))
        txt(m, b.name, w * b.from + 8f, y + laneH / 2f - 7f, DjColors.text.a(appear), 11f)
    }
    // the two overlaps: volume curves cross
    for ((from, to) in listOf(0.40f to 0.50f, 0.68f to 0.78f)) {
        val show = smooth(seg(t, 0.3f, 0.45f)) * fade
        val x0 = w * from
        val x1 = w * to
        drawLine(DjColors.deckA.a(show), Offset(x0, y0 + 4f), Offset(x1, y0 + laneH - 4f), 2.5f, StrokeCap.Round)
        drawLine(DjColors.deckB.a(show), Offset(x0, y1 + laneH - 4f), Offset(x1, y1 + 4f), 2.5f, StrokeCap.Round)
        txt(m, "Übergang", x0 - 6f, y1 + laneH + 6f, DjColors.good.a(show), 10f)
    }
    val px = w * (0.04f + 0.92f * seg(t, 0.15f, 0.92f))
    drawLine(Color.White.a(fade), Offset(px, 8f), Offset(px, y1 + laneH + 4f), 2f)
    txt(m, "Tempo und Takt werden angeglichen", 14f, H - 22f, DjColors.muted.a(fade), 10f)
    val wav = smooth(seg(t, 0.85f, 0.93f)) * fade
    drawRoundRect(DjColors.good.a(wav), Offset(w - 88f, H - 28f), Size(76f, 20f), CornerRadius(10f))
    txt(m, "⬇ als WAV", w - 80f, H - 25f, Color.Black.a(wav), 11f, FontWeight.Bold)
}

private fun DrawScope.drawKnob(c: Offset, r: Float, frac: Float, color: Color, alpha: Float) {
    drawCircle(DjColors.panelRaised.a(alpha), r, c)
    drawArc(DjColors.line.a(alpha), 135f, 270f, false, Offset(c.x - r, c.y - r), Size(r * 2f, r * 2f), style = Stroke(5f, cap = StrokeCap.Round))
    drawArc(color.a(alpha), 135f, 270f * frac, false, Offset(c.x - r, c.y - r), Size(r * 2f, r * 2f), style = Stroke(5f, cap = StrokeCap.Round))
    val ang = (135f + 270f * frac) * PI.toFloat() / 180f
    drawLine(DjColors.text.a(alpha), c, Offset(c.x + cos(ang) * r * 0.75f, c.y + sin(ang) * r * 0.75f), 3f, StrokeCap.Round)
}

private fun DrawScope.sceneEffects(t: Float, m: TextMeasurer) {
    val fade = fadeOut(t)
    val cy = H * 0.48f
    // knob (echo amount)
    val frac = 0.5f + 0.5f * sin(t * 2f * PI.toFloat() - 1.4f)
    drawKnob(Offset(W * 0.17f, cy), 30f, frac, DjColors.deckA, fade)
    txt(m, "Echo", W * 0.17f - 14f, cy + 40f, DjColors.muted.a(fade), 11f)
    // echo: a hit and its repeats
    val x0 = W * 0.34f
    val prog = (t * 2f) % 1f
    for (k in 0 until 5) {
        val f = 0.62f.let { var v = 1f; repeat(k) { v *= it }; v }
        val vis = smooth(seg(prog, k * 0.14f, k * 0.14f + 0.08f)) * (1f - smooth(seg(prog, 0.85f, 1f)))
        val r = 14f * (0.5f + 0.5f * f)
        drawCircle(DjColors.deckB.a(vis * f * fade), r, Offset(x0 + k * 34f, cy))
    }
    txt(m, "Echo: der Klang wiederholt sich", x0 - 4f, cy + 40f, DjColors.muted.a(fade), 10f)
    // reverb: a ring that grows
    val rp = (t * 2f + 0.3f) % 1f
    val c = Offset(W * 0.85f, cy)
    drawCircle(DjColors.good.a((1f - rp) * 0.8f * fade), 6f + 28f * rp, c, style = Stroke(3f))
    drawCircle(DjColors.good.a(fade), 6f, c)
    txt(m, "Hall", c.x - 11f, cy + 46f, DjColors.muted.a(fade), 11f)
}

private fun DrawScope.scenePads(t: Float, m: TextMeasurer) {
    val fade = fadeOut(t)
    val step = ((t * 32f).toInt()) % 16
    val frac = (t * 32f) % 1f
    val patterns = listOf(setOf(0, 4, 8, 12), setOf(4, 12), setOf(2, 6, 10, 14))
    val colors = listOf(DjColors.deckA, DjColors.deckB, DjColors.warn)
    val names = listOf("Kick", "Snare", "HiHat")
    // 4x4 pads, the lowest row is lit by the sequencer
    val cell = 21f
    val gx = 18f
    val gy = 18f
    for (r in 0 until 4) for (c in 0 until 4) {
        val rowFromBottom = 3 - r
        var lit = 0f
        if (rowFromBottom == 0 && c in 0..2 && step in patterns[c]) lit = 1f - frac
        val base = if (rowFromBottom == 0 && c in 0..2) colors[c] else DjColors.line
        drawRoundRect(DjColors.panelRaised.a(fade), Offset(gx + c * (cell + 4f), gy + r * (cell + 4f)), Size(cell, cell), CornerRadius(6f))
        drawRoundRect(base.a((0.25f + 0.75f * lit) * fade), Offset(gx + c * (cell + 4f), gy + r * (cell + 4f)), Size(cell, cell), CornerRadius(6f))
    }
    // the sequencer rows
    val sx = W * 0.46f
    val cw = (W - sx - 16f) / 16f
    for (row in 0 until 3) {
        val y = 26f + row * 30f
        txt(m, names[row], sx - 38f, y + 4f, DjColors.muted.a(fade), 10f)
        for (s in 0 until 16) {
            val x = sx + s * cw
            val on = s in patterns[row]
            val cur = s == step
            drawRoundRect(
                if (on) colors[row].a((if (cur) 1f else 0.7f) * fade) else DjColors.panelRaised.a(fade),
                Offset(x, y), Size(cw - 3f, 20f), CornerRadius(3f),
            )
            if (cur) drawRoundRect(Color.White.a(0.28f * fade), Offset(x, y), Size(cw - 3f, 20f), CornerRadius(3f))
        }
    }
    txt(m, "Der Sequenzer spielt die Punkte im Takt", sx, H - 26f, DjColors.muted.a(fade), 10f)
}

private fun DrawScope.screenWave(top: Float, bottom: Float, grow: Float, alpha: Float, color: Color) {
    val n = 64
    val bw = (W - 40f) / n
    val cy = (top + bottom) / 2f
    for (i in 0 until n) {
        val amp = (0.12f + 0.88f * abs(sin(i * 0.5f) * cos(i * 0.17f + 0.6f))) * (bottom - top) / 2f * grow
        drawRoundRect(color.a(alpha), Offset(20f + i * bw, cy - amp), Size(bw * 0.6f, (amp * 2f).coerceAtLeast(2f)), CornerRadius(1.5f))
    }
}

private fun DrawScope.scenePluginPlay(t: Float, m: TextMeasurer) {
    val fade = fadeOut(t)
    drawRoundRect(Color(0xFF1B1D22).a(fade), Offset(8f, 8f), Size(W - 16f, H - 16f), CornerRadius(12f))
    screenWave(24f, H - 34f, 1f, 0.8f * fade, DjColors.wave)
    val fx = W * lerp(0.15f, 0.82f, smooth(seg(t, 0.08f, 0.88f)))
    val fy = H * 0.58f
    drawLine(DjColors.deckA.a(0.5f * fade), Offset(fx, 18f), Offset(fx, H - 30f), 1.5f)
    for (k in 0 until 16) {
        val ph = (t * 7f + k / 16f) % 1f
        val gx = fx + sin(k * 2.3f) * 16f
        val gy = fy - ph * H * 0.42f
        drawCircle(DjColors.deckA.a((1f - ph) * fade), 3.2f, Offset(gx, gy))
    }
    drawCircle(Color.White.a(0.30f * fade), 15f, Offset(fx, fy))
    drawCircle(Color.White.a(0.85f * fade), 15f, Offset(fx, fy), style = Stroke(2f))
    txt(m, "drücken und ziehen: Körner kommen von dieser Stelle", 22f, H - 26f, DjColors.muted.a(fade), 10f)
}

private fun DrawScope.scenePluginLoad(t: Float, m: TextMeasurer) {
    val fade = fadeOut(t)
    drawRoundRect(Color(0xFF1B1D22).a(fade), Offset(8f, 44f), Size(W - 16f, H - 52f), CornerRadius(12f))
    val pulse = 0.55f + 0.45f * sin(t * 2f * PI.toFloat() * 2f)
    val pillA = if (t < 0.2f) pulse else 1f
    drawRoundRect(DjColors.deckA.a(0.25f * pillA * fade), Offset(14f, 8f), Size(110f, 28f), CornerRadius(14f))
    drawRoundRect(DjColors.deckA.a(pillA * fade), Offset(14f, 8f), Size(110f, 28f), CornerRadius(14f), Stroke(1.5f))
    txt(m, "+ Datei laden", 26f, 14f, DjColors.text.a(fade), 12f)
    // the file flies in
    val fly = smooth(seg(t, 0.2f, 0.45f))
    if (t in 0.2f..0.5f) {
        val x = lerp(30f, W * 0.5f - 50f, fly)
        val y = lerp(20f, H * 0.5f, fly)
        drawRoundRect(DjColors.panelRaised.a(fade), Offset(x, y), Size(100f, 26f), CornerRadius(8f))
        txt(m, "♪ meinsong.mp3", x + 8f, y + 6f, DjColors.text.a(fade), 11f)
    }
    val load = seg(t, 0.45f, 0.6f)
    if (t in 0.45f..0.62f) {
        val y = H * 0.62f
        drawRoundRect(DjColors.line.a(fade), Offset(W * 0.3f, y), Size(W * 0.4f, 6f), CornerRadius(3f))
        drawRoundRect(DjColors.deckA.a(fade), Offset(W * 0.3f, y), Size(W * 0.4f * load, 6f), CornerRadius(3f))
        txt(m, "wird geladen …", W * 0.3f, y - 18f, DjColors.muted.a(fade), 10f)
    }
    val grow = smooth(seg(t, 0.62f, 0.8f))
    screenWave(56f, H - 18f, if (t < 0.62f) 0.02f else grow.coerceAtLeast(0.02f), 0.8f * fade, DjColors.wave)
    if (t > 0.8f) txt(m, "Fertig: die Datei ist jetzt die Quelle", 22f, H - 30f, DjColors.good.a(smooth(seg(t, 0.8f, 0.86f)) * fade), 11f)
}

private fun DrawScope.scenePluginBeat(t: Float, m: TextMeasurer) {
    val fade = fadeOut(t)
    val rows = 4
    val x0 = 40f
    val cw = (W - x0 - 16f) / 16f
    val rh = 24f
    val y0 = 16f
    val hits = listOf(0 to 0, 3 to 2, 6 to 1, 8 to 3, 10 to 2, 13 to 3)
    val shown = if (t < 0.5f) (t / 0.5f * (hits.size + 1)).toInt() else hits.size
    val cursor = if (t < 0.5f) -1 else ((t - 0.5f) / 0.5f * 32f).toInt() % 16
    for (r in 0 until rows) txt(m, "${r + 1}", 22f, y0 + r * rh + 4f, DjColors.muted.a(fade), 10f)
    for (s in 0 until 16) {
        val x = x0 + s * cw
        drawRoundRect((if ((s / 4) % 2 == 0) DjColors.panelRaised else DjColors.panel).a(fade), Offset(x, y0 - 2f), Size(cw - 2f, rows * rh + 2f), CornerRadius(3f))
        if (s == cursor) drawRoundRect(Color.White.a(0.18f * fade), Offset(x, y0 - 2f), Size(cw - 2f, rows * rh + 2f), CornerRadius(3f))
    }
    hits.forEachIndexed { k, (s, r) ->
        if (k < shown) {
            val x = x0 + s * cw
            drawRoundRect((if (s == cursor) Color.White else DjColors.deckA).a(fade), Offset(x + 1f, y0 + r * rh + 2f), Size(cw - 4f, rh - 4f), CornerRadius(4f))
        }
    }
    val msg = if (t < 0.5f) "Tippe Punkte ins Raster: Spalte = Schritt, Zeile = Stück der Datei" else "Das Muster läuft im Takt"
    txt(m, msg, 22f, H - 30f, DjColors.muted.a(fade), 10f)
}

private fun DrawScope.sceneKnobs(t: Float, m: TextMeasurer) {
    val fade = fadeOut(t)
    val cy = H * 0.42f
    val names = listOf("length", "grains", "tune")
    val color = listOf(DjColors.deckA, DjColors.deckB, DjColors.good)
    for (i in 0 until 3) {
        val f = 0.5f + 0.4f * sin(t * 2f * PI.toFloat() + i * 2.1f)
        val c = Offset(W * (0.2f + 0.3f * i), cy)
        drawKnob(c, 28f, f, color[i], fade)
        txt(m, names[i], c.x - 18f, cy + 38f, DjColors.muted.a(fade), 11f)
    }
    // the hand that drags the middle knob up and down
    val hy = cy - 6f - 26f * sin(t * 2f * PI.toFloat() + 2.1f)
    drawCircle(Color.White.a(0.3f * fade), 11f, Offset(W * 0.5f + 40f, hy))
    drawCircle(Color.White.a(0.85f * fade), 11f, Offset(W * 0.5f + 40f, hy), style = Stroke(2f))
}
