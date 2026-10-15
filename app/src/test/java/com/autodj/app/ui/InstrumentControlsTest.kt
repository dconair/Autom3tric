package com.autodj.app.ui

import com.autodj.app.audio.ParamSpec
import com.autodj.app.audio.ParamType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class InstrumentControlsTest {
    @Test
    fun knobQuantisationSnapsToTheRequestedNumberOfSteps() {
        assertEquals(0.37f, quantizeKnob(0.37f, 0), 0f)
        assertEquals(0.5f, quantizeKnob(0.49f, 2), 0f)
        assertEquals(1f, quantizeKnob(1.7f, 4), 0f)
        assertEquals(0f, quantizeKnob(-3f, 4), 0f)
        // 48 steps = whole semitones on a +-24 st range
        val v = quantizeKnob(0.5104f, 48)
        assertEquals(0.5f, v, 1e-6f)
    }

    @Test
    fun knobAngleSweeps270DegreesAroundTwelveOClock() {
        assertEquals(-135f, knobAngleDegrees(0f), 1e-4f)
        assertEquals(0f, knobAngleDegrees(0.5f), 1e-4f)
        assertEquals(135f, knobAngleDegrees(1f), 1e-4f)
        assertEquals(135f, knobAngleDegrees(7f), 1e-4f)
    }

    @Test
    fun everyEnvelopeShapeIsZeroAtBothEndsAndPeaksAtOneInTheMiddle() {
        for (shape in 0..5) {
            assertEquals("shape $shape start", 0f, envelopeShape(shape, 0f), 1e-6f)
            assertEquals("shape $shape end", 0f, envelopeShape(shape, 1f), 1e-6f)
            assertEquals("shape $shape centre", 1f, envelopeShape(shape, 0.5f), 1e-5f)
            for (i in 0..100) {
                val x = i / 100f
                val y = envelopeShape(shape, x)
                assertTrue("shape $shape at $x = $y", y in -1e-6f..1.000001f)
                assertEquals("shape $shape symmetric at $x", y, envelopeShape(shape, 1f - x), 1e-5f)
            }
        }
    }

    @Test
    fun envelopeShapesMatchTheNativeDefinitions() {
        // values checked against native EnvelopeTables::evaluate (test_envelopes.cpp uses the same points)
        assertEquals(0.5f, envelopeShape(3, 0.125f), 1e-6f)      // trapezoid ramp
        assertEquals(0.5f, envelopeShape(4, 0.015f), 1e-6f)      // square ramp (3 %)
        assertEquals(0.5f, envelopeShape(5, 0.25f), 1e-6f)       // triangle
        assertEquals(0.5f, envelopeShape(0, 0.25f), 1e-6f)       // hann
        assertEquals(1f, envelopeShape(2, 0.3f), 1e-6f)          // tukey flat top
        assertEquals(envelopeShape(0, 0.1f), envelopeShape(99, 0.1f), 0f)  // unknown -> hann, like the engine
    }

    @Test
    fun cellPadsAreNamedLikeMidiKeysFromC3() {
        assertEquals("C3", cellNoteName(0))
        assertEquals("C#3", cellNoteName(1))
        assertEquals("B3", cellNoteName(11))
        assertEquals("C4", cellNoteName(12))
        assertEquals("D#4", cellNoteName(15))
    }

    @Test
    fun valueFormatters() {
        assertEquals("11 %", formatPercent(0.11f))
        assertEquals("+0 st", formatSemitones(0f))
        assertEquals("-7 st", formatSemitones(-7f))
        assertEquals("+0.0 dB", formatDb(0f))
        assertEquals("+6.0 dB", formatDb(6f))
        assertEquals("23 /s", formatDensity(23.4f))
        assertEquals("3.5 /s", formatDensity(3.5f))
    }

    // ---- the registry snapshot (kept in sync with the native definition by the ctest param_specs_snapshot) ----

    private val specs: List<ParamSpec> by lazy {
        val json = checkNotNull(javaClass.getResourceAsStream("/param_specs.json")) { "param_specs.json missing" }.bufferedReader().readText()
        ParamSpec.parseAll(json)
    }

    private fun spec(key: String) = checkNotNull(specs.firstOrNull { it.key == key }) { "missing $key" }

    @Test
    fun theInstrumentFindsEveryParameterItNeedsForEveryVoice() {
        for (v in 0 until 4) {
            for (k in listOf("cell_mode", "cell_count", "cell_index", "step_div", "position", "size_ms", "density", "spread",
                "pitch_st", "env_shape", "sync_mode", "sync_div", "gate", "source")) {
                assertNotNull("granular.v$v.$k", specs.firstOrNull { it.key == "granular.v$v.$k" })
            }
        }
        assertNotNull(spec("granular.chaos"))
        assertNotNull(spec("granular.output_db"))
    }

    @Test
    fun instrumentParametersHaveTheRangesTheScreenAssumes() {
        assertEquals(listOf("Hold", "Step", "Keys"), spec("granular.v0.cell_mode").enumNames)
        assertEquals(listOf("8", "16"), spec("granular.v0.cell_count").enumNames)
        assertEquals(6, spec("granular.v0.env_shape").enumNames.size)
        assertEquals(7, spec("granular.v0.sync_div").enumNames.size)   // SyncKnobOrder permutes exactly these seven
        assertEquals(ParamType.Int, spec("granular.v0.cell_index").type)
        assertEquals(15f, spec("granular.v0.cell_index").max, 0f)
        assertEquals(-24f, spec("granular.output_db").min, 0f)
        assertEquals(6f, spec("granular.output_db").max, 0f)
        assertEquals(0f, spec("granular.output_db").def, 0f)
        assertEquals(0f, spec("granular.chaos").def, 0f)
        assertEquals(48, (spec("granular.v0.pitch_st").max - spec("granular.v0.pitch_st").min).toInt())
    }

    @Test
    fun parameterIdsAreDenseAndOrdered() {
        assertEquals(specs.indices.toList(), specs.map { it.id })   // the ViewModel indexes its state list by id
    }
}
