package com.autodj.app.dj

import androidx.compose.runtime.State
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.autodj.app.audio.AudioEngine
import com.autodj.app.audio.DjSnapshot
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** What the UI shows of one deck besides the audio thread's status. */
class DeckUi {
    var item by mutableStateOf<TrackItem?>(null)
    var loading by mutableStateOf(false)
    internal var loadedAt = 0L
}

/**
 * The two decks of the manual mixer: load a library song, play / cue / seek, the mixer controls (volume, trim, EQ, filter,
 * tempo, sync, key lock) and the crossfader. The controls are engine parameters ("deck.a.eq_low_db" ...); this class keeps one
 * Compose state per parameter so that a fader recomposes only itself.
 */
class DeckController(
    private val dj: DjController,
    private val engine: AudioEngine,
    private val scope: CoroutineScope,
    private val io: CoroutineDispatcher,
    private val ui: CoroutineDispatcher,
    private val clock: () -> Long = System::currentTimeMillis,
) {
    val deck = listOf(DeckUi(), DeckUi())
    var error by mutableStateOf<String?>(null)

    private val states = HashMap<String, androidx.compose.runtime.MutableFloatState>()

    private fun key(d: Int, name: String) = if (d < 0) "mixer.$name" else "deck.${if (d == 0) "a" else "b"}.$name"

    /** The value of "deck.a.<name>" / "deck.b.<name>" (or "mixer.<name>" for d < 0) as a state. */
    fun param(d: Int, name: String): State<Float> = stateOf(key(d, name))

    private fun stateOf(k: String) = states.getOrPut(k) { mutableFloatStateOf(engine.spec(k)?.let { engine.getParam(it.id) } ?: 0f) }

    fun set(d: Int, name: String, value: Float) {
        val k = key(d, name)
        val spec = engine.spec(k) ?: return
        val v = value.coerceIn(spec.min, spec.max)
        stateOf(k).floatValue = v
        engine.setParam(spec.id, v)
    }

    fun reset(d: Int, name: String) {
        val spec = engine.spec(key(d, name)) ?: return
        set(d, name, spec.def)
    }

    private val mixBusy get() = dj.mix.state == MixState.Loading || dj.mix.state == MixState.Playing || dj.mix.state == MixState.Stopping

    fun load(d: Int, item: TrackItem) {
        if (mixBusy) {
            error = "Der Auto-Mix läuft und belegt beide Decks. Stoppe ihn zuerst."
            return
        }
        if (!item.ready || deck[d].loading) return
        deck[d].loading = true
        error = null
        scope.launch(io) {
            var err: String? = null
            val id = dj.loadTrackBlocking(item) { err = it }
            val ok = id != null && engine.deckLoad(d, id, item.analysis?.mixInSec?.coerceAtLeast(0.0) ?: 0.0)
            if (id != null) engine.trackRelease(id)  // the engine keeps what the deck plays
            withContext(ui) {
                deck[d].loading = false
                if (ok) {
                    deck[d].item = item
                    deck[d].loadedAt = clock()
                } else {
                    error = "„${item.name}“ konnte nicht geladen werden${err?.let { ": $it" } ?: ""}"
                }
            }
        }
    }

    fun eject(d: Int) {
        engine.deckUnload(d)
        deck[d].item = null
    }

    fun playPause(d: Int) {
        if (mixBusy) return
        engine.deckPlay(d, !dj.snapshot.deck[d].playing)
    }

    /** Back to the start of the song (its first bar) and stop. */
    fun cue(d: Int) {
        if (mixBusy) return
        engine.deckPlay(d, false)
        engine.deckSeek(d, deck[d].item?.analysis?.mixInSec?.coerceAtLeast(0.0) ?: 0.0)
    }

    fun seek(d: Int, sec: Double) {
        if (mixBusy) return
        engine.deckSeek(d, sec)
    }

    /** Called with every status poll: a deck that the engine emptied (the auto mix took the decks) loses its song in the UI. */
    fun onStatus(s: DjSnapshot) {
        for (d in 0..1) {
            val u = deck[d]
            if (u.item != null && !u.loading && !s.deck[d].loaded && clock() - u.loadedAt > 1000) u.item = null
        }
    }
}
