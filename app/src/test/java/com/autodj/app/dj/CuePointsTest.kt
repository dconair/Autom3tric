package com.autodj.app.dj

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class CuePointsTest {
    /** 120 BPM, 4/4: a beat every 0.5 s, a bar every 2 s, grid starting at 0.1 s; four sections over 120 s. */
    private fun song(tempo: Boolean = true, sections: List<SectionInfo> = listOf(
        SectionInfo(0.0, 16.1, "intro"), SectionInfo(16.1, 48.1, "drop"), SectionInfo(48.1, 64.1, "break"),
        SectionInfo(64.1, 96.1, "drop"), SectionInfo(96.1, 120.0, "outro"),
    )) = TrackAnalysisInfo(
        durationSec = 120.0, bpm = if (tempo) 120.0 else 0.0, beatSec = if (tempo) 0.5 else 0.0, gridOffsetSec = 0.1,
        tempoConfidence = 0.9, gridConfidence = 0.9, downbeatConfidence = 0.8, keyName = "A minor", camelot = "8A", keyConfidence = 0.7,
        rmsDb = -14.0, peakDb = -1.0, trimDb = 0.0, barSec = if (tempo) 2.0 else 0.0, firstBarSec = 0.1, barEnergy = emptyList(),
        sections = sections, mixInSec = 0.1, mixOutSec = 96.1, json = "{}",
    )

    @Test
    fun findsTheLandmarksOfASongInOrderAndOnTheBarGrid() {
        val a = song()
        val cues = CuePoints.auto(a)
        assertEquals(listOf("Start", "Drop", "Break", "Drop2", "Out"), cues.map { it.label }.also { println(it) })
        assertEquals(cues.sortedBy { it.sec }, cues)
        for (c in cues) assertEquals("${c.label} sits on a bar line", 0.0, ((c.sec - a.firstBarSec) / a.barSec).let { it - Math.round(it) }, 1e-6)
        assertTrue(cues.all { it.auto })
    }

    @Test
    fun neverMoreCuesThanPadsAndLandmarksKeepTheirDistance() {
        val many = List(20) { SectionInfo(it * 6.0, it * 6.0 + 6.0, if (it % 2 == 0) "drop" else "break") }
        val a = song(sections = many)
        val cues = CuePoints.auto(a)
        assertTrue(cues.size <= CuePoints.SLOTS)
        for (i in 1 until cues.size) assertTrue(cues[i].sec - cues[i - 1].sec >= a.barSec * 2 - 1e-6)
        assertTrue("start and mix-out survive", cues.any { it.label == "Start" } && cues.any { it.label == "Out" })
    }

    @Test
    fun aSongWithoutTempoStillGetsCuesButNoLoops() {
        val a = song(tempo = false)
        assertTrue(CuePoints.auto(a).size >= 3)
        assertNull(CuePoints.loopFor(a, 10.0, 4))
        assertNull(CuePoints.cueLoopFor(a, emptyList(), 10.0))
        assertEquals(12.345, CuePoints.snapToBeat(a, 12.345), 0.0)
    }

    @Test
    fun beatLoopsStartOnTheBeatBeforeThePlayhead() {
        val a = song()
        val l = CuePoints.loopFor(a, 10.7, 4)!!
        assertEquals(10.6, l.startSec, 1e-9)  // beats at 0.1 + k * 0.5
        assertEquals(2.0, l.lengthSec, 1e-9)
        assertEquals(4, l.beats)
        assertEquals(10.6, CuePoints.loopFor(a, 10.58, 1)!!.startSec, 1e-9)  // 60 ms of tolerance: just before the beat counts as on it
        assertNull("no loop that would run past the end", CuePoints.loopFor(a, 119.9, 16))
    }

    @Test
    fun theCueLoopRepeatsFourBarsFromTheCueThePlayheadIsIn() {
        val a = song()
        val cues = CuePoints.slots(CuePoints.auto(a))
        val inDrop = cues.filterNotNull().first { it.label == "Drop" }
        val l = CuePoints.cueLoopFor(a, cues, inDrop.sec + 5.3)!!
        assertEquals(inDrop.sec, l.startSec, 1e-9)
        assertEquals(8.0, l.lengthSec, 1e-9)
        assertEquals(0, l.beats)
    }
}
