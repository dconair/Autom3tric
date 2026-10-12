package com.autodj.app.dj

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.autodj.app.audio.AudioEngine
import com.autodj.app.audio.DjSnapshot
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import org.json.JSONObject
import java.io.File
import java.util.Locale

/**
 * The automatic mix: a list of library songs, the plan the native planner makes of it (which transition where, tempo ramps),
 * live playback and the offline export. Songs are planned from their analysis alone; the decoded audio of a song is bound to
 * the plan shortly before it is needed and dropped once it has been played, so a long set never sits in memory as a whole.
 *
 * Threading: Compose state is only written on [ui]; decoding and native calls that take time run on [io].
 */
class MixController(
    private val dj: DjController,
    private val engine: AudioEngine,
    private val scope: CoroutineScope,
    private val io: CoroutineDispatcher,
    private val ui: CoroutineDispatcher,
) {
    /** Library ids of the songs in the mix, in play order. */
    val list = mutableStateListOf<String>()
    var transition by mutableStateOf(TransitionChoice.Auto)
        private set
    var length by mutableStateOf(LengthChoice.Auto)
        private set

    /** The plan for the current list (a preview until the mix is started). */
    var plan by mutableStateOf<PlanInfo?>(null)
        private set
    var state by mutableStateOf(MixState.Idle)
        private set

    /** What the audio thread reported last. */
    var snapshot by mutableStateOf(DjSnapshot.EMPTY)
        private set

    /** Progress text while the mix is being prepared ("Lade Song 2 von 5 …"). */
    var status by mutableStateOf<String?>(null)
        private set
    var error by mutableStateOf<String?>(null)

    /** The plan that is playing and the list index it started at (list index = [runningFrom] + plan item). */
    var runningPlan by mutableStateOf<PlanInfo?>(null)
        private set
    var runningFrom by mutableIntStateOf(0)
        private set

    /** 0..1 while an export runs, null otherwise. */
    var exportProgress by mutableStateOf<Float?>(null)
        private set
    var exportResult by mutableStateOf<String?>(null)
        private set

    /** Called on [ui] with the path of a finished export (Android copies it to the place the user picked). */
    var onExported: (path: String) -> Unit = {}

    val locked: Boolean get() = state == MixState.Loading || state == MixState.Playing || state == MixState.Stopping || exportProgress != null

    /** List index of the song that started last, or -1. */
    val currentIndex: Int get() = if (state == MixState.Playing && snapshot.plan.item >= 0) runningFrom + snapshot.plan.item else -1

    private class Run(val planId: Int, val info: PlanInfo, val items: List<TrackItem>) {
        val bound = BooleanArray(items.size)
        val loading = HashSet<Int>()
        val failed = HashSet<Int>()
    }

    private var running: Run? = null
    private val stubs = HashMap<String, Int>()
    private var replanJob: Job? = null
    private var exportJob: Job? = null
    @Volatile private var exportCancel = false

    // ---- the list ---------------------------------------------------------------------------------

    fun contains(id: String) = list.contains(id)

    fun add(id: String) {
        if (locked || list.contains(id) || dj.item(id)?.ready != true) return
        list.add(id)
        changed()
    }

    fun addAll() {
        if (locked) return
        for (t in dj.tracks) if (t.ready && !list.contains(t.id)) list.add(t.id)
        changed()
    }

    fun removeAt(index: Int) {
        if (locked || index !in list.indices) return
        list.removeAt(index)
        changed()
    }

    fun remove(id: String) {
        val i = list.indexOf(id)
        if (i >= 0) {
            list.removeAt(i)
            changed()
        }
    }

    fun move(index: Int, delta: Int) {
        val to = index + delta
        if (locked || index !in list.indices || to !in list.indices) return
        list.add(to, list.removeAt(index))
        changed()
    }

    fun clear() {
        if (locked) return
        list.clear()
        changed()
    }

    /** Sorts the list so that neighbours fit together (tempo, key) or by tempo; the first song stays first for [OrderChoice.Smart]. */
    fun optimiseOrder(how: OrderChoice) {
        if (locked || list.size < 2) return
        val ids = list.toList()
        scope.launch(io) {
            val stub = ids.map { id -> dj.item(id)?.let(::stubFor) ?: 0 }
            val order = engine.orderTracks(stub.toIntArray(), how.code)
            withContext(ui) {
                if (order.size == ids.size && list.toList() == ids) {
                    val sorted = order.map { ids[it] }
                    list.clear()
                    list.addAll(sorted)
                    changed()
                }
            }
        }
    }

    fun chooseTransition(t: TransitionChoice) {
        transition = t
        changed()
    }

    fun chooseLength(l: LengthChoice) {
        length = l
        changed()
    }

    /** The library dropped a song. */
    fun onLibraryRemoved(id: String) {
        if (state == MixState.Idle || state == MixState.Finished) remove(id)
        synchronized(stubs) { stubs.remove(id) }?.let { engine.trackRelease(it) }
    }

    private fun changed() {
        replanJob?.cancel()
        val ids = list.toList()
        val type = transition.code
        val len = length.beats
        if (ids.isEmpty()) {
            val old = plan
            plan = null
            old?.let { scope.launch(io) { engine.planRelease(it.planId) } }
            return
        }
        replanJob = scope.launch(io) {
            delay(40)  // a burst of edits plans once
            val stub = ids.mapNotNull { id -> dj.item(id)?.let(::stubFor) }
            val info = if (stub.size == ids.size) PlanInfo.fromJson(engine.planCreate(stub.toIntArray(), type, len)) else null
            withContext(ui) {
                val old = plan
                plan = info
                old?.let { scope.launch(io) { engine.planRelease(it.planId) } }
            }
        }
    }

    private fun stubFor(item: TrackItem): Int? = synchronized(stubs) {
        stubs[item.id] ?: item.analysis?.let { a ->
            engine.trackStub(item.name, a.json).takeIf { it > 0 }?.also { stubs[item.id] = it }
        }
    }

    // ---- playback ---------------------------------------------------------------------------------

    /** Plays the list from song [fromIndex]; a running mix is stopped first. */
    fun start(fromIndex: Int = 0) {
        if (state == MixState.Loading || state == MixState.Stopping || exportProgress != null) return
        val items = list.drop(fromIndex).mapNotNull { dj.item(it) }.filter { it.ready }
        if (items.isEmpty()) {
            error = "Die Mix-Liste ist leer. Füge Songs aus der Bibliothek hinzu."
            return
        }
        error = null
        exportResult = null
        state = MixState.Loading
        status = "Plane den Mix …"
        val type = transition.code
        val len = length.beats
        scope.launch(io) {
            haltPlayback()
            val stub = items.mapNotNull(::stubFor)
            val info = if (stub.size == items.size) PlanInfo.fromJson(engine.planCreate(stub.toIntArray(), type, len)) else null
            if (info == null) {
                fail("Der Mix konnte nicht geplant werden")
                return@launch
            }
            val run = Run(info.planId, info, items)
            // the first two songs have to be there before the first beat; the rest follows while the mix plays
            for (j in 0 until minOf(2, items.size)) {
                setStatus("Lade Song ${j + 1} von ${items.size}: ${items[j].name} …")
                if (!bindItem(run, j)) {
                    release(run)
                    fail("„${items[j].name}“ konnte nicht geladen werden")
                    return@launch
                }
            }
            if (!engine.planStart(run.planId)) {
                release(run)
                fail("Der Mix konnte nicht gestartet werden")
                return@launch
            }
            withContext(ui) {
                running = run
                runningPlan = info
                runningFrom = fromIndex
                status = null
                state = MixState.Playing
            }
        }
    }

    fun stop() {
        if (state != MixState.Playing && state != MixState.Finished) return
        state = MixState.Stopping
        scope.launch(io) {
            haltPlayback()
            withContext(ui) {
                state = MixState.Idle
                runningPlan = null
            }
        }
    }

    /** Jumps to the next song of the list with a cut on the bar (the mix restarts there). */
    fun skipToNext() {
        val i = currentIndex
        if (i < 0 || i + 1 >= list.size) return
        start(i + 1)
    }

    /** Called ~30 times a second with the audio thread's state. */
    fun onStatus(s: DjSnapshot) {
        snapshot = s
        val run = running ?: return
        if (state != MixState.Playing) return
        val p = s.plan
        if (p.finished) {
            state = MixState.Finished
            scope.launch(io) { haltPlayback() }
            return
        }
        prefetch(run, p.item, p.waiting)
    }

    private fun prefetch(run: Run, item: Int, waiting: Int) {
        for (j in intArrayOf(item + 1, waiting)) {
            if (j < 0 || j >= run.items.size) continue
            val start = synchronized(run) { !run.bound[j] && j !in run.failed && run.loading.add(j) }
            if (!start) continue
            scope.launch(io) {
                val ok = bindItem(run, j)
                synchronized(run) {
                    run.loading.remove(j)
                    if (!ok) run.failed.add(j)
                }
                if (!ok) withContext(ui) { error = "„${run.items[j].name}“ konnte nicht geladen werden und wird übersprungen" }
            }
        }
        // a song two places back is over (it shared the deck with the one that started last)
        for (j in 0 until item - 1) {
            val drop = synchronized(run) { run.bound[j].also { run.bound[j] = false } }
            if (drop) engine.planUnbind(run.planId, j)
        }
    }

    private fun bindItem(run: Run, j: Int): Boolean {
        var err: String? = null
        val id = dj.loadTrackBlocking(run.items[j]) { err = it }
        if (id == null) {
            if (err != null) scope.launch(ui) { error = "${run.items[j].name}: $err" }
            return false
        }
        val ok = engine.planBind(run.planId, j, id)
        engine.trackRelease(id)  // the plan keeps the audio alive
        if (ok) synchronized(run) { run.bound[j] = true }
        return ok
    }

    /** Stops the plan on the audio thread, waits until it let go of the songs, then frees them. */
    private suspend fun haltPlayback() {
        val run = withContext(ui) { running.also { running = null } } ?: return
        engine.planStop()
        withTimeoutOrNull(1000) {
            while (snapshot.plan.active || snapshot.plan.finished) delay(10)
        }
        release(run)
    }

    private fun release(run: Run) {
        for (j in run.items.indices) {
            val drop = synchronized(run) { run.bound[j].also { run.bound[j] = false } }
            if (drop) engine.planUnbind(run.planId, j)
        }
        engine.planRelease(run.planId)
    }

    private suspend fun setStatus(text: String) = withContext(ui) { status = text }

    private suspend fun fail(message: String) = withContext(ui) {
        error = message
        status = null
        state = MixState.Idle
    }

    // ---- export -----------------------------------------------------------------------------------

    /** Renders the whole list into a WAV file at [path] (16 or 24 bit) plus a text file with the track list next to it. */
    fun export(path: String, bits: Int = 16) {
        if (locked) return
        val items = list.mapNotNull { dj.item(it) }.filter { it.ready }
        if (items.isEmpty()) {
            error = "Die Mix-Liste ist leer."
            return
        }
        error = null
        exportResult = null
        exportCancel = false
        exportProgress = 0f
        val type = transition.code
        val len = length.beats
        exportJob = scope.launch(io) {
            var run: Run? = null
            var exportId = 0
            var ok = false
            var message: String? = null
            try {
                val stub = items.mapNotNull(::stubFor)
                val info = if (stub.size == items.size) PlanInfo.fromJson(engine.planCreate(stub.toIntArray(), type, len)) else null
                if (info == null) {
                    message = "Der Mix konnte nicht geplant werden"
                    return@launch
                }
                val r = Run(info.planId, info, items).also { run = it }
                for (j in 0 until minOf(2, items.size)) {
                    if (!bindItem(r, j)) {
                        message = "„${items[j].name}“ konnte nicht geladen werden"
                        return@launch
                    }
                }
                val begin = JSONObject(engine.exportBegin(r.planId, path, bits))
                if (!begin.optBoolean("ok")) {
                    message = begin.optString("error", "Export konnte nicht gestartet werden")
                    return@launch
                }
                exportId = begin.getInt("exportId")
                val prog = DoubleArray(5)
                var lastItem = -1
                while (!exportCancel) {
                    val more = engine.exportStep(exportId, EXPORT_STEP_FRAMES, prog)
                    val item = prog[2].toInt()
                    if (item != lastItem) {
                        lastItem = item
                        val want = item + 1
                        if (want in items.indices && !r.bound[want]) {
                            if (!bindItem(r, want)) {
                                message = "„${items[want].name}“ konnte nicht geladen werden"
                                return@launch
                            }
                        }
                        for (j in 0 until item - 1) if (r.bound[j]) {
                            r.bound[j] = false
                            engine.planUnbind(r.planId, j)
                        }
                    }
                    val fraction = if (prog[1] > 0) (prog[0] / prog[1]).toFloat().coerceIn(0f, 1f) else 0f
                    withContext(ui) { exportProgress = fraction }
                    if (!more) break
                }
                val end = JSONObject(engine.exportEnd(exportId, discard = exportCancel))
                exportId = 0
                if (exportCancel) {
                    message = "Export abgebrochen"
                } else if (end.optBoolean("ok")) {
                    ok = true
                    writeTracklist(path, info)
                } else {
                    message = end.optString("error").ifEmpty { "Export fehlgeschlagen" }
                }
            } catch (e: Exception) {
                message = e.message ?: e.javaClass.simpleName
            } finally {
                if (exportId != 0) engine.exportEnd(exportId, discard = true)
                run?.let { release(it) }
                withContext(ui) {
                    exportProgress = null
                    if (ok) {
                        exportResult = path
                        onExported(path)
                    } else if (message != null) {
                        error = message
                    }
                }
            }
        }
    }

    fun cancelExport() {
        exportCancel = true
    }

    private fun writeTracklist(wavPath: String, info: PlanInfo) {
        try {
            val f = File(wavPath)
            val txt = File(f.parentFile, f.nameWithoutExtension + "-Tracklist.txt")
            txt.writeText(trackList(info))
        } catch (_: Exception) {
            // the WAV is the result, the list is a courtesy
        }
    }

    companion object {
        const val EXPORT_STEP_FRAMES = 24000

        fun trackList(info: PlanInfo): String = buildString {
            info.items.forEachIndexed { i, it ->
                append(String.format(Locale.US, "%d:%02d  %02d  %s\n", it.startSec.toInt() / 60, it.startSec.toInt() % 60, i + 1, it.name))
            }
        }
    }
}
