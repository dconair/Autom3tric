package com.autodj.app.dj

import org.json.JSONObject

/** How the transitions of an automatic mix are chosen. [code] is what the native planner expects. */
enum class TransitionChoice(val code: Int, val label: String, val hint: String) {
    Auto(0, "Automatisch", "Je Songpaar der passende Übergang, abhängig von Tempo und Tonart"),
    EqBlend(1, "Bass-Tausch", "Der neue Song kommt ohne Bass, in der Mitte tauschen die Bässe"),
    Crossfade(2, "Überblendung", "Weiche Überblendung der beiden Songs"),
    FilterSweep(3, "Filter", "Der alte Song wird ausgedünnt, der neue öffnet sich"),
    Cut(4, "Schnitt", "Kein Überlappen: der neue Song startet hart auf dem Taktanfang"),
}

/** Length of the overlap in beats; 0 = the planner decides per pair. */
enum class LengthChoice(val beats: Int, val label: String) {
    Auto(0, "Auto"),
    Short(8, "8 Beats"),
    Medium(16, "16 Beats"),
    Long(32, "32 Beats"),
}

/** How "Reihenfolge optimieren" sorts the list. [code] is the native OrderMode. */
enum class OrderChoice(val code: Int, val label: String) {
    Smart(1, "Passend (Tempo + Tonart)"),
    TempoUp(2, "Tempo aufsteigend"),
    TempoDown(3, "Tempo absteigend"),
}

enum class MixState { Idle, Loading, Playing, Stopping, Finished }

data class PlanItemInfo(
    val name: String,
    val deck: Int,
    val startSec: Double,
    val endSec: Double,
    /** Position in the song where it comes in. */
    val trackStartSec: Double,
    /** 2 = the song runs at double / 0.5 = half of its detected tempo to fit its neighbours. */
    val beatMul: Double,
    val gridTrusted: Boolean,
)

data class TransitionInfoK(
    val from: Int,
    val to: Int,
    val type: String,
    val startSec: Double,
    val lengthSec: Double,
    val lengthBeats: Double,
    val bpmFrom: Double,
    val bpmTo: Double,
    val tempoRamp: Boolean,
    val note: String,
) {
    val label: String
        get() = when (type) {
            "eq_blend" -> "Bass-Tausch"
            "crossfade" -> "Überblendung"
            "filter_sweep" -> "Filter"
            "cut" -> "Schnitt"
            else -> type
        }
}

/** The result of planning: when each song plays, which transitions happen. Mirrors planToJson in native/dj/MixPlan.cpp. */
data class PlanInfo(
    val planId: Int,
    val totalSec: Double,
    val items: List<PlanItemInfo>,
    val transitions: List<TransitionInfoK>,
) {
    companion object {
        fun fromJson(json: String): PlanInfo? {
            val o = JSONObject(json)
            if (!o.optBoolean("ok")) return null
            val p = o.getJSONObject("plan")
            val items = p.getJSONArray("items").let { a ->
                List(a.length()) {
                    val i = a.getJSONObject(it)
                    PlanItemInfo(
                        name = i.optString("name"), deck = i.optInt("deck"), startSec = i.optDouble("startSec"), endSec = i.optDouble("endSec"),
                        trackStartSec = i.optDouble("trackStartSec"), beatMul = i.optDouble("beatMul", 1.0), gridTrusted = i.optBoolean("gridTrusted"),
                    )
                }
            }
            val trs = p.getJSONArray("transitions").let { a ->
                List(a.length()) {
                    val t = a.getJSONObject(it)
                    TransitionInfoK(
                        from = t.optInt("from"), to = t.optInt("to"), type = t.optString("type"), startSec = t.optDouble("startSec"),
                        lengthSec = t.optDouble("lengthSec"), lengthBeats = t.optDouble("lengthBeats"), bpmFrom = t.optDouble("bpmFrom"),
                        bpmTo = t.optDouble("bpmTo"), tempoRamp = t.optBoolean("tempoRamp"), note = t.optString("note"),
                    )
                }
            }
            return PlanInfo(o.optInt("planId"), p.optDouble("totalSec"), items, trs)
        }
    }
}
