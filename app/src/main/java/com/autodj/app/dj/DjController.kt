package com.autodj.app.dj

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.autodj.app.audio.AudioEngine
import com.autodj.app.audio.DjSnapshot
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.util.UUID

/** A library track in a granular source slot. */
data class GranularSlot(val trackId: String, val name: String)

const val FIRST_GRANULAR_SLOT = 4
const val MAX_GRANULAR_SLOTS = 12

/**
 * The DJ side of the app that is not audio-thread code: the track library, the import queue (decode -> native analysis),
 * persistence. Platform code supplies the [TrackDecoder] and the [LibraryStore]; everything else is shared between the
 * Android app and the desktop app. Native calls run on [io]; the Compose state is only touched on [ui].
 */
class DjController(
    private val engine: AudioEngine,
    private val decoder: TrackDecoder,
    private val store: LibraryStore,
    private val scope: CoroutineScope,
    private val io: CoroutineDispatcher = Dispatchers.IO,
    private val ui: CoroutineDispatcher = Dispatchers.Main,
    private val clock: () -> Long = System::currentTimeMillis,
) {
    val tracks = mutableStateListOf<TrackItem>()

    /** Library tracks that sit in granular source slots [FIRST_GRANULAR_SLOT] and up, in slot order (shown on the instrument's source chips). */
    val granularSlots = mutableStateListOf<GranularSlot>()

    /** Called on [ui] after a granular slot was (re)loaded, so that waveform caches can be dropped. */
    var onGranularSlotLoaded: (slot: Int) -> Unit = {}

    private var nextGranularReplace = 0

    /** The automatic mix: list, plan, playback, export. */
    val mix = MixController(this, engine, scope, io, ui)

    /** The two decks of the manual mixer. */
    val decks = DeckController(this, engine, scope, io, ui)

    /** Effects and drum pads. */
    val perf = PerformanceController(engine)

    /** What the audio thread reported last (decks and mix); refreshed by [poll]. */
    var snapshot by mutableStateOf(DjSnapshot.EMPTY)
        private set

    /** Called ~30 times a second from the UI loop of the host. */
    fun poll() {
        engine.pollDj()?.let {
            snapshot = it
            decks.onStatus(it)
            mix.onStatus(it)
        }
    }

    private val queue = Channel<String>(Channel.UNLIMITED)

    /**
     * The same rows as [tracks], readable from any thread: a Compose state list must not be iterated while the UI thread writes
     * to it (ConcurrentModificationException), and the importer and the mix controller look rows up from worker threads.
     */
    private val byId = java.util.concurrent.ConcurrentHashMap<String, TrackItem>()

    init {
        tracks.addAll(store.load())
        for (t in tracks) byId[t.id] = t
        scope.launch(io) {
            for (id in queue) process(id)
        }
    }

    // ---- library --------------------------------------------------------------------------------

    /** Queues files for import; returns how many were new (already known sources are skipped). */
    fun importSources(sources: List<String>): Int {
        var added = 0
        for (src in sources) {
            if (tracks.any { it.source == src }) continue
            val item = TrackItem(id = UUID.randomUUID().toString(), name = if (src.startsWith(TrackItem.DEMO_PREFIX)) "Demo-Song ${(src.removePrefix(TrackItem.DEMO_PREFIX).toIntOrNull() ?: 0) + 1}" else decoder.displayName(src), source = src, addedAt = clock())
            tracks.add(item)
            byId[item.id] = item
            queue.trySend(item.id)
            added++
        }
        return added
    }

    /** The four built-in synthetic songs (different tempo and key), analysed like any other file. */
    fun addDemoTracks(): Int = importSources((0..3).map { TrackItem.DEMO_PREFIX + it })

    fun remove(id: String) {
        mix.onLibraryRemoved(id)
        tracks.removeAll { it.id == id }
        byId.remove(id)
        persist()
    }

    /** A library row by id; safe to call from any thread. */
    fun item(id: String): TrackItem? = byId[id]

    /** True while any import is still decoding or analysing. */
    val busy: Boolean get() = byId.values.any { it.status == TrackStatus.Queued || it.status == TrackStatus.Decoding || it.status == TrackStatus.Analysing }

    // ---- use of a track by the engine -----------------------------------------------------------

    /**
     * Decodes [item] again (the library keeps only the analysis) and registers it with the engine; returns the native
     * track id, or null with [onError] describing why not. Blocks: call from a background context.
     */
    fun loadTrackBlocking(item: TrackItem, onError: (String) -> Unit = {}): Int? {
        return try {
            val json = if (item.isDemo) {
                engine.addDemoTrack(item.source.removePrefix(TrackItem.DEMO_PREFIX).toIntOrNull() ?: 0, keep = true, overviewColumns = 0)
            } else {
                decodeAndFinish(item, keep = true, cachedJson = item.analysis?.json, onStage = {}, onProgress = {})
            }
            val o = JSONObject(json)
            if (!o.optBoolean("ok")) {
                onError(o.optString("error", "unknown error"))
                null
            } else {
                o.optInt("trackId").takeIf { it > 0 }
            }
        } catch (e: DecodeException) {
            onError(e.message ?: "decode error")
            null
        } catch (e: Exception) {
            onError(e.message ?: e.javaClass.simpleName)
            null
        }
    }

    /** Puts a library track into a granular source slot (the instrument's source chips). Blocking; background context. */
    fun loadIntoGranularBlocking(item: TrackItem, slot: Int, onError: (String) -> Unit = {}): Boolean {
        val id = loadTrackBlocking(item, onError) ?: return false
        val ok = engine.trackToGranularSource(id, slot)
        engine.trackRelease(id)  // the engine keeps the audio it was handed alive on its own
        if (!ok) onError("Could not hand the track to the granular engine")
        return ok
    }

    /**
     * Makes [item] a source of the granular instrument and reports the slot (or the error) on [ui]. A track that is
     * already in a slot is reused; when all slots are taken the oldest one is replaced.
     */
    fun sendToGranular(item: TrackItem, onDone: (slot: Int?, error: String?) -> Unit = { _, _ -> }) {
        val have = granularSlots.indexOfFirst { it.trackId == item.id }
        if (have >= 0) {
            onDone(FIRST_GRANULAR_SLOT + have, null)
            return
        }
        val index = if (granularSlots.size < MAX_GRANULAR_SLOTS) granularSlots.size else nextGranularReplace++ % MAX_GRANULAR_SLOTS
        val slot = FIRST_GRANULAR_SLOT + index
        scope.launch(io) {
            var error: String? = null
            val ok = loadIntoGranularBlocking(item, slot) { error = it }
            withContext(ui) {
                if (ok) {
                    val entry = GranularSlot(item.id, item.name)
                    if (index < granularSlots.size) granularSlots[index] = entry else granularSlots.add(entry)
                    onGranularSlotLoaded(slot)
                    onDone(slot, null)
                } else {
                    onDone(null, error ?: "Konnte den Song nicht laden")
                }
            }
        }
    }

    // ---- import pipeline ------------------------------------------------------------------------

    private suspend fun process(id: String) {
        val item = byId[id] ?: return  // removed while queued
        try {
            update(id) { it.copy(status = TrackStatus.Decoding, progress = -1f, error = null) }
            val json = if (item.isDemo) {
                update(id) { it.copy(status = TrackStatus.Analysing, progress = 0.2f) }
                engine.addDemoTrack(item.source.removePrefix(TrackItem.DEMO_PREFIX).toIntOrNull() ?: 0, keep = false)
            } else {
                decodeAndFinish(
                    item, keep = false, cachedJson = null,
                    onStage = { s -> scope.launch(ui) { update(id) { it.copy(status = s, progress = if (s == TrackStatus.Analysing) 0.5f else -1f) } } },
                    onProgress = { p -> scope.launch(ui) { update(id) { it.copy(progress = p) } } },
                )
            }
            val o = JSONObject(json)
            if (!o.optBoolean("ok")) throw DecodeException(o.optString("error", "analysis failed"))
            val analysis = TrackAnalysisInfo.fromJson(o.getJSONObject("analysis"))
            val overview = Overview.fromJson(o.optJSONObject("overview"))
            update(id) { it.copy(status = TrackStatus.Ready, progress = 1f, analysis = analysis, overview = overview, error = null) }
            persist()
        } catch (e: Exception) {
            update(id) { it.copy(status = TrackStatus.Failed, progress = 0f, error = e.message ?: e.javaClass.simpleName) }
        }
    }

    private suspend fun update(id: String, f: (TrackItem) -> TrackItem) {
        withContext(ui) {
            val i = tracks.indexOfFirst { it.id == id }
            if (i >= 0) {
                val next = f(tracks[i])
                tracks[i] = next
                byId[id] = next
            }
        }
    }

    private fun persist() {
        scope.launch(ui) {
            val snapshot = tracks.toList()  // read on the thread that writes the list
            withContext(io) {
                try {
                    store.save(snapshot)
                } catch (_: Exception) {
                    // a full disk must not break the session
                }
            }
        }
    }

    /** Decode on this thread, then let the native side convert + analyse. Returns the native result JSON. */
    private fun decodeAndFinish(
        item: TrackItem,
        keep: Boolean,
        cachedJson: String?,
        onStage: (TrackStatus) -> Unit,
        onProgress: (Float) -> Unit,
    ): String {
        var ticket = 0
        var lastReported = -1f
        val sink = object : PcmSink {
            override fun onFormat(sampleRate: Int, channels: Int) {
                ticket = engine.ingestBegin(sampleRate, channels)
                if (ticket == 0) throw DecodeException("Unsupported audio format ($sampleRate Hz, $channels channels)")
            }

            override fun onSamples(data: FloatArray, count: Int) {
                if (ticket == 0 || !engine.ingestAppend(ticket, data, count)) throw DecodeException("Audio data was rejected by the engine")
            }

            override fun onProgress(fraction: Float) {
                val p = (fraction * 0.5f).coerceIn(0f, 0.5f)
                if (p - lastReported >= 0.02f) {
                    lastReported = p
                    onProgress(p)
                }
            }
        }
        try {
            decoder.decode(item.source, sink)
        } catch (e: Throwable) {
            if (ticket != 0) engine.ingestCancel(ticket)
            throw e
        }
        if (ticket == 0) throw DecodeException("The file contains no audio")
        onStage(TrackStatus.Analysing)
        val t = ticket
        val poller = scope.launch(io) {
            while (isActive) {
                onProgress(0.5f + 0.5f * engine.ingestProgress(t) / 100f)
                delay(150)
            }
        }
        try {
            return engine.ingestFinish(t, item.name, cachedJson, keep)
        } finally {
            poller.cancel()
        }
    }
}
