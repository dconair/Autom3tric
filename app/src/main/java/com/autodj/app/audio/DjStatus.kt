package com.autodj.app.audio

/** One deck as the audio thread last reported it. */
data class DeckSnapshot(
    val loaded: Boolean = false,
    val playing: Boolean = false,
    val ended: Boolean = false,
    val trackId: Int = 0,
    val posSec: Double = 0.0,
    val durationSec: Double = 0.0,
    /** Tempo ratio applied to the song (1.0 = native tempo). */
    val rate: Double = 1.0,
    /** Effective tempo = native bpm x rate; 0 = the song has no detected tempo. */
    val bpm: Double = 0.0,
    /** 0..1 within the current beat of the song's own grid. */
    val beatPhase: Double = 0.0,
    val peakL: Float = 0f,
    val peakR: Float = 0f,
)

/** The automatic mix as the audio thread last reported it. */
data class PlanSnapshot(
    val active: Boolean = false,
    val finished: Boolean = false,
    /** Plan item that started last, -1 before the first one. */
    val item: Int = -1,
    /** Transition that is running right now, -1 = none. */
    val transition: Int = -1,
    /** Plan item that is due but whose audio has not arrived yet, -1 = none. */
    val waiting: Int = -1,
    val timeSec: Double = 0.0,
    val totalSec: Double = 0.0,
    val beat: Double = 0.0,
    val bpm: Double = 0.0,
)

data class DjSnapshot(val deck: List<DeckSnapshot> = listOf(DeckSnapshot(), DeckSnapshot()), val plan: PlanSnapshot = PlanSnapshot()) {
    companion object {
        const val SIZE = 31
        val EMPTY = DjSnapshot()

        fun fromArray(v: DoubleArray): DjSnapshot {
            fun deck(o: Int) = DeckSnapshot(
                loaded = v[o] != 0.0, playing = v[o + 1] != 0.0, ended = v[o + 2] != 0.0, trackId = v[o + 3].toInt(), posSec = v[o + 4],
                durationSec = v[o + 5], rate = v[o + 6], bpm = v[o + 7], beatPhase = v[o + 8], peakL = v[o + 9].toFloat(), peakR = v[o + 10].toFloat(),
            )
            return DjSnapshot(
                listOf(deck(0), deck(11)),
                PlanSnapshot(
                    active = v[22] != 0.0, finished = v[23] != 0.0, item = v[24].toInt(), transition = v[25].toInt(), waiting = v[26].toInt(),
                    timeSec = v[27], totalSec = v[28], beat = v[29], bpm = v[30],
                ),
            )
        }
    }
}
