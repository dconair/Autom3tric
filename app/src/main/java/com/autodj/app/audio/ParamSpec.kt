package com.autodj.app.audio

import org.json.JSONArray
import java.util.Locale
import kotlin.math.abs
import kotlin.math.ln
import kotlin.math.pow
import kotlin.math.roundToInt

enum class ParamType { Float, Int, Bool, Enum }

/**
 * One entry of the native parameter registry (ParamRegistry::specsToJson). The UI never hard-codes ranges,
 * units or curves: sliders, formatting and defaults all come from here, so a parameter added in C++ shows up
 * in the app without touching Kotlin.
 */
data class ParamSpec(
    val id: Int,
    val key: String,
    val label: String,
    val group: String,
    val unit: String,
    val min: Float,
    val max: Float,
    val def: Float,
    val type: ParamType,
    val log: Boolean,
    val enumNames: List<String>,
) {
    /** Same mapping as ParamRegistry::toNormalized. */
    fun toNormalized(v: Float): Float {
        val x = v.coerceIn(min, max)
        if (max <= min) return 0f
        return if (log) (ln(x / min) / ln(max / min)) else (x - min) / (max - min)
    }

    /** Same mapping (including rounding of discrete types) as ParamRegistry::fromNormalized. */
    fun fromNormalized(n: Float): Float {
        val c = n.coerceIn(0f, 1f)
        val v = if (log) min * (max / min).pow(c) else min + c * (max - min)
        val clamped = v.coerceIn(min, max)
        return if (type == ParamType.Float) clamped else clamped.roundToInt().toFloat()
    }

    fun format(v: Float): String = when (type) {
        ParamType.Bool -> if (v >= 0.5f) "on" else "off"
        ParamType.Enum -> enumNames.getOrElse(v.roundToInt()) { v.roundToInt().toString() }
        ParamType.Int -> v.roundToInt().toString() + unitSuffix()
        ParamType.Float -> {
            if (unit == "%") {
                "${(v * 100f).roundToInt()} %"
            } else {
                val span = abs(max - min)
                val decimals = if (span >= 100f) 0 else if (span >= 10f) 1 else 2
                String.format(Locale.US, "%.${decimals}f", v) + unitSuffix()
            }
        }
    }

    private fun unitSuffix() = if (unit.isEmpty()) "" else " $unit"

    /** Key without the voice prefix, e.g. "granular.v2.size_ms" -> "size_ms". */
    val shortKey: String get() = key.substringAfterLast('.')

    companion object {
        fun parseAll(json: String): List<ParamSpec> {
            val arr = JSONArray(json)
            return List(arr.length()) { i ->
                val o = arr.getJSONObject(i)
                val names = o.optJSONArray("enum")
                ParamSpec(
                    id = o.getInt("id"),
                    key = o.getString("key"),
                    label = o.getString("label"),
                    group = o.getString("group"),
                    unit = o.optString("unit", ""),
                    min = o.getDouble("min").toFloat(),
                    max = o.getDouble("max").toFloat(),
                    def = o.getDouble("def").toFloat(),
                    type = ParamType.entries[o.getInt("type")],
                    log = o.getInt("scale") == 1,
                    enumNames = if (names == null) emptyList() else List(names.length()) { names.getString(it) },
                )
            }
        }
    }
}
