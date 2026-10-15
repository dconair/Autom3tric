package com.autodj.app.ui.dj

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.autodj.app.dj.DjController
import com.autodj.app.dj.EchoTimes
import com.autodj.app.dj.PerformanceController
import com.autodj.app.dj.RollSizes
import com.autodj.app.dj.StutterRates
import com.autodj.app.design.AutoDjSpace
import com.autodj.app.design.AutoDjType
import com.autodj.app.design.Btn
import com.autodj.app.design.Display
import com.autodj.app.design.Ds
import com.autodj.app.design.HoldSurface
import com.autodj.app.design.Segment
import com.autodj.app.design.Tile
import com.autodj.app.design.Txt
import com.autodj.app.design.riseIn
import com.autodj.app.ui.Knob
import kotlin.math.roundToInt

/**
 * Effects on the whole output (decks, auto mix, pads, granular). Echo, reverb and flanger are switched on and off; beat roll and
 * stutter work while their surface is held; the freeze captures the last seconds of the output into the granular instrument.
 * Everything that is tempo-bound follows the beat of the music that plays.
 */
@Composable
fun EffectsScreen(dj: DjController, modifier: Modifier = Modifier, onCaptureToGranular: ((seconds: Double) -> Unit)? = null) {
    val perf = dj.perf
    BoxWithConstraints(modifier.fillMaxSize()) {
        val wide = maxWidth >= 900.dp
        val margin = if (wide) AutoDjSpace.margin else AutoDjSpace.phoneMargin
        val gap = if (wide) AutoDjSpace.gap else AutoDjSpace.phoneGap
        if (wide) {
            Column(Modifier.fillMaxSize().padding(horizontal = margin).padding(bottom = margin), verticalArrangement = Arrangement.spacedBy(gap)) {
                Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(gap)) {
                    EchoTile(perf, Modifier.weight(6f).fillMaxHeight(), 0)
                    ReverbTile(perf, Modifier.weight(3f).fillMaxHeight(), 1)
                    FlangerTile(perf, Modifier.weight(3f).fillMaxHeight(), 2)
                }
                Row(Modifier.fillMaxWidth().weight(2f), horizontalArrangement = Arrangement.spacedBy(gap)) {
                    RollTile(perf, Modifier.weight(4f).fillMaxHeight(), 3)
                    StutterTile(perf, Modifier.weight(4f).fillMaxHeight(), 4)
                    FreezeTile(onCaptureToGranular, Modifier.weight(4f).fillMaxHeight(), 5)
                }
            }
        } else {
            Column(
                Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(horizontal = margin).padding(bottom = margin),
                verticalArrangement = Arrangement.spacedBy(gap),
            ) {
                EchoTile(perf, Modifier.fillMaxWidth(), 0)
                Row(horizontalArrangement = Arrangement.spacedBy(gap)) {
                    ReverbTile(perf, Modifier.weight(1f), 1)
                    FlangerTile(perf, Modifier.weight(1f), 2)
                }
                RollTile(perf, Modifier.fillMaxWidth().heightIn(min = 200.dp), 3)
                StutterTile(perf, Modifier.fillMaxWidth().heightIn(min = 200.dp), 4)
                FreezeTile(onCaptureToGranular, Modifier.fillMaxWidth(), 5)
            }
        }
    }
}

private fun pct(v: Float) = "${(v * 100).roundToInt()} %"

/** Title, one sentence, and the on/off action on the right. */
@Composable
private fun FxHead(title: String, sentence: String, on: Boolean?, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.Top, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Column(Modifier.weight(1f)) {
            Txt(title, style = AutoDjType.lead)
            Txt(sentence, style = AutoDjType.small, second = true, modifier = Modifier.padding(top = 2.dp))
        }
        if (on != null) Btn(if (on) "An" else "Aus", onClick = { onChange(!on) }, primary = on, small = true, modifier = Modifier.height(36.dp))
    }
}

@Composable
private fun EchoTile(perf: PerformanceController, modifier: Modifier, index: Int) {
    val on = perf.param("fx.echo.on").value >= 0.5f
    Tile(modifier.riseIn(index)) {
        FxHead("Echo", "Wiederholungen im Takt, die zwischen links und rechts wandern.", on) { perf.setOn("fx.echo.on", it) }
        Box(Modifier.weight(1f))
        Row(Modifier.fillMaxWidth().padding(top = 12.dp), verticalAlignment = Alignment.Bottom) {
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Txt("Abstand", style = AutoDjType.small, second = true)
                Segment(EchoTimes, perf.param("fx.echo.time").value.roundToInt(), { perf.set("fx.echo.time", it.toFloat()) })
            }
            Row(horizontalArrangement = Arrangement.spacedBy(24.dp), verticalAlignment = Alignment.Bottom) {
                val fb = perf.param("fx.echo.feedback").value
                Knob("Wiederholung", pct(fb / 0.92f), fb / 0.92f, { perf.set("fx.echo.feedback", it * 0.92f) }, size = 72.dp, onReset = { perf.set("fx.echo.feedback", 0.4f) })
                val mix = perf.param("fx.echo.mix").value
                Knob("Pegel", pct(mix), mix, { perf.set("fx.echo.mix", it) }, size = 72.dp, onReset = { perf.set("fx.echo.mix", 0.3f) })
            }
        }
    }
}

@Composable
private fun ReverbTile(perf: PerformanceController, modifier: Modifier, index: Int) {
    val on = perf.param("fx.reverb.on").value >= 0.5f
    Tile(modifier.riseIn(index)) {
        FxHead("Hall", "Raum um den ganzen Mix.", on) { perf.setOn("fx.reverb.on", it) }
        Box(Modifier.weight(1f))
        val mix = perf.param("fx.reverb.mix").value
        Box(Modifier.fillMaxWidth().padding(top = 12.dp), contentAlignment = Alignment.Center) {
            Knob("Menge", pct(mix), mix, { perf.set("fx.reverb.mix", it) }, size = 88.dp, onReset = { perf.set("fx.reverb.mix", 0.3f) })
        }
    }
}

@Composable
private fun FlangerTile(perf: PerformanceController, modifier: Modifier, index: Int) {
    val on = perf.param("fx.flanger.on").value >= 0.5f
    Tile(modifier.riseIn(index)) {
        FxHead("Flanger", "Düsenartiges Wandern des Klangs.", on) { perf.setOn("fx.flanger.on", it) }
        Box(Modifier.weight(1f))
        val mix = perf.param("fx.flanger.mix").value
        Box(Modifier.fillMaxWidth().padding(top = 12.dp), contentAlignment = Alignment.Center) {
            Knob("Intensität", pct(mix), mix, { perf.set("fx.flanger.mix", it) }, size = 88.dp, onReset = { perf.set("fx.flanger.mix", 0.4f) })
        }
    }
}

@Composable
private fun RollTile(perf: PerformanceController, modifier: Modifier, index: Int) {
    val c = Ds.c
    Tile(modifier.riseIn(index)) {
        FxHead("Beat Roll", "Hält den letzten Schnipsel fest und wiederholt ihn.", null) {}
        Row(Modifier.padding(top = 12.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Txt("Länge in Beats", style = AutoDjType.small, second = true)
            Segment(RollSizes, perf.param("fx.roll.size").value.roundToInt(), { perf.set("fx.roll.size", it.toFloat()) }, small = true)
        }
        HoldSurface(
            "Halten für Roll", c.padFx, c.ink, { perf.setOn("fx.roll.on", true) }, { perf.setOn("fx.roll.on", false) },
            Modifier.weight(1f).fillMaxWidth().padding(top = 12.dp),
        )
    }
}

@Composable
private fun StutterTile(perf: PerformanceController, modifier: Modifier, index: Int) {
    val c = Ds.c
    Tile(modifier.riseIn(index)) {
        FxHead("Stutter", "Zerhackt den Klang im Takt, wie ein Gate.", null) {}
        Row(Modifier.padding(top = 12.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Txt("Rate", style = AutoDjType.small, second = true)
            Segment(StutterRates, perf.param("fx.stutter.rate").value.roundToInt(), { perf.set("fx.stutter.rate", it.toFloat()) }, small = true)
        }
        HoldSurface(
            "Halten für Stutter", c.deckBField, TextOnSlate, { perf.setOn("fx.stutter.on", true) }, { perf.setOn("fx.stutter.on", false) },
            Modifier.weight(1f).fillMaxWidth().padding(top = 12.dp),
        )
    }
}

private val TextOnSlate = androidx.compose.ui.graphics.Color.White

private val FreezeSeconds = listOf(2, 4, 8)

@Composable
private fun FreezeTile(onCapture: ((seconds: Double) -> Unit)?, modifier: Modifier, index: Int) {
    var seconds by remember { mutableIntStateOf(1) }
    Tile(modifier.riseIn(index)) {
        FxHead("Granular-Freeze", "Fängt die letzten Sekunden des Ausgangs ein und legt sie als Quelle ins Granular-Instrument.", null) {}
        Row(Modifier.padding(top = 12.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Txt("Länge", style = AutoDjType.small, second = true)
            Segment(FreezeSeconds.map { "$it s" }, seconds, { seconds = it }, small = true)
        }
        Display(Modifier.weight(1f).fillMaxWidth().padding(top = 0.dp)) {
            Txt("Die letzten ${FreezeSeconds[seconds]} Sekunden werden eingefangen.", style = AutoDjType.small, second = true, modifier = Modifier.align(Alignment.Center).padding(16.dp))
        }
        Btn(
            "Einfangen und in Granular öffnen", onClick = { onCapture?.invoke(FreezeSeconds[seconds].toDouble()) }, primary = true, enabled = onCapture != null,
            modifier = Modifier.fillMaxWidth().padding(top = 12.dp),
        )
    }
}
