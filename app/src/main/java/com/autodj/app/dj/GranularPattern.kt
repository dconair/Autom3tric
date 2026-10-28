package com.autodj.app.dj

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

const val PATTERN_STEPS = 16
const val PATTERN_REST = -1

/** A ready-made beat for the pattern grid: [build] gets the number of cells (8 or 16) and returns the cell of each of the 16 steps (-1 = rest). */
class PatternPreset(val name: String, val hint: String, val build: (cells: Int) -> IntArray)

private fun steps(n: Int, f: (Int) -> Int): IntArray = IntArray(PATTERN_STEPS) { s -> f(s).let { if (it < 0) PATTERN_REST else it % n } }

/** What the beat builder offers as starting points. */
val PatternPresets = listOf(
    PatternPreset("Original", "Die Stücke der Reihe nach, so wie der Song läuft") { n -> steps(n) { it } },
    PatternPreset("Rückwärts", "Die Stücke in umgekehrter Reihenfolge") { n -> steps(n) { n - 1 - (it % n) } },
    PatternPreset("Stotter", "Jedes Stück wird viermal wiederholt") { n -> steps(n) { (it / 4) * (n / 4).coerceAtLeast(1) } },
    PatternPreset("Synkope", "Wenige Treffer, die gegen den Takt springen") { n ->
        val hits = mapOf(0 to 0, 3 to 2, 6 to 1, 8 to 4, 10 to 2, 13 to 3)
        steps(n) { hits[it] ?: -1 }
    },
    PatternPreset("Vierer", "Ein kräftiger Schlag auf jeder Zählzeit") { n -> steps(n) { if (it % 4 == 0) 0 else -1 } },
    PatternPreset("Zufall", "Würfelt ein neues Muster") { n ->
        val rnd = java.util.Random()
        steps(n) { if (rnd.nextInt(10) < 7) rnd.nextInt(n) else -1 }
    },
    PatternPreset("Leer", "Alles aus: setze selbst Punkte ins Raster") { n -> steps(n) { -1 } },
)

/**
 * The 16-step patterns of the four granular voices ("beat builder"). Step s of voice v either plays one of the cells (slices) of the
 * source or is a rest; the audio thread gets the pattern whenever it changes. Compose state, so the grid redraws on its own.
 */
class GranularPattern(private val send: (voice: Int, mask: Int, cells: Long) -> Unit) {
    private val grids = Array(MAX_VOICES) { mutableStateListOf<Int>().also { l -> repeat(PATTERN_STEPS) { s -> l.add(s % 8) } } }
    private val names = Array(MAX_VOICES) { mutableStateOf("Original") }

    init {
        for (v in 0 until MAX_VOICES) push(v)
    }

    fun cell(voice: Int, step: Int): Int = grids[voice.coerceIn(0, MAX_VOICES - 1)][step]

    /** Name of the preset the voice's pattern started from, or "Eigenes Muster" once it was edited. */
    fun name(voice: Int): String = names[voice.coerceIn(0, MAX_VOICES - 1)].value

    fun isEmpty(voice: Int): Boolean = grids[voice.coerceIn(0, MAX_VOICES - 1)].all { it == PATTERN_REST }

    /** A tap on the dot of [cell] in column [step]: put the step on that cell, or make it a rest when it already is. */
    fun toggle(voice: Int, step: Int, cell: Int) {
        val g = grids[voice.coerceIn(0, MAX_VOICES - 1)]
        g[step] = if (g[step] == cell) PATTERN_REST else cell
        names[voice.coerceIn(0, MAX_VOICES - 1)].value = "Eigenes Muster"
        push(voice)
    }

    fun load(voice: Int, preset: PatternPreset, cells: Int) {
        val v = voice.coerceIn(0, MAX_VOICES - 1)
        val built = preset.build(cells)
        for (s in 0 until PATTERN_STEPS) grids[v][s] = built[s]
        names[v].value = preset.name
        push(v)
    }

    /** Sends the pattern of [voice] to the audio thread. */
    fun push(voice: Int) {
        val g = grids[voice.coerceIn(0, MAX_VOICES - 1)]
        var mask = 0
        var cells = 0L
        for (s in 0 until PATTERN_STEPS) {
            val c = g[s]
            if (c >= 0) {
                mask = mask or (1 shl s)
                cells = cells or ((c and 15).toLong() shl (4 * s))
            }
        }
        send(voice, mask, cells)
    }

    companion object {
        const val MAX_VOICES = 4
    }
}
