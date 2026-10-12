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
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.Stable
import androidx.compose.runtime.State
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.autodj.app.AutoDjViewModel
import com.autodj.app.audio.GrainVizSnapshot
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
    val chaos: Bound, val output: Bound,
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
            )
        }
    }
}

private const val MODE_HOLD = 0
private const val MODE_STEP = 1
private const val MODE_KEYS = 2

/** Knob order of the sync divisions, slow -> fast: 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32 (as indices of the native enum). */
private val SyncKnobOrder = intArrayOf(0, 4, 1, 5, 2, 6, 3)
private val DemoSourceNames = listOf("pad", "drums", "noise", "sine")

fun formatPercent(v: Float) = "${(v * 100f).roundToInt()} %"
fun formatDensity(v: Float) = if (v >= 10f) String.format(Locale.US, "%.0f /s", v) else String.format(Locale.US, "%.1f /s", v)
fun formatSemitones(v: Float) = String.format(Locale.US, "%+d st", v.roundToInt())
fun formatDb(v: Float) = String.format(Locale.US, "%+.1f dB", v)

/**
 * The granular instrument: dark screen (touch = play), mode row (hold / step / keys), cell pads, grain envelope and the
 * nine controls: space, chaos | length, GRAINS, position | tune, output (+ sync/free above the big knob).
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun InstrumentScreen(host: InstrumentHost, modifier: Modifier = Modifier) {
    val voice = host.selectedVoice
    val p = InstrumentParams.of(host, voice)
    if (p == null) {
        Text("Parameter registry does not match the instrument layout.", modifier.padding(16.dp))
        return
    }
    val n = if (p.cellCount.index == 1) 16 else 8
    val mode = p.mode.index
    val slot = p.source.index
    val peaks = host.peaksFor(slot)
    val info = host.sourceInfoFor(slot)
    val envNames = p.envShape.spec.enumNames

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

    val screen: @Composable (Modifier) -> Unit = { m ->
        WaveformScreen(
            peaks = peaks, info = info, viz = host.viz, voice = voice, cells = n,
            onTouch = ::touch, onRelease = ::stop, modifier = m,
        )
    }
    val modeRow: @Composable () -> Unit = {
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Segmented(listOf("hold", "step", "keys"), mode, { p.mode.set(it.toFloat()) })
            Segmented(listOf("8", "16"), p.cellCount.index, { p.cellCount.set(it.toFloat()) })
            if (mode == MODE_STEP) {
                val names = p.stepDiv.spec.enumNames
                val cur = p.stepDiv.index.coerceIn(0, names.size - 1)
                Stepper(
                    names[cur],
                    onPrev = { p.stepDiv.set(((cur + names.size - 1) % names.size).toFloat()) },
                    onNext = { p.stepDiv.set(((cur + 1) % names.size).toFloat()) },
                )
                OutlinedPill("reset", onClick = { host.resetSteps(voice) })
            }
        }
    }
    val pads: @Composable () -> Unit = {
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            for (row in 0 until n / 8) {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    for (c in 0 until 8) {
                        val i = row * 8 + c
                        PadKey(
                            cellNoteName(i), active = host.viz.cell[voice.coerceIn(0, 3)] == i,
                            onPress = { pad(i) }, onRelease = ::stop, modifier = Modifier.weight(1f),
                        )
                    }
                }
            }
        }
    }
    val envelope: @Composable () -> Unit = {
        EnvelopeSelector(envNames, p.envShape.index, { p.envShape.set(it.toFloat()) }, Modifier.fillMaxWidth())
    }
    val sources: @Composable () -> Unit = {
        FlowRow(
            horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp),
            itemVerticalAlignment = Alignment.CenterVertically,
        ) {
            Segmented(listOf("1", "2", "3", "4"), voice, { host.selectedVoice = it })
            OutlinedPill("hold on", onClick = { p.gate.set(if (p.gate.value >= 0.5f) 0f else 1f) }, selected = p.gate.value >= 0.5f)
            Segmented(DemoSourceNames, slot.coerceIn(0, DemoSourceNames.size - 1), { p.source.set(it.toFloat()) })
        }
    }

    BoxWithConstraints(modifier.fillMaxSize()) {
        val wide = maxWidth >= 840.dp
        val small = if (wide) 76.dp else 72.dp
        val big = if (wide) 150.dp else 136.dp
        Column(
            Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(horizontal = 16.dp, vertical = 12.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp),
        ) {
            if (wide) {
                Row(horizontalArrangement = Arrangement.spacedBy(20.dp)) {
                    Column(Modifier.weight(1.25f)) { screen(Modifier.aspectRatio(1.75f)) }
                    Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                        modeRow()
                        pads()
                        envelope()
                        sources()
                    }
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly, verticalAlignment = Alignment.Bottom) {
                    Row(horizontalArrangement = Arrangement.spacedBy(24.dp), verticalAlignment = Alignment.Top) {
                        SpaceKnob(p, small)
                        ChaosKnob(p, small)
                    }
                    GroupDivider()
                    Row(horizontalArrangement = Arrangement.spacedBy(24.dp), verticalAlignment = Alignment.Bottom) {
                        LengthKnob(p, small)
                        Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(10.dp)) {
                            SyncToggle(p)
                            GrainsKnob(p, big)
                        }
                        PositionKnob(p, small)
                    }
                    GroupDivider()
                    Row(horizontalArrangement = Arrangement.spacedBy(24.dp), verticalAlignment = Alignment.Top) {
                        TuneKnob(p, small)
                        OutputKnob(p, small)
                    }
                }
            } else {
                screen(Modifier.aspectRatio(1.8f))
                modeRow()
                pads()
                envelope()
                sources()
                Column(Modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    SyncToggle(p)
                    GrainsKnob(p, big)
                }
                SpacerRow { LengthKnob(p, small); PositionKnob(p, small); TuneKnob(p, small) }
                SpacerRow { SpaceKnob(p, small); ChaosKnob(p, small); OutputKnob(p, small) }
            }
            Spacer(Modifier.height(16.dp))
        }
    }
}

@Composable
private fun SyncToggle(p: InstrumentParams) {
    Segmented(listOf("sync", "free"), if (p.syncMode.index == 1) 0 else 1, { p.syncMode.set(if (it == 0) 1f else 0f) })
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
            size = size, steps = order.size - 1, onReset = { p.syncDiv.reset() },
        )
    } else {
        Knob(
            "grains", formatDensity(p.density.value), p.density.normalized, { p.density.setNormalized(it) },
            size = size, onReset = { p.density.reset() },
        )
    }
}

@Composable
private fun LengthKnob(p: InstrumentParams, size: Dp) = Knob(
    "length", "${p.sizeMs.value.roundToInt()} ms", p.sizeMs.normalized, { p.sizeMs.setNormalized(it) },
    size = size, onReset = { p.sizeMs.reset() },
)

@Composable
private fun PositionKnob(p: InstrumentParams, size: Dp) = Knob(
    "position", formatPercent(p.position.value), p.position.normalized, { p.position.setNormalized(it) },
    size = size, onReset = { p.position.reset() },
)

@Composable
private fun TuneKnob(p: InstrumentParams, size: Dp) = Knob(
    "tune", formatSemitones(p.pitch.value), p.pitch.normalized, { p.pitch.setNormalized(it) },
    size = size, steps = (p.pitch.spec.max - p.pitch.spec.min).roundToInt(), onReset = { p.pitch.reset() },
)

@Composable
private fun SpaceKnob(p: InstrumentParams, size: Dp) = Knob(
    "space", formatPercent(p.spread.value), p.spread.normalized, { p.spread.setNormalized(it) },
    size = size, onReset = { p.spread.reset() },
)

@Composable
private fun ChaosKnob(p: InstrumentParams, size: Dp) = Knob(
    "chaos", formatPercent(p.chaos.value), p.chaos.normalized, { p.chaos.setNormalized(it) },
    size = size, onReset = { p.chaos.reset() },
)

@Composable
private fun OutputKnob(p: InstrumentParams, size: Dp) = Knob(
    "output", formatDb(p.output.value), p.output.normalized, { p.output.setNormalized(it) },
    size = size, onReset = { p.output.reset() },
)

/** Adapter between the ViewModel (engine, polled state) and the instrument screen. */
fun AutoDjViewModel.asInstrumentHost(): InstrumentHost {
    val vm = this
    return object : InstrumentHost {
        override val viz get() = vm.viz
        override var selectedVoice: Int
            get() = vm.selectedVoice
            set(value) { vm.selectedVoice = value }
        override fun spec(key: String) = vm.engine.spec(key)
        override fun paramState(spec: ParamSpec): State<Float> = vm.paramState(spec)
        override fun setParam(spec: ParamSpec, value: Float) = vm.setParam(spec, value)
        override fun noteOn(voice: Int, pitch: Float, velocity: Float) = vm.noteOn(voice, pitch, velocity)
        override fun noteOff(voice: Int) = vm.noteOff(voice)
        override fun resetSteps(voice: Int) = vm.resetSteps(voice)
        override fun peaksFor(slot: Int) = vm.peaksFor(slot, vm.peaksVersion)
        override fun sourceInfoFor(slot: Int) = vm.sourceInfoFor(slot, vm.peaksVersion)
    }
}
