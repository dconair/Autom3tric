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

    // ---- tracks ---------------------------------------------------------------------------------
    // These calls are thread-safe on the native side and meant for a worker thread (decode/analysis takes seconds).

    /** Starts a decode job; returns a ticket (0 = refused). */
    fun ingestBegin(srcRate: Int, channels: Int): Int = NativeEngine.nativeIngestBegin(handle, srcRate, channels)
    fun ingestAppend(ticket: Int, data: FloatArray, count: Int): Boolean = NativeEngine.nativeIngestAppend(handle, ticket, data, count)
    /**
     * Converts, analyses (or adopts [cachedAnalysisJson]) and returns the result JSON. [keep] = false forgets the audio again
     * (library import), true leaves it registered as a track for decks / the granular instrument.
     */
    fun ingestFinish(ticket: Int, name: String, cachedAnalysisJson: String?, keep: Boolean, overviewColumns: Int = OVERVIEW_COLUMNS): String =
        NativeEngine.nativeIngestFinish(handle, ticket, name, cachedAnalysisJson ?: "", keep, overviewColumns)
    fun ingestCancel(ticket: Int) = NativeEngine.nativeIngestCancel(handle, ticket)
    fun ingestProgress(ticket: Int): Int = NativeEngine.nativeIngestProgress(handle, ticket)
    fun trackJson(trackId: Int, overviewColumns: Int = 0): String = NativeEngine.nativeTrackJson(handle, trackId, overviewColumns)
    /** Min/max waveform of [startSec, endSec) in [columns] columns (peak pyramid), or null for an unknown track. */
    fun trackPeaks(trackId: Int, startSec: Double, endSec: Double, columns: Int): FloatArray? {
        val out = FloatArray(2 * columns)
        return if (NativeEngine.nativeTrackPeaks(handle, trackId, startSec, endSec, columns, out)) out else null
    }
    fun trackRelease(trackId: Int) = NativeEngine.nativeTrackRelease(handle, trackId)
    fun trackToGranularSource(trackId: Int, slot: Int): Boolean = NativeEngine.nativeTrackToSource(handle, trackId, slot)
    fun addDemoTrack(kind: Int, keep: Boolean, overviewColumns: Int = OVERVIEW_COLUMNS): String =
        NativeEngine.nativeAddDemoTrack(handle, kind, keep, overviewColumns)
    /** Renders [frames] stereo frames without audio hardware (tests, export). Only while the audio output is stopped. */
    fun renderOffline(frames: Int, out: FloatArray): Int = NativeEngine.nativeRenderOffline(handle, frames, out)

    // ---- DJ: decks, automatic mix, offline export ---------------------------------------------------
    private val djBuf = DoubleArray(DjSnapshot.SIZE)

    /** A library entry without audio (analysis only), enough to plan a mix with. Returns the native track id, 0 on error. */
    fun trackStub(name: String, analysisJson: String): Int = NativeEngine.nativeTrackStub(handle, name, analysisJson)
    fun deckLoad(deck: Int, trackId: Int, startSec: Double = 0.0): Boolean = NativeEngine.nativeDeckLoad(handle, deck, trackId, startSec)
    fun deckUnload(deck: Int): Boolean = NativeEngine.nativeDeckLoad(handle, deck, 0, 0.0)
    fun deckPlay(deck: Int, play: Boolean): Boolean = NativeEngine.nativeDeckPlay(handle, deck, play)
    fun deckSeek(deck: Int, sec: Double): Boolean = NativeEngine.nativeDeckSeek(handle, deck, sec)

    /** Suggested order for [ids] (mode: 0 as listed, 1 smart, 2 tempo up, 3 tempo down): indices into [ids]. */
    fun orderTracks(ids: IntArray, mode: Int): IntArray = NativeEngine.nativeOrderTracks(handle, ids, mode)

    /** Plans a mix of the tracks in this order. type: 0 auto, 1 EQ blend, 2 crossfade, 3 filter sweep, 4 cut. Returns the result JSON. */
    fun planCreate(ids: IntArray, type: Int, lengthBeats: Int, seed: Int = 1): String = NativeEngine.nativePlanCreate(handle, ids, type, lengthBeats, seed)
    fun planBind(planId: Int, item: Int, trackId: Int): Boolean = NativeEngine.nativePlanBind(handle, planId, item, trackId)
    fun planBound(planId: Int, item: Int): Boolean = NativeEngine.nativePlanBound(handle, planId, item)
    fun planUnbind(planId: Int, item: Int) = NativeEngine.nativePlanUnbind(handle, planId, item)
    fun planStart(planId: Int): Boolean = NativeEngine.nativePlanStart(handle, planId)
    fun planStop() = NativeEngine.nativePlanStop(handle)
    fun planRelease(planId: Int) = NativeEngine.nativePlanRelease(handle, planId)

    fun pollDj(): DjSnapshot? = if (NativeEngine.nativePollDj(handle, djBuf)) DjSnapshot.fromArray(djBuf) else null

    /** Opens the output file of an offline render of [planId]; result JSON {ok, exportId, frames} or {ok:false, error}. */
    fun exportBegin(planId: Int, path: String, bits: Int): String = NativeEngine.nativeExportBegin(handle, planId, path, bits)

    /** Renders up to [frames] more frames; [progress] receives [written, expected, lastStartedItem, waitingItem, peak]. False = finished. */
    fun exportStep(exportId: Int, frames: Int, progress: DoubleArray): Boolean = NativeEngine.nativeExportStep(handle, exportId, frames, progress)
    fun exportEnd(exportId: Int, discard: Boolean): String = NativeEngine.nativeExportEnd(handle, exportId, discard)

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
        const val OVERVIEW_COLUMNS = 512

        /** Runs the native granular benchmark on the calling thread (call it from a background dispatcher). */
        fun runBenchmark(quick: Boolean): BenchmarkResult {
            val o = JSONObject(NativeEngine.nativeRunBenchmark(quick))
            return BenchmarkResult(o.getString("markdown"), o.getJSONObject("json").toString(2))
        }

        fun benchmarkProgress(): Int = NativeEngine.nativeBenchmarkProgress()
    }
}

data class BenchmarkResult(val markdown: String, val json: String)
