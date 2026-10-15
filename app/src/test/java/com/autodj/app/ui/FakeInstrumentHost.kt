package com.autodj.app.ui

import androidx.compose.runtime.State
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import com.autodj.app.audio.GrainVizItem
import com.autodj.app.audio.GrainVizSnapshot
import com.autodj.app.audio.ParamSpec
import com.autodj.app.audio.SourceInfo
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.sin
import kotlin.random.Random

/** In-memory stand-in for the engine: real registry (snapshot), real defaults, synthetic waveform and grain cloud. */
class FakeInstrumentHost(
    overrides: Map<String, Float> = emptyMap(),
    cell: Int = -1,
    voice: Int = 0,
) : InstrumentHost {
    private val specs: List<ParamSpec> = ParamSpec.parseAll(
        checkNotNull(FakeInstrumentHost::class.java.getResourceAsStream("/param_specs.json")).bufferedReader().readText(),
    )
    private val states = specs.map { mutableFloatStateOf(it.def) }
    private var voiceState by mutableIntStateOf(voice)
    val log = mutableListOf<String>()

    init {
        for ((k, v) in overrides) value(k, v)
    }

    fun value(key: String): Float = states[specs.first { it.key == key }.id].floatValue
    fun value(key: String, v: Float) { states[specs.first { it.key == key }.id].floatValue = v }

    override val viz: GrainVizSnapshot = run {
        val rnd = Random(5)
        val grains = List(70) {
            val v = if (it % 5 == 0) 1 else 0
            GrainVizItem(
                pos = (0.28f + 0.12f * rnd.nextFloat()).coerceIn(0f, 1f) + if (v == 1) 0.35f else 0f,
                size = 0.1f + 0.4f * rnd.nextFloat(), pan = rnd.nextFloat() * 2f - 1f,
                age = rnd.nextFloat(), rate = 1f, voice = v,
            )
        }
        GrainVizSnapshot(
            voices = 4,
            playhead = floatArrayOf(0.34f, 0.72f, 0f, 0f),
            voiceLevel = floatArrayOf(0.7f, 0.5f, 0f, 0f),
            cell = intArrayOf(cell, -1, -1, -1),
            grains = grains,
        )
    }

    override var selectedVoice: Int
        get() = voiceState
        set(value) { voiceState = value; log += "voice=$value" }

    override fun spec(key: String) = specs.firstOrNull { it.key == key }
    override fun paramState(spec: ParamSpec): State<Float> = states[spec.id]
    override fun setParam(spec: ParamSpec, value: Float) {
        states[spec.id].floatValue = value.coerceIn(spec.min, spec.max)
        log += "${spec.key}=${states[spec.id].floatValue}"
    }
    override fun noteOn(voice: Int, pitch: Float, velocity: Float) { log += "noteOn($voice)" }
    override fun noteOff(voice: Int) { log += "noteOff($voice)" }
    override fun resetSteps(voice: Int) { log += "resetSteps($voice)" }

    override fun peaksFor(slot: Int): FloatArray {
        val rnd = Random(slot + 1)
        val cols = 512
        return FloatArray(2 * cols) { i ->
            val c = i / 2
            val env = 0.35f + 0.65f * abs(sin(c * 2f * PI.toFloat() / 150f)) * (0.6f + 0.4f * rnd.nextFloat())
            if (i % 2 == 0) -env else env
        }
    }

    override fun sourceInfoFor(slot: Int) = SourceInfo("chord-pad", 12.0, 48000)

    /** What the audio thread would have received: voice, step mask, cells. */
    val sentPatterns = mutableListOf<Triple<Int, Int, Long>>()
    override val pattern = com.autodj.app.dj.GranularPattern { v, m, c -> sentPatterns += Triple(v, m, c) }
    override val canLoadFiles = true
    override val canDropFiles = true
    override var loadStatus: String? by mutableStateOf(null)
    override fun loadFiles() { log += "loadFiles" }
}
