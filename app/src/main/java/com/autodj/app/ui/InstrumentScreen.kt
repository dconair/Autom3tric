package com.autodj.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.heightIn
import com.autodj.app.design.AutoDjSpace
import com.autodj.app.design.AutoDjType
import com.autodj.app.design.Btn
import com.autodj.app.design.Tile
import com.autodj.app.design.Txt
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.remember
import androidx.compose.runtime.Stable
import androidx.compose.runtime.State
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.autodj.app.audio.GrainVizSnapshot
import com.autodj.app.dj.GranularPattern
import com.autodj.app.audio.SourceInfo
import com.autodj.app.audio.ParamSpec
import java.util.Locale
import kotlin.math.floor
import kotlin.math.roundToInt

/** What the instrument screen needs from the app. The ViewModel provides it ([asInstrumentHost]); tests provide a fake. */
interface InstrumentHost {
    val viz: GrainVizSnapshot
    var selectedVoice: Int
    fun spec(key: String): ParamSpec?
    fun paramState(spec: ParamSpec): State<Float>
    fun setParam(spec: ParamSpec, value: Float)
    fun noteOn(voice: Int, pitch: Float, velocity: Float)
    fun noteOff(voice: Int)
    fun resetSteps(voice: Int)
    fun peaksFor(slot: Int): FloatArray?
    fun sourceInfoFor(slot: Int): SourceInfo?

    /** Labels of the source chips in slot order: the four built-in sources first, then imported songs. */
    val sourceNames: List<String> get() = DemoSourceNames

    /** The beat builder (16-step pattern per voice); null where a platform does not offer it. */
    val pattern: GranularPattern? get() = null

    /** True when this platform can open a chooser for audio files ([loadFiles]). */
    val canLoadFiles: Boolean get() = false

    /** True when files can also be dropped on the window (desktop). */
    val canDropFiles: Boolean get() = false

    /** Opens the file chooser; the chosen audio files become sources of the selected voice. */
    fun loadFiles() {}

    /** Words for the screen while a file is being read, analysed and loaded; null when nothing is loading. */
    val loadStatus: String? get() = null

    /** True while a deck or the auto mix plays: the beat grid then runs on the beats of that music, not on the free tempo. */
    val followsMusic: Boolean get() = false
}

/** One registry parameter as the instrument sees it: read the value in composition, write through the host. */
@Stable
class Bound(private val host: InstrumentHost, val spec: ParamSpec) {
    val value: Float get() = host.paramState(spec).value
    val index: Int get() = value.roundToInt()
    val normalized: Float get() = spec.toNormalized(value)
    fun set(v: Float) = host.setParam(spec, v)
    fun setNormalized(n: Float) = host.setParam(spec, spec.fromNormalized(n))
    fun reset() = host.setParam(spec, spec.def)
}

/** Everything the instrument screen touches for one voice. Null if the native registry lacks a key (programming error). */
@Stable
class InstrumentParams private constructor(
    val mode: Bound, val cellCount: Bound, val cellIndex: Bound, val stepDiv: Bound,
    val position: Bound, val sizeMs: Bound, val density: Bound, val spread: Bound, val pitch: Bound,
    val envShape: Bound, val syncMode: Bound, val syncDiv: Bound, val gate: Bound, val source: Bound,
    val chaos: Bound, val output: Bound, val seqOn: Bound, val sprayMs: Bound, val jitter: Bound,
) {
    companion object {
        fun of(host: InstrumentHost, voice: Int): InstrumentParams? {
            fun b(short: String) = host.spec("granular.v$voice.$short")?.let { Bound(host, it) }
            fun g(key: String) = host.spec(key)?.let { Bound(host, it) }
            return InstrumentParams(
                b("cell_mode") ?: return null, b("cell_count") ?: return null, b("cell_index") ?: return null,
                b("step_div") ?: return null, b("position") ?: return null, b("size_ms") ?: return null,
                b("density") ?: return null, b("spread") ?: return null, b("pitch_st") ?: return null,
                b("env_shape") ?: return null, b("sync_mode") ?: return null, b("sync_div") ?: return null,
                b("gate") ?: return null, b("source") ?: return null,
                g("granular.chaos") ?: return null, g("granular.output_db") ?: return null,
                b("seq_on") ?: return null, b("spray_ms") ?: return null, b("jitter") ?: return null,
            )
        }
    }
}

private const val MODE_HOLD = 0
private const val MODE_STEP = 1
private const val MODE_KEYS = 2

/** Knob order of the sync divisions, slow -> fast: 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32 (as indices of the native enum). */
private val SyncKnobOrder = intArrayOf(0, 4, 1, 5, 2, 6, 3)
val DemoSourceNames = listOf("pad", "drums", "pluck", "bass")

/**
 * The state the instrument starts in (both apps): voice 1 on the pad, a cloud that sounds full and clean, and every voice
 * 6 dB louder than the engine default, so that the instrument sits at the level of the decks and the pads.
 */
fun applyInstrumentDefaults(set: (key: String, value: Float) -> Unit) {
    for (v in 0..3) {
        set("granular.v$v.volume", 1.6f)
        set("granular.v$v.seq_on", 1f)  // in step mode the beat grid decides what plays (its first pattern is the plain walk)
    }
    set("granular.v0.source", 0f)
    set("granular.v0.gate", 1f)
    set("granular.v0.density", 40f)
    set("granular.v0.size_ms", 160f)
    set("granular.v0.spray_ms", 300f)
    set("granular.v0.spread", 0.8f)
}

fun formatPercent(v: Float) = "${(v * 100f).roundToInt()} %"
fun formatDensity(v: Float) = if (v >= 10f) String.format(Locale.US, "%.0f /s", v) else String.format(Locale.US, "%.1f /s", v)
fun formatSemitones(v: Float) = String.format(Locale.US, "%+d st", v.roundToInt())
fun formatDb(v: Float) = String.format(Locale.US, "%+.1f dB", v)

/**
 * The granular instrument on the board of the design language: the dark display (touch = play) next to the control tile (mode,
 * cells, play) and the envelope tile (shape, voice, source); in step mode the beat builder; at the bottom one tile with the knobs:
 * space, chaos | length, GRAINS, position | tune, output. The line at the very bottom explains whatever was touched last.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun InstrumentScreen(host: InstrumentHost, modifier: Modifier = Modifier, onShowGuide: (() -> Unit)? = null, beatRequest: Int = 0) {
    val voice = host.selectedVoice
    val p = InstrumentParams.of(host, voice)
    if (p == null) {
        Txt("Parameter registry does not match the instrument layout.", modifier.padding(16.dp))
        return
    }
    val help = remember { HelpState(InstrumentHelp.Start) }
    val n = if (p.cellCount.index == 1) 16 else 8
    val mode = p.mode.index
    val slot = p.source.index
    val userSource = slot >= DemoSourceNames.size
    val peaks = host.peaksFor(slot)
    val info = host.sourceInfoFor(slot)
    val envNames = p.envShape.spec.enumNames
    // the guide's "Beat-Start" button: set up and start a beat once per request
    LaunchedEffect(beatRequest) {
        if (beatRequest > 0 && host.pattern != null) startBeat(host, p, voice, userSource)
    }

    fun start() = host.noteOn(voice, 0f, 1f)
    fun stop() = host.noteOff(voice)
    fun touch(x: Float, first: Boolean) {
        when (mode) {
            MODE_HOLD -> p.position.set(x)
            MODE_KEYS -> p.cellIndex.set(floor(x * n).toInt().coerceIn(0, n - 1).toFloat())
        }
        if (first) start()
    }
    fun pad(i: Int) {
        when (mode) {
            MODE_HOLD -> p.position.set((i + 0.5f) / n)
            MODE_KEYS -> p.cellIndex.set(i.toFloat())
        }
        start()
    }

    val actions: @Composable () -> Unit = {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            if (host.canLoadFiles) Btn("Datei laden", onClick = { host.loadFiles() }, small = true, modifier = Modifier.helpText(InstrumentHelp.Load))
            val status = host.loadStatus
            Txt(
                when {
                    status != null -> status
                    !userSource && host.canLoadFiles -> if (host.canDropFiles) "Eingebauter Klang. Eigene Musik: Datei laden oder ins Fenster ziehen." else "Eingebauter Klang. Eigene Musik: Datei laden."
                    else -> ""
                },
                style = AutoDjType.small, second = true, maxLines = 1, modifier = Modifier.weight(1f),
            )
            if (onShowGuide != null) Btn("Anleitung", onClick = onShowGuide, small = true)
        }
    }
    val screen: @Composable (Modifier) -> Unit = { m ->
        WaveformScreen(
            peaks = peaks, info = info, viz = host.viz, voice = voice, cells = n,
            onTouch = ::touch, onRelease = ::stop, modifier = m.helpText(InstrumentHelp.Screen),
        )
    }
    val controls: @Composable (Boolean) -> Unit = { wide ->
        Tile(Modifier.fillMaxWidth()) {
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp), itemVerticalAlignment = Alignment.CenterVertically) {
                Segmented(
                    listOf("hold", "step", "keys"), mode, { p.mode.set(it.toFloat()) },
                    help = "${InstrumentHelp.Hold}  ·  ${InstrumentHelp.Step}  ·  ${InstrumentHelp.Keys}",
                )
                Segmented(listOf("8", "16"), p.cellCount.index, { p.cellCount.set(it.toFloat()) }, help = InstrumentHelp.Cells)
                if (mode == MODE_STEP) OutlinedPill("reset", onClick = { host.resetSteps(voice) }, small = true)
                val on = p.gate.value >= 0.5f
                if (!(mode == MODE_STEP && host.pattern != null)) Btn(if (on) "Stopp" else "Abspielen", onClick = { p.gate.set(if (on) 0f else 1f) }, on = on, primary = !on, small = true, modifier = Modifier.helpText(InstrumentHelp.Play))
            }
            Txt(
                when (mode) {
                    MODE_HOLD -> "hold: eine Stelle der Datei festhalten. Ziehe auf der Welle."
                    MODE_STEP -> "step: Datei in Stücke schneiden und im Takt abspielen. Das Muster baust du im Raster."
                    else -> "keys: jede Taste spielt ein Stück der Datei."
                },
                style = AutoDjType.small, second = true, modifier = Modifier.padding(top = 10.dp, bottom = 10.dp),
            )
            val perRow = if (wide) 8 else 4
            Column(verticalArrangement = Arrangement.spacedBy(4.dp), modifier = Modifier.helpText(if (mode == MODE_KEYS) InstrumentHelp.Keys else InstrumentHelp.Cells)) {
                for (row in 0 until n / perRow) {
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        for (c in 0 until perRow) {
                            val i = row * perRow + c
                            PadKey(
                                cellNoteName(i), active = host.viz.cell[voice.coerceIn(0, 3)] == i,
                                onPress = { pad(i) }, onRelease = ::stop, modifier = Modifier.weight(1f),
                            )
                        }
                    }
                }
            }
        }
    }
    val envelope: @Composable () -> Unit = {
        EnvelopeSelector(envNames, p.envShape.index, { p.envShape.set(it.toFloat()) }, Modifier.fillMaxWidth(), help = InstrumentHelp.Envelope) {
            FlowRow(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp), itemVerticalAlignment = Alignment.CenterVertically) {
                Segmented(listOf("1", "2", "3", "4"), voice, { host.selectedVoice = it }, help = InstrumentHelp.Voice)
                val names = host.sourceNames
                val builtIn = DemoSourceNames.size
                Segmented(DemoSourceNames, if (slot < builtIn) slot else -1, { p.source.set(it.toFloat()) }, help = InstrumentHelp.Source)
                for (i in builtIn until names.size) {
                    val label = names[i].let { if (it.length > 14) it.take(13) + "…" else it }
                    OutlinedPill(label, onClick = { p.source.set(i.toFloat()) }, selected = slot == i, small = true, help = InstrumentHelp.Source)
                }
            }
        }
    }
    val beat: @Composable () -> Unit = {
        if (mode == MODE_STEP && host.pattern != null) BeatPanel(host, p, voice, n, userSource)
    }

    CompositionLocalProvider(LocalHelp provides help) {
        Column(modifier.fillMaxSize()) {
            BoxWithConstraints(Modifier.weight(1f).fillMaxWidth()) {
                val wide = maxWidth >= 840.dp
                val small = if (wide) 72.dp else 64.dp
                val big = if (wide) 92.dp else 88.dp
                val margin = if (wide) AutoDjSpace.margin else AutoDjSpace.phoneMargin
                val gap = if (wide) AutoDjSpace.gap else AutoDjSpace.phoneGap
                Column(
                    Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(horizontal = margin),
                    verticalArrangement = Arrangement.spacedBy(gap),
                ) {
                    actions()
                    if (wide) {
                        Row(Modifier.fillMaxWidth().height(IntrinsicSize.Min), horizontalArrangement = Arrangement.spacedBy(gap)) {
                            Box(Modifier.weight(7f).fillMaxHeight().heightIn(min = 300.dp)) { screen(Modifier.fillMaxSize()) }
                            Column(Modifier.weight(5f), verticalArrangement = Arrangement.spacedBy(gap)) {
                                controls(true)
                                envelope()
                            }
                        }
                        beat()
                        Tile(Modifier.fillMaxWidth(), padding = 20.dp) {
                            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly, verticalAlignment = Alignment.CenterVertically) {
                                SpaceKnob(p, small)
                                ChaosKnob(p, small)
                                LengthKnob(p, small)
                                Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(10.dp)) {
                                    SyncToggle(p)
                                    GrainsKnob(p, big)
                                }
                                PositionKnob(p, small)
                                TuneKnob(p, small)
                                OutputKnob(p, small)
                            }
                        }
                    } else {
                        screen(Modifier.aspectRatio(1.7f))
                        controls(false)
                        beat()
                        envelope()
                        Tile(Modifier.fillMaxWidth()) {
                            Column(Modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(10.dp)) {
                                SyncToggle(p)
                                GrainsKnob(p, big)
                            }
                            Spacer(Modifier.height(16.dp))
                            SpacerRow { LengthKnob(p, small); PositionKnob(p, small); TuneKnob(p, small) }
                            Spacer(Modifier.height(16.dp))
                            SpacerRow { SpaceKnob(p, small); ChaosKnob(p, small); OutputKnob(p, small) }
                        }
                    }
                    Spacer(Modifier.height(4.dp))
                }
            }
            HelpStrip(help)
        }
    }
}

@Composable
private fun SyncToggle(p: InstrumentParams) {
    Segmented(listOf("sync", "free"), if (p.syncMode.index == 1) 0 else 1, { p.syncMode.set(if (it == 0) 1f else 0f) }, help = InstrumentHelp.Sync)
}

@Composable
private fun GrainsKnob(p: InstrumentParams, size: Dp) {
    val synced = p.syncMode.index == 1
    if (synced) {
        val names = p.syncDiv.spec.enumNames
        val order = SyncKnobOrder.filter { it < names.size }
        val pos = order.indexOf(p.syncDiv.index.coerceIn(0, names.size - 1)).coerceAtLeast(0)
        Knob(
            "grains", names[order[pos]], pos / (order.size - 1).toFloat(),
            { v -> p.syncDiv.set(order[(v * (order.size - 1)).roundToInt()].toFloat()) },
            size = size, steps = order.size - 1, onReset = { p.syncDiv.reset() }, help = InstrumentHelp.Grains,
        )
    } else {
        Knob(
            "grains", formatDensity(p.density.value), p.density.normalized, { p.density.setNormalized(it) },
            size = size, onReset = { p.density.reset() }, help = InstrumentHelp.Grains,
        )
    }
}

@Composable
private fun LengthKnob(p: InstrumentParams, size: Dp) = Knob(
    "length", "${p.sizeMs.value.roundToInt()} ms", p.sizeMs.normalized, { p.sizeMs.setNormalized(it) },
    size = size, onReset = { p.sizeMs.reset() }, help = InstrumentHelp.Length,
)

@Composable
private fun PositionKnob(p: InstrumentParams, size: Dp) = Knob(
    "position", formatPercent(p.position.value), p.position.normalized, { p.position.setNormalized(it) },
    size = size, onReset = { p.position.reset() }, help = InstrumentHelp.Position,
)

@Composable
private fun TuneKnob(p: InstrumentParams, size: Dp) = Knob(
    "tune", formatSemitones(p.pitch.value), p.pitch.normalized, { p.pitch.setNormalized(it) },
    size = size, steps = (p.pitch.spec.max - p.pitch.spec.min).roundToInt(), onReset = { p.pitch.reset() }, help = InstrumentHelp.Tune,
)

@Composable
private fun SpaceKnob(p: InstrumentParams, size: Dp) = Knob(
    "space", formatPercent(p.spread.value), p.spread.normalized, { p.spread.setNormalized(it) },
    size = size, onReset = { p.spread.reset() }, help = InstrumentHelp.Space,
)

@Composable
private fun ChaosKnob(p: InstrumentParams, size: Dp) = Knob(
    "chaos", formatPercent(p.chaos.value), p.chaos.normalized, { p.chaos.setNormalized(it) },
    size = size, onReset = { p.chaos.reset() }, help = InstrumentHelp.Chaos,
)

@Composable
private fun OutputKnob(p: InstrumentParams, size: Dp) = Knob(
    "output", formatDb(p.output.value), p.output.normalized, { p.output.setNormalized(it) },
    size = size, onReset = { p.output.reset() }, help = InstrumentHelp.Output,
)
