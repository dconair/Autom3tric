package com.autodj.app.ui.dj

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autodj.app.dj.DjColors
import com.autodj.app.dj.DjController
import com.autodj.app.dj.DjTheme
import com.autodj.app.dj.EchoTimes
import com.autodj.app.dj.PerformanceController
import com.autodj.app.dj.RollSizes
import com.autodj.app.dj.StutterRates
import java.util.Locale
import kotlin.math.roundToInt

private val EchoColor = Color(0xFF4FC3F7)
private val ReverbColor = Color(0xFFB39DDB)
private val FlangerColor = Color(0xFF58C27D)
private val RollColor = Color(0xFFE0A440)
private val StutterColor = Color(0xFFF06292)

/**
 * Effects on the whole output (decks, auto mix, pads, granular): echo, reverb and flanger are switched on and off, beat roll and
 * stutter work while the button is held. Everything that is tempo-bound follows the beat of the music that is playing.
 */
@Composable
fun EffectsScreen(dj: DjController, modifier: Modifier = Modifier) {
    val perf = dj.perf
    DjTheme {
        BoxWithConstraints(modifier.fillMaxSize().background(DjColors.bg)) {
            val wide = maxWidth >= 900.dp
            Column(
                Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(if (wide) 16.dp else 10.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                ClockBanner(dj)
                if (wide) {
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        EchoCard(perf, Modifier.weight(1f))
                        ReverbCard(perf, Modifier.weight(1f))
                        FlangerCard(perf, Modifier.weight(1f))
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        RollCard(perf, Modifier.weight(1f))
                        StutterCard(perf, Modifier.weight(1f))
                    }
                } else {
                    EchoCard(perf, Modifier.fillMaxWidth())
                    ReverbCard(perf, Modifier.fillMaxWidth())
                    FlangerCard(perf, Modifier.fillMaxWidth())
                    RollCard(perf, Modifier.fillMaxWidth())
                    StutterCard(perf, Modifier.fillMaxWidth())
                }
                Text(
                    "Tipp: Beat Roll und Stutter halten, solange du die Taste gedrückt hältst. Echo, Hall und Flanger bleiben an, bis du sie ausschaltest.",
                    color = DjColors.muted, fontSize = 12.sp,
                )
            }
        }
    }
}

/** Which tempo the effects (and the drum pads) follow right now, and where it comes from. */
@Composable
internal fun ClockBanner(dj: DjController, modifier: Modifier = Modifier) {
    val s = dj.snapshot
    val free by dj.perf.param("transport.bpm")
    val (bpm, source) = when {
        s.plan.active && s.plan.bpm > 0 -> s.plan.bpm to "vom Auto-Mix"
        s.deck[0].playing && s.deck[0].bpm > 0 -> s.deck[0].bpm to "von Deck A"
        s.deck[1].playing && s.deck[1].bpm > 0 -> s.deck[1].bpm to "von Deck B"
        else -> free.toDouble() to "freier Takt, kein Song läuft"
    }
    Row(
        modifier.fillMaxWidth().clip(RoundedCornerShape(10.dp)).background(DjColors.panel).border(1.dp, DjColors.line, RoundedCornerShape(10.dp))
            .padding(horizontal = 14.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Text(String.format(Locale.US, "%.1f", bpm), color = DjColors.text, fontSize = 22.sp, fontWeight = FontWeight.SemiBold)
        Text("BPM", color = DjColors.muted, fontSize = 11.sp)
        Text("Takt $source", color = DjColors.muted, fontSize = 12.sp, modifier = Modifier.weight(1f), maxLines = 1)
    }
}

@Composable
private fun EchoCard(perf: PerformanceController, modifier: Modifier) {
    val on = perf.param("fx.echo.on").value >= 0.5f
    FxCard("Echo", "Wiederholungen im Takt, wandern zwischen links und rechts", EchoColor, on, { perf.setOn("fx.echo.on", it) }, modifier) {
        ChipRow("Abstand", EchoTimes, perf.param("fx.echo.time").value.roundToInt(), EchoColor) { perf.set("fx.echo.time", it.toFloat()) }
        FxSlider("Wiederholungen", perf.param("fx.echo.feedback").value, 0f, 0.92f, EchoColor, { "${(it / 0.92f * 100).roundToInt()} %" }) { perf.set("fx.echo.feedback", it) }
        FxSlider("Pegel", perf.param("fx.echo.mix").value, 0f, 1f, EchoColor, { "${(it * 100).roundToInt()} %" }) { perf.set("fx.echo.mix", it) }
    }
}

@Composable
private fun ReverbCard(perf: PerformanceController, modifier: Modifier) {
    val on = perf.param("fx.reverb.on").value >= 0.5f
    FxCard("Hall", "Raum um den ganzen Mix", ReverbColor, on, { perf.setOn("fx.reverb.on", it) }, modifier) {
        FxSlider("Menge", perf.param("fx.reverb.mix").value, 0f, 1f, ReverbColor, { "${(it * 100).roundToInt()} %" }) { perf.set("fx.reverb.mix", it) }
    }
}

@Composable
private fun FlangerCard(perf: PerformanceController, modifier: Modifier) {
    val on = perf.param("fx.flanger.on").value >= 0.5f
    FxCard("Flanger", "Düsenartiges Wandern des Klangs", FlangerColor, on, { perf.setOn("fx.flanger.on", it) }, modifier) {
        FxSlider("Intensität", perf.param("fx.flanger.mix").value, 0f, 1f, FlangerColor, { "${(it * 100).roundToInt()} %" }) { perf.set("fx.flanger.mix", it) }
    }
}

@Composable
private fun RollCard(perf: PerformanceController, modifier: Modifier) {
    val on = perf.param("fx.roll.on").value >= 0.5f
    FxCard("Beat Roll", "Hält den letzten Schnipsel fest und wiederholt ihn", RollColor, null, {}, modifier) {
        ChipRow("Länge (Beats)", RollSizes, perf.param("fx.roll.size").value.roundToInt(), RollColor) { perf.set("fx.roll.size", it.toFloat()) }
        HoldButton("Halten für Roll", RollColor, on) { perf.setOn("fx.roll.on", it) }
    }
}

@Composable
private fun StutterCard(perf: PerformanceController, modifier: Modifier) {
    val on = perf.param("fx.stutter.on").value >= 0.5f
    FxCard("Stutter", "Zerhackt den Klang im Takt, wie ein Gate", StutterColor, null, {}, modifier) {
        ChipRow("Rate (Note)", StutterRates, perf.param("fx.stutter.rate").value.roundToInt(), StutterColor) { perf.set("fx.stutter.rate", it.toFloat()) }
        HoldButton("Halten für Stutter", StutterColor, on) { perf.setOn("fx.stutter.on", it) }
    }
}

// ---- widgets (shared with the pads screen) -------------------------------------------------------------

/** A panel with a title; [on] != null adds the switch in the corner. */
@Composable
internal fun FxCard(
    title: String,
    subtitle: String,
    color: Color,
    on: Boolean?,
    onChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
    content: @Composable ColumnScope.() -> Unit,
) {
    Column(
        modifier.clip(RoundedCornerShape(12.dp)).background(DjColors.panel).border(1.dp, if (on == true) color.copy(alpha = 0.7f) else DjColors.line, RoundedCornerShape(12.dp)).padding(14.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text(title, color = DjColors.text, fontSize = 17.sp, fontWeight = FontWeight.SemiBold)
                Text(subtitle, color = DjColors.muted, fontSize = 12.sp)
            }
            if (on != null) SwitchChip(if (on) "An" else "Aus", on, color) { onChange(!on) }
        }
        content()
    }
}

@Composable
internal fun SwitchChip(label: String, on: Boolean, color: Color, enabled: Boolean = true, onClick: () -> Unit) {
    Box(
        Modifier.clip(RoundedCornerShape(8.dp)).background(if (on) color else DjColors.panelRaised)
            .border(1.dp, if (on) color else DjColors.line, RoundedCornerShape(8.dp))
            .clickable(enabled = enabled, role = Role.Switch, onClick = onClick).padding(horizontal = 18.dp, vertical = 8.dp),
    ) { Text(label, color = if (on) Color.Black else DjColors.text, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, maxLines = 1) }
}

@Composable
internal fun ChipRow(label: String, options: List<String>, selected: Int, color: Color, onSelect: (Int) -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        Text(label, color = DjColors.muted, fontSize = 12.sp, modifier = Modifier.width(100.dp), maxLines = 1)
        options.forEachIndexed { i, o ->
            val on = i == selected
            Box(
                Modifier.clip(RoundedCornerShape(50)).background(if (on) color.copy(alpha = 0.22f) else DjColors.panelRaised)
                    .border(1.dp, if (on) color else DjColors.line, RoundedCornerShape(50))
                    .clickable { onSelect(i) }.padding(horizontal = 12.dp, vertical = 5.dp),
            ) { Text(o, color = if (on) DjColors.text else DjColors.muted, fontSize = 12.sp, maxLines = 1) }
        }
    }
}

@Composable
internal fun FxSlider(label: String, value: Float, min: Float, max: Float, color: Color, format: (Float) -> String, onChange: (Float) -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(label, color = DjColors.muted, fontSize = 12.sp, modifier = Modifier.width(100.dp), maxLines = 1)
        Slider(
            value = value.coerceIn(min, max), onValueChange = onChange, valueRange = min..max, modifier = Modifier.weight(1f),
            colors = SliderDefaults.colors(thumbColor = color, activeTrackColor = color, inactiveTrackColor = DjColors.line),
        )
        Text(format(value), color = DjColors.text, fontSize = 12.sp, modifier = Modifier.width(52.dp), maxLines = 1)
    }
}

/** A button that is "on" exactly as long as it is held down (mouse button or finger). */
@Composable
internal fun HoldButton(label: String, color: Color, held: Boolean, onHold: (Boolean) -> Unit) {
    var pressed by remember { mutableStateOf(false) }
    val lit = pressed || held
    Box(
        Modifier.fillMaxWidth().height(64.dp).clip(RoundedCornerShape(12.dp)).background(if (lit) color else color.copy(alpha = 0.16f))
            .border(1.dp, color, RoundedCornerShape(12.dp))
            .pointerInput(Unit) {
                detectTapGestures(onPress = {
                    pressed = true
                    onHold(true)
                    try {
                        tryAwaitRelease()
                    } finally {
                        pressed = false
                        onHold(false)
                    }
                })
            },
        contentAlignment = Alignment.Center,
    ) { Text(label, color = if (lit) Color.Black else DjColors.text, fontSize = 15.sp, fontWeight = FontWeight.SemiBold, maxLines = 1) }
}
