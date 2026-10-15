package com.autodj.app.ui.dj

import com.autodj.app.dj.DjController

/** The tempo the effects, the pads and the pulse follow right now, and in a few words where it comes from. */
fun clockInfo(dj: DjController): Pair<Double, String> {
    val s = dj.snapshot
    val free = dj.perf.param("transport.bpm").value
    return when {
        s.plan.active && s.plan.bpm > 0 -> s.plan.bpm to "Auto-Mix"
        s.deck[0].playing && s.deck[0].bpm > 0 -> s.deck[0].bpm to "Deck A"
        s.deck[1].playing && s.deck[1].bpm > 0 -> s.deck[1].bpm to "Deck B"
        else -> free.toDouble() to "freier Takt"
    }
}

/** True while a deck or the auto mix plays (the pulse follows that music, see [com.autodj.app.design.Beat]). */
fun musicPlaying(dj: DjController): Boolean {
    val s = dj.snapshot
    return s.plan.active || s.deck[0].playing || s.deck[1].playing
}

/** Hands the engine's position in the bar to the UI's pulse; called once per status poll by both apps. */
fun feedBeat(dj: DjController, status: com.autodj.app.audio.EngineStatus) {
    com.autodj.app.design.Beat.update(status.beatPhase, status.beatIndex, status.playing || musicPlaying(dj), status.beatFromMusic)
}
