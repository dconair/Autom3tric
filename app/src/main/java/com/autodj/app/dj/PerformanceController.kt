package com.autodj.app.dj

import androidx.compose.runtime.State
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.autodj.app.audio.AudioEngine

const val PAD_COUNT = 16
const val STEP_COUNT = 16

/** Names of the sixteen pads, row by row (top left first). The sounds are synthesised by the native engine. */
val PadNames = listOf(
    "Kick", "Sub-Kick", "Snare", "Clap",
    "Hi-Hat", "Open Hat", "Rim", "Shaker",
    "Tom tief", "Tom hoch", "Cowbell", "Conga",
    "Crash", "Riser", "Laser", "Drop",
)

/** Pad colours by kind: drums, hats, percussion, effects. */
fun padKind(pad: Int): Int = when (pad) {
    0, 1, 2, 3 -> 0
    4, 5, 7 -> 1
    6, 8, 9, 10, 11 -> 2
    else -> 3
}

/** A drum pattern: bit s of a pad's mask is step s (a 16th note); step 0 is the downbeat of the bar. */
class PadPattern(val name: String, val rows: IntArray) {
    init {
        require(rows.size == PAD_COUNT)
    }
}

private fun mask(vararg steps: Int): Int = steps.fold(0) { m, s -> m or (1 shl s) }

private fun pattern(name: String, vararg rows: Pair<Int, Int>): PadPattern {
    val m = IntArray(PAD_COUNT)
    for ((pad, mk) in rows) m[pad] = mk
    return PadPattern(name, m)
}

/** The patterns the sequencer offers. "House" is what the native sequencer starts with. */
val PadPatterns = listOf(
    pattern("House", 0 to 0x1111, 3 to 0x1010, 4 to 0x8888, 5 to 0x4444, 7 to 0xAAAA),
    pattern("Techno", 0 to 0x1111, 3 to 0x1010, 4 to 0xAAAA, 5 to 0x4444, 6 to mask(3, 6, 11, 14)),
    pattern("Hip-Hop", 0 to mask(0, 7, 10), 2 to mask(4, 12), 4 to mask(0, 2, 4, 6, 8, 10, 12, 14), 5 to mask(15)),
    pattern("Breakbeat", 0 to mask(0, 5, 10), 2 to mask(4, 12), 3 to mask(14), 4 to mask(0, 2, 4, 6, 8, 10, 12, 14), 11 to mask(7, 13)),
    pattern("Leer"),
)

/** The note lengths of the enum parameters, as the chips show them. */
val EchoTimes = listOf("1/8", "1/4", "1/2", "3/4", "1")
val RollSizes = listOf("1", "1/2", "1/4", "1/8")
val StutterRates = listOf("1/4", "1/8", "1/16")

/**
 * The performance section: master effects (echo, reverb, flanger, beat roll, stutter) and the drum pads with their 16-step
 * sequencer. The effects are plain engine parameters ("fx.*", "pads.*"); this class keeps one Compose state per parameter, holds
 * the pattern grid, and sends pad hits and pattern rows to the audio thread. Everything is tied to the beat grid of what plays.
 */
class PerformanceController(private val engine: AudioEngine) {
    private val states = HashMap<String, androidx.compose.runtime.MutableFloatState>()

    private fun stateOf(key: String) = states.getOrPut(key) { mutableFloatStateOf(engine.spec(key)?.let { engine.getParam(it.id) } ?: 0f) }

    /** Current value of parameter [key] (for example "fx.echo.on"). */
    fun param(key: String): State<Float> = stateOf(key)

    fun on(key: String): Boolean = stateOf(key).floatValue >= 0.5f

    fun set(key: String, value: Float) {
        val spec = engine.spec(key) ?: return
        val v = value.coerceIn(spec.min, spec.max)
        stateOf(key).floatValue = v
        engine.setParam(spec.id, v)
    }

    fun setOn(key: String, on: Boolean) = set(key, if (on) 1f else 0f)

    fun toggle(key: String) = setOn(key, !on(key))

    // ---- pads ---------------------------------------------------------------------------------------

    /** The sequencer grid: one 16-bit mask per pad. */
    val rows = mutableStateListOf<Int>().also { l -> repeat(PAD_COUNT) { l.add(PadPatterns[0].rows[it]) } }

    var patternName by mutableStateOf(PadPatterns[0].name)
        private set

    init {
        sendAllRows()
    }

    /** Plays a pad once; [velocity] 0..1. */
    fun hit(pad: Int, velocity: Float = 1f) {
        if (pad in 0 until PAD_COUNT) engine.padTrigger(pad, velocity.coerceIn(0.05f, 1f))
    }

    fun stepOn(pad: Int, step: Int): Boolean = (rows[pad] shr step) and 1 != 0

    fun toggleStep(pad: Int, step: Int) {
        val m = rows[pad] xor (1 shl step)
        rows[pad] = m
        patternName = "Eigenes Muster"
        engine.padRow(pad, m)
    }

    fun clearRow(pad: Int) {
        rows[pad] = 0
        patternName = "Eigenes Muster"
        engine.padRow(pad, 0)
    }

    fun loadPattern(p: PadPattern) {
        for (i in 0 until PAD_COUNT) rows[i] = p.rows[i]
        patternName = p.name
        sendAllRows()
    }

    private fun sendAllRows() {
        for (i in 0 until PAD_COUNT) engine.padRow(i, rows[i])
    }
}
