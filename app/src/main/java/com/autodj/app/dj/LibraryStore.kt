package com.autodj.app.dj

import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/** Persists the analysed library (never the audio itself) so the next start shows the tracks without decoding anything. */
interface LibraryStore {
    fun load(): List<TrackItem>
    fun save(tracks: List<TrackItem>)
}

class MemoryLibraryStore : LibraryStore {
    private var saved: List<TrackItem> = emptyList()
    override fun load(): List<TrackItem> = saved
    override fun save(tracks: List<TrackItem>) {
        saved = tracks.filter { it.ready }
    }
}

/** library.json next to the app data: `{ "version": 1, "tracks": [ {id, name, source, addedAt, analysis, overview} ] }`. */
class JsonLibraryStore(private val file: File) : LibraryStore {
    override fun load(): List<TrackItem> {
        if (!file.isFile) return emptyList()
        return try {
            val root = JSONObject(file.readText())
            if (root.optInt("version", 1) > VERSION) return emptyList()
            val arr = root.optJSONArray("tracks") ?: return emptyList()
            buildList {
                for (i in 0 until arr.length()) {
                    val o = arr.optJSONObject(i) ?: continue
                    val analysis = o.optJSONObject("analysis")?.let { TrackAnalysisInfo.fromJson(it) } ?: continue
                    add(
                        TrackItem(
                            id = o.optString("id").ifEmpty { continue },
                            name = o.optString("name"),
                            source = o.optString("source"),
                            status = TrackStatus.Ready,
                            progress = 1f,
                            analysis = analysis,
                            overview = Overview.fromJson(o.optJSONObject("overview")),
                            addedAt = o.optLong("addedAt"),
                        ),
                    )
                }
            }
        } catch (e: Exception) {
            emptyList()  // a damaged cache only costs a re-import
        }
    }

    override fun save(tracks: List<TrackItem>) {
        val arr = JSONArray()
        for (t in tracks) {
            val a = t.analysis ?: continue
            if (!t.ready) continue
            arr.put(
                JSONObject().put("id", t.id).put("name", t.name).put("source", t.source).put("addedAt", t.addedAt)
                    .put("analysis", JSONObject(a.json)).put("overview", t.overview?.toJson()),
            )
        }
        file.parentFile?.mkdirs()
        val tmp = File(file.parentFile, file.name + ".tmp")
        tmp.writeText(JSONObject().put("version", VERSION).put("tracks", arr).toString())
        if (!tmp.renameTo(file)) {
            file.writeText(tmp.readText())
            tmp.delete()
        }
    }

    private companion object {
        const val VERSION = 1
    }
}
