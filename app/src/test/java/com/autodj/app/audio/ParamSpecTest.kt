package com.autodj.app.audio

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class ParamSpecTest {
    private val json = """
      [
        {"id":0,"key":"transport.bpm","label":"Tempo","group":"Transport","unit":"BPM","min":40.0,"max":220.0,"def":120.0,"type":0,"scale":0,"flags":3},
        {"id":1,"key":"granular.v0.size_ms","label":"Grain size","group":"Granular 1","unit":"ms","min":5.0,"max":500.0,"def":80.0,"type":0,"scale":1,"flags":3},
        {"id":2,"key":"granular.v0.env_shape","label":"Envelope","group":"Granular 1","unit":"","min":0.0,"max":3.0,"def":0.0,"type":3,"scale":0,"flags":3,"enum":["Hann","Gauss","Tukey","Trapezoid"]},
        {"id":3,"key":"granular.v0.gate","label":"Gate","group":"Granular 1","unit":"","min":0.0,"max":1.0,"def":0.0,"type":2,"scale":0,"flags":3},
        {"id":4,"key":"granular.v0.reverse_prob","label":"Reverse","group":"Granular 1","unit":"%","min":0.0,"max":1.0,"def":0.0,"type":0,"scale":0,"flags":3}
      ]
    """.trimIndent()

    private val specs = ParamSpec.parseAll(json)

    @Test
    fun parsesTheNativeRegistryFormat() {
        assertEquals(5, specs.size)
        assertEquals(ParamType.Enum, specs[2].type)
        assertEquals(listOf("Hann", "Gauss", "Tukey", "Trapezoid"), specs[2].enumNames)
        assertTrue(specs[1].log)
        assertEquals("size_ms", specs[1].shortKey)
    }

    @Test
    fun logScaleNormalisationRoundTripsAndMatchesTheNativeFormula() {
        val s = specs[1]
        assertEquals(0f, s.toNormalized(5f), 1e-6f)
        assertEquals(1f, s.toNormalized(500f), 1e-6f)
        // geometric mean of the range sits in the middle of a log slider
        assertEquals(0.5f, s.toNormalized(Math.sqrt(5.0 * 500.0).toFloat()), 1e-5f)
        for (v in listOf(5f, 12f, 80f, 333f, 500f)) assertEquals(v, s.fromNormalized(s.toNormalized(v)), v * 1e-4f)
    }

    @Test
    fun discreteTypesRoundToWholeValues() {
        assertEquals(2f, specs[2].fromNormalized(0.67f), 0f)
        assertEquals(1f, specs[3].fromNormalized(0.9f), 0f)
        assertEquals(0f, specs[3].fromNormalized(0.2f), 0f)
    }

    @Test
    fun formatsValuesForDisplay() {
        assertEquals("120 BPM", specs[0].format(120f))
        assertEquals("80 ms", specs[1].format(80f))
        assertEquals("Tukey", specs[2].format(2f))
        assertEquals("on", specs[3].format(1f))
        assertEquals("25 %", specs[4].format(0.25f))
    }
}
