package com.autodj.app.dj

import kotlin.math.floor
import kotlin.math.roundToLong

/** A hotcue: a named place in a song that a pad jumps to. [auto] cues come from the analysis, the others were set by hand. */
data class HotCue(val sec: Double, val label: String, val auto: Boolean = true)

/** A running loop on a deck. [beats] is 0 for a loop that does not follow the beat length buttons (the cue loop). */
data class LoopState(val startSec: Double, val lengthSec: Double, val beats: Int) {
    val endSec: Double get() = startSec + lengthSec
}

/**
 * What the app prepares for a song on its own, from the analysis alone (tempo grid, bars, sections, mix-in / mix-out): hotcues at
 * the musical landmarks and the bar-aligned start of a loop. It can't look songs up in a DJ database (that would need an online
 * service); everything here is derived from the audio itself, so it also works for songs nobody ever catalogued.
 */
object CuePoints {
    const val SLOTS = 8
    val LoopBeats = listOf(1, 2, 4, 8, 16)

    /** Bars a "cue loop" repeats. */
    const val CueLoopBars = 4

    private class Candidate(val sec: Double, val label: String, val priority: Int)

    /**
     * Up to [SLOTS] cues in song order: the first bar, the start of every intro / drop / break / outro, and the best point to mix
     * out. Landmarks closer than two bars to a stronger one are dropped. All are snapped to the bar grid when it can be trusted.
     */
    fun auto(a: TrackAnalysisInfo): List<HotCue> {
        val found = ArrayList<Candidate>()
        found += Candidate(snapToBar(a, a.mixInSec.coerceAtLeast(0.0)), "Start", 100)
        val counts = HashMap<String, Int>()
        for (s in a.sections) {
            if (s.startSec < 0.5) continue
            val name = when (s.label) {
                "intro" -> "Intro"
                "drop" -> "Drop"
                "break" -> "Break"
                "outro" -> "Outro"
                else -> "Teil"
            }
            val n = (counts[name] ?: 0) + 1
            counts[name] = n
            val priority = when (s.label) {
                "drop" -> 90
                "break" -> 80
                "outro" -> 70
                "intro" -> 60
                else -> 40
            } - n  // earlier ones of a kind win
            found += Candidate(snapToBar(a, s.startSec), if (n > 1) "$name$n" else name, priority)
        }
        if (a.mixOutSec > 0.0) found += Candidate(snapToBar(a, a.mixOutSec), "Out", 95)

        val minGap = if (a.hasTempo && a.barSec > 0.0) a.barSec * 2.0 else 4.0
        val kept = ArrayList<Candidate>()
        for (c in found.sortedByDescending { it.priority }) {
            if (c.sec >= a.durationSec - 0.5 && a.durationSec > 0) continue
            if (kept.any { kotlin.math.abs(it.sec - c.sec) < minGap }) continue
            if (kept.size < SLOTS) kept += c
        }
        return kept.sortedBy { it.sec }.map { HotCue(it.sec, it.label) }
    }

    /** The cue list as the pad row shows it: [SLOTS] places, `null` for the empty ones. */
    fun slots(cues: List<HotCue>): List<HotCue?> = List(SLOTS) { cues.getOrNull(it) }

    private fun gridOk(a: TrackAnalysisInfo) = a.hasTempo && a.beatSec > 0.0

    /** The beat grid line nearest to [sec] (or [sec] itself when the song has no usable tempo). */
    fun snapToBeat(a: TrackAnalysisInfo, sec: Double): Double {
        if (!gridOk(a)) return sec
        val k = ((sec - a.gridOffsetSec) / a.beatSec).roundToLong()
        return (a.gridOffsetSec + k * a.beatSec).coerceAtLeast(0.0)
    }

    /** The bar line nearest to [sec]; falls back to the beat grid, then to [sec]. */
    fun snapToBar(a: TrackAnalysisInfo, sec: Double): Double {
        if (!(a.hasTempo && a.barSec > 0.0 && a.gridConfidence >= 0.4)) return snapToBeat(a, sec)
        val k = ((sec - a.firstBarSec) / a.barSec).roundToLong()
        return (a.firstBarSec + k * a.barSec).coerceAtLeast(0.0)
    }

    /** Start of a [beats]-beat loop at the position: the last beat line at or before it (with 60 ms of tolerance). */
    fun loopFor(a: TrackAnalysisInfo, posSec: Double, beats: Int): LoopState? {
        if (!gridOk(a) || beats <= 0) return null
        val k = floor((posSec + 0.06 - a.gridOffsetSec) / a.beatSec)
        val start = (a.gridOffsetSec + k * a.beatSec).coerceAtLeast(0.0)
        val len = beats * a.beatSec
        return if (start + len > a.durationSec) null else LoopState(start, len, beats)
    }

    /** [CueLoopBars] bars from the cue the playhead is in (the last one at or before it), else from the bar line before it. */
    fun cueLoopFor(a: TrackAnalysisInfo, cues: List<HotCue?>, posSec: Double): LoopState? {
        if (!gridOk(a)) return null
        val bar = if (a.barSec > 0.0) a.barSec else a.beatSec * 4.0
        val cue = cues.filterNotNull().filter { it.sec <= posSec + 0.06 }.maxByOrNull { it.sec }
        val start = cue?.sec ?: floor(((posSec - a.firstBarSec) / bar)).let { (a.firstBarSec + it * bar).coerceAtLeast(0.0) }
        val len = CueLoopBars * bar
        return if (start + len > a.durationSec) null else LoopState(start, len, 0)
    }
}
