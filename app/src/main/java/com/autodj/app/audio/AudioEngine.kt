package com.autodj.app.audio

import org.json.JSONObject

/**
 * Kotlin handle to one native Engine + Oboe driver. Not thread-safe by design: call it from the main thread
 * (the ViewModel does); the audio thread is entirely native.
 */
class AudioEngine(val sampleRate: Int, voices: Int = 4) : AutoCloseable {
    private var handle: Long = NativeEngine.nativeCreate(sampleRate, voices)
    private val statusBuf = DoubleArray(EngineStatus.SIZE)
    private val vizBuf = FloatArray(GrainVizSnapshot.SIZE)

    val specs: List<ParamSpec> = ParamSpec.parseAll(NativeEngine.nativeParamSpecs(handle))
    private val byKey: Map<String, ParamSpec> = specs.associateBy { it.key }

    fun spec(key: String): ParamSpec? = byKey[key]

    /** Oboe result code (0 = OK). */
    fun start(exclusive: Boolean = true): Int = NativeEngine.nativeStart(handle, exclusive, 0)
    fun stop() = NativeEngine.nativeStop(handle)
    fun driverInfo(): DriverInfo = DriverInfo.fromJson(NativeEngine.nativeDriverInfo(handle))

    fun setParam(id: Int, value: Float) = NativeEngine.nativeSetParam(handle, id, value)
    fun getParam(id: Int): Float = NativeEngine.nativeGetParam(handle, id)
    fun setByKey(key: String, value: Float) {
        byKey[key]?.let { setParam(it.id, value) }
    }

    fun exportPreset(name: String): String = NativeEngine.nativeExportPreset(handle, name)

    /** Returns a one-line human readable summary of the import. */
    fun importPreset(json: String): String {
        val r = JSONObject(NativeEngine.nativeImportPreset(handle, json))
        return if (r.optBoolean("ok")) {
            "Loaded ${r.optInt("applied")} parameters (preset v${r.optInt("fromVersion")}), " +
                "${r.optJSONArray("unknownKeys")?.length() ?: 0} unknown, ${r.optJSONArray("clampedKeys")?.length() ?: 0} clamped"
        } else {
            "Import failed: ${r.optString("error")}"
        }
    }

    fun play() = NativeEngine.nativePlay(handle)
    fun stopTransport() = NativeEngine.nativeStopTransport(handle)
    fun seek(beats: Double) = NativeEngine.nativeSeek(handle, beats)
    fun noteOn(voice: Int, pitchSemitones: Float = 0f, velocity: Float = 1f) =
        NativeEngine.nativeNoteOn(handle, voice, pitchSemitones, velocity)
    fun noteOff(voice: Int) = NativeEngine.nativeNoteOff(handle, voice)
    fun resetEngine() = NativeEngine.nativeResetEngine(handle)
    /** Restarts the Step-mode sequence of a voice at cell 0. */
    fun resetSteps(voice: Int) = NativeEngine.nativeResetSteps(handle, voice)
    fun collectGarbage(): Int = NativeEngine.nativeCollectGarbage(handle)
    fun loadDemoSource(slot: Int, kind: Int): Boolean = NativeEngine.nativeLoadDemoSource(handle, slot, kind)

    /** Waveform overview of a source slot: [min0, max0, min1, max1, ...] for [columns] columns, or null if the slot is empty. */
    fun sourcePeaks(slot: Int, columns: Int): FloatArray? {
        val out = FloatArray(2 * columns)
        return if (NativeEngine.nativeSourcePeaks(handle, slot, columns, out)) out else null
    }

    fun sourceInfo(slot: Int): SourceInfo? = SourceInfo.fromJson(NativeEngine.nativeSourceInfo(handle, slot))

    fun pollStatus(): EngineStatus? =
        if (NativeEngine.nativePollStatus(handle, statusBuf)) EngineStatus.fromArray(statusBuf) else null

    fun pollViz(): GrainVizSnapshot? =
        if (NativeEngine.nativePollViz(handle, vizBuf)) GrainVizSnapshot.fromArray(vizBuf) else null

    override fun close() {
        if (handle != 0L) {
            NativeEngine.nativeDestroy(handle)
            handle = 0L
        }
    }

    companion object {
        /** Runs the native granular benchmark on the calling thread (call it from a background dispatcher). */
        fun runBenchmark(quick: Boolean): BenchmarkResult {
            val o = JSONObject(NativeEngine.nativeRunBenchmark(quick))
            return BenchmarkResult(o.getString("markdown"), o.getJSONObject("json").toString(2))
        }

        fun benchmarkProgress(): Int = NativeEngine.nativeBenchmarkProgress()
    }
}

data class BenchmarkResult(val markdown: String, val json: String)
