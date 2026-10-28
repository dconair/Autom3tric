package com.autodj.app.dj

import org.json.JSONArray
import org.json.JSONObject

/** One detected song section (intro, body, drop, break, outro). */
data class SectionInfo(val startSec: Double, val endSec: Double, val label: String)

/** What the native analyser found out about a track. Mirrors native/analysis/Analysis.h. */
data class TrackAnalysisInfo(
    val durationSec: Double,
    val bpm: Double,
    val beatSec: Double,
    val gridOffsetSec: Double,
    val tempoConfidence: Double,
    val gridConfidence: Double,
    val downbeatConfidence: Double,
    val keyName: String,
    val camelot: String,
    val keyConfidence: Double,
    val rmsDb: Double,
    val peakDb: Double,
    val trimDb: Double,
    val barSec: Double,
    val firstBarSec: Double,
    val barEnergy: List<Float>,
    val sections: List<SectionInfo>,
    val mixInSec: Double,
    val mixOutSec: Double,
    /** The original analysis JSON, kept so that loading the track again can skip the analysis. */
    val json: String,
) {
    val hasTempo: Boolean get() = bpm > 0.0
    val hasKey: Boolean get() = camelot.isNotEmpty()

    companion object {
        fun fromJson(o: JSONObject): TrackAnalysisInfo {
            val energy = o.optJSONArray("barEnergy")?.let { a -> List(a.length()) { a.optDouble(it, 0.0).toFloat() / 100f } } ?: emptyList()
            val sections = o.optJSONArray("sections")?.let { a ->
                List(a.length()) {
                    val s = a.getJSONObject(it)
                    SectionInfo(s.optDouble("start"), s.optDouble("end"), s.optString("label"))
                }
            } ?: emptyList()
            return TrackAnalysisInfo(
                durationSec = o.optDouble("durationSec", 0.0),
                bpm = o.optDouble("bpm", 0.0),
                beatSec = o.optDouble("beatSec", 0.0),
                gridOffsetSec = o.optDouble("gridOffsetSec", 0.0),
                tempoConfidence = o.optDouble("tempoConfidence", 0.0),
                gridConfidence = o.optDouble("gridConfidence", 0.0),
                downbeatConfidence = o.optDouble("downbeatConfidence", 0.0),
                keyName = o.optString("keyName", ""),
                camelot = o.optString("camelot", ""),
                keyConfidence = o.optDouble("keyConfidence", 0.0),
                rmsDb = o.optDouble("rmsDb", -120.0),
                peakDb = o.optDouble("peakDb", -120.0),
                trimDb = o.optDouble("trimDb", 0.0),
                barSec = o.optDouble("barSec", 0.0),
                firstBarSec = o.optDouble("firstBarSec", 0.0),
                barEnergy = energy,
                sections = sections,
                mixInSec = o.optDouble("mixInSec", 0.0),
                mixOutSec = o.optDouble("mixOutSec", 0.0),
                json = o.toString(),
            )
        }
    }
}

/** Waveform overview for the library row: min/max per column, -127..127. */
class Overview(val min: ByteArray, val max: ByteArray) {
    val columns: Int get() = min.size

    fun toJson(): JSONObject = JSONObject()
        .put("min", JSONArray(min.map { it.toInt() }))
        .put("max", JSONArray(max.map { it.toInt() }))

    companion object {
        fun fromJson(o: JSONObject?): Overview? {
            if (o == null) return null
            val mn = o.optJSONArray("min") ?: return null
            val mx = o.optJSONArray("max") ?: return null
            if (mn.length() != mx.length() || mn.length() == 0) return null
            return Overview(ByteArray(mn.length()) { mn.optInt(it).coerceIn(-127, 127).toByte() }, ByteArray(mx.length()) { mx.optInt(it).coerceIn(-127, 127).toByte() })
        }
    }
}

enum class TrackStatus { Queued, Decoding, Analysing, Ready, Failed }

/** One row of the library. Immutable: the controller replaces it when something changes. */
data class TrackItem(
    val id: String,
    val name: String,
    /** File path, content:// URI, or `demo:<n>` for the built-in synthetic songs. */
    val source: String,
    val status: TrackStatus = TrackStatus.Queued,
    /** 0..1 while decoding/analysing; negative = unknown (indeterminate). */
    val progress: Float = -1f,
    val error: String? = null,
    val analysis: TrackAnalysisInfo? = null,
    val overview: Overview? = null,
    val addedAt: Long = 0L,
) {
    val isDemo: Boolean get() = source.startsWith(DEMO_PREFIX)
    val ready: Boolean get() = status == TrackStatus.Ready && analysis != null

    companion object {
        const val DEMO_PREFIX = "demo:"
    }
}
