package com.autodj.app.audio

import org.json.JSONObject

/** Snapshot published by the audio thread once per callback (native struct EngineStatus). */
data class EngineStatus(
    val load: Float = 0f,
    val loadPeak: Float = 0f,
    val lastBlockLoad: Float = 0f,
    val peakL: Float = 0f,
    val peakR: Float = 0f,
    val deadlineMisses: Long = 0,
    val xruns: Long = 0,
    val droppedGrains: Long = 0,
    val callbacks: Long = 0,
    val clockBeat: Double = 0.0,
    val playBeat: Double = 0.0,
    val bpm: Double = 120.0,
    val playing: Boolean = false,
    val activeGrains: Int = 0,
    val activeVoices: Int = 0,
    val qualityLevel: Int = 0,
    val callbackFrames: Int = 0,
    val sampleRate: Int = 48000,
) {
    companion object {
        /** Length of the array filled by nativePollStatus. */
        const val SIZE = 18

        fun fromArray(a: DoubleArray) = EngineStatus(
            load = a[0].toFloat(), loadPeak = a[1].toFloat(), lastBlockLoad = a[2].toFloat(),
            peakL = a[3].toFloat(), peakR = a[4].toFloat(),
            deadlineMisses = a[5].toLong(), xruns = a[6].toLong(), droppedGrains = a[7].toLong(),
            callbacks = a[8].toLong(), clockBeat = a[9], playBeat = a[10], bpm = a[11],
            playing = a[12] != 0.0, activeGrains = a[13].toInt(), activeVoices = a[14].toInt(),
            qualityLevel = a[15].toInt(), callbackFrames = a[16].toInt(), sampleRate = a[17].toInt(),
        )
    }
}

class GrainVizItem(val pos: Float, val size: Float, val pan: Float, val age: Float, val rate: Float, val voice: Int)

/** Grain cloud snapshot for the visualisation (native struct GrainViz, flattened by nativePollViz). */
class GrainVizSnapshot(
    val voices: Int,
    val playhead: FloatArray,
    val voiceLevel: FloatArray,
    /** Cell each voice currently reads from (Step / Keys mode), -1 in Hold mode. */
    val cell: IntArray,
    val grains: List<GrainVizItem>,
    /** Step 0..15 of each voice's pattern, -1 when the voice runs no pattern. */
    val step: IntArray = IntArray(MAX_VOICES) { -1 },
) {
    companion object {
        const val MAX_VOICES = 4
        const val MAX_GRAINS = 256
        const val HEAD = 2 + 4 * MAX_VOICES
        const val SIZE = HEAD + 6 * MAX_GRAINS
        val EMPTY = GrainVizSnapshot(4, FloatArray(MAX_VOICES), FloatArray(MAX_VOICES), IntArray(MAX_VOICES) { -1 }, emptyList())

        fun fromArray(a: FloatArray): GrainVizSnapshot {
            val count = a[0].toInt().coerceIn(0, MAX_GRAINS)
            val items = List(count) { i ->
                val b = HEAD + 6 * i
                GrainVizItem(a[b], a[b + 1], a[b + 2], a[b + 3], a[b + 4], a[b + 5].toInt())
            }
            return GrainVizSnapshot(
                voices = a[1].toInt(),
                playhead = FloatArray(MAX_VOICES) { a[2 + it] },
                voiceLevel = FloatArray(MAX_VOICES) { a[2 + MAX_VOICES + it] },
                cell = IntArray(MAX_VOICES) { a[2 + 2 * MAX_VOICES + it].toInt() },
                grains = items,
                step = IntArray(MAX_VOICES) { a[2 + 3 * MAX_VOICES + it].toInt() },
            )
        }
    }
}

/** What the Oboe stream actually negotiated (native struct DriverInfo as JSON). */
data class DriverInfo(
    val running: Boolean = false,
    val api: String = "none",
    val sharing: String = "-",
    val performance: String = "-",
    val sampleRate: Int = 0,
    val framesPerBurst: Int = 0,
    val framesPerCallback: Int = 0,
    val bufferSize: Int = 0,
    val bufferCapacity: Int = 0,
    val latencyMs: Double = -1.0,
    val xruns: Int = 0,
    val restarts: Int = 0,
    val performanceHint: Boolean = false,
    val lastError: String = "",
    val simd: String = "",
    val rtGuard: Boolean = false,
    val rtViolations: Long = 0,
) {
    companion object {
        fun fromJson(json: String): DriverInfo {
            val o = JSONObject(json)
            return DriverInfo(
                running = o.optBoolean("running"),
                api = o.optString("api", "none"),
                sharing = o.optString("sharing", "-"),
                performance = o.optString("performance", "-"),
                sampleRate = o.optInt("sampleRate"),
                framesPerBurst = o.optInt("framesPerBurst"),
                framesPerCallback = o.optInt("framesPerCallback"),
                bufferSize = o.optInt("bufferSize"),
                bufferCapacity = o.optInt("bufferCapacity"),
                latencyMs = o.optDouble("latencyMs", -1.0),
                xruns = o.optInt("xruns"),
                restarts = o.optInt("restarts"),
                performanceHint = o.optBoolean("performanceHint"),
                lastError = o.optString("lastError", ""),
                simd = o.optString("simd", ""),
                rtGuard = o.optBoolean("rtGuard"),
                rtViolations = o.optLong("rtViolations"),
            )
        }
    }
}

/** What a source slot holds (native Engine::sourceInfo as JSON). */
data class SourceInfo(val name: String, val seconds: Double, val sampleRate: Int) {
    companion object {
        /** null for the empty string the native side returns for an empty slot. */
        fun fromJson(json: String): SourceInfo? {
            if (json.isEmpty()) return null
            val o = JSONObject(json)
            return SourceInfo(o.optString("name", ""), o.optDouble("seconds", 0.0), o.optInt("sampleRate"))
        }
    }
}
