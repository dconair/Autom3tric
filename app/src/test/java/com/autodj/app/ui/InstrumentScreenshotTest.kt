package com.autodj.app.ui

import android.graphics.Bitmap
import android.graphics.Canvas
import android.os.Looper
import androidx.activity.ComponentActivity
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.background
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.SemanticsActions
import androidx.compose.ui.test.doubleClick
import androidx.compose.ui.test.hasContentDescription
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performSemanticsAction
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.click
import androidx.compose.ui.unit.dp
import androidx.compose.ui.geometry.Offset
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import org.robolectric.Shadows.shadowOf
import java.time.Duration
import java.io.File

/**
 * Renders the instrument with Compose on the JVM (Robolectric native graphics) and exercises its controls through
 * the accessibility tree. The PNGs land in app/build/screenshots; they are what the layout looks like, not what a
 * particular phone's fonts and GPU do.
 */
@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [34], qualifiers = "w411dp-h1200dp-xhdpi")
class InstrumentScreenshotTest {
    @get:Rule val compose = createAndroidComposeRule<ComponentActivity>()

    /**
     * compose's own captureToImage() posts to the main looper and then blocks on it, which cannot work when the test
     * thread *is* the main thread (Robolectric PAUSED looper). So: let the looper run a few frames, then draw the
     * window's decor view into a bitmap with Robolectric's native Canvas.
     */
    private fun shot(name: String) {
        val dir = File(System.getProperty("autodj.screenshotDir") ?: "build/screenshots").apply { mkdirs() }
        compose.waitForIdle()
        shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(200))
        val view = compose.activity.window.decorView
        val bmp = Bitmap.createBitmap(view.width, view.height, Bitmap.Config.ARGB_8888)
        view.draw(Canvas(bmp))
        File(dir, "$name.png").outputStream().use { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }
    }

    private fun show(host: InstrumentHost) {
        com.autodj.app.installAutoDjFonts()
        compose.setContent {
            AutoDjTheme { Box(Modifier.fillMaxSize().background(com.autodj.app.design.Ds.c.ground)) { InstrumentScreen(host) } }
        }
        compose.waitForIdle()
    }

    @Test fun holdMode() {
        show(FakeInstrumentHost())
        shot("instrument_hold")
    }

    @Test fun stepModeSixteenCells() {
        show(FakeInstrumentHost(mapOf("granular.v0.cell_mode" to 1f, "granular.v0.cell_count" to 1f, "granular.chaos" to 0.3f), cell = 5))
        shot("instrument_step16")
    }

    @Test fun keysModeSyncedGrains() {
        show(FakeInstrumentHost(mapOf("granular.v0.cell_mode" to 2f, "granular.v0.sync_mode" to 1f, "granular.v0.sync_div" to 1f, "granular.v0.env_shape" to 4f), cell = 2))
        shot("instrument_keys_sync")
    }

    // ---------------------------------------------------------------------------------------------------- behaviour

    @Test fun modeSegmentsChangeTheEngineParameter() {
        val host = FakeInstrumentHost()
        show(host)
        compose.onNodeWithText("step").performClick()
        assertEquals(1f, host.value("granular.v0.cell_mode"), 0f)
        compose.onNodeWithText("keys").performClick()
        assertEquals(2f, host.value("granular.v0.cell_mode"), 0f)
        compose.onNodeWithText("16").performClick()
        assertEquals(1f, host.value("granular.v0.cell_count"), 0f)
        compose.onNodeWithText("free").performClick()
        assertEquals(0f, host.value("granular.v0.sync_mode"), 0f)
        compose.onNodeWithText("sync").performClick()
        assertEquals(1f, host.value("granular.v0.sync_mode"), 0f)
    }

    @Test fun stepControlsAppearOnlyInStepModeAndResetCallsTheEngine() {
        val host = FakeInstrumentHost()
        show(host)
        compose.onNodeWithText("reset").assertDoesNotExist()
        compose.onNodeWithText("step").performClick()
        compose.onNodeWithText("reset").performClick()
        assertTrue(host.log.contains("resetSteps(0)"))
        assertEquals(3f, host.value("granular.v0.step_div"), 0f) // default 1/4
        // two "›" exist in step mode: the stepper's (first) and the envelope selector's (last)
        compose.onAllNodesWithText("›")[0].performClick()
        assertEquals(4f, host.value("granular.v0.step_div"), 0f)
        compose.onAllNodesWithText("‹")[0].performClick()
        compose.onAllNodesWithText("‹")[0].performClick()
        assertEquals(2f, host.value("granular.v0.step_div"), 0f)
        assertEquals("envelope untouched by the stepper", 0f, host.value("granular.v0.env_shape"), 0f)
    }

    @Test fun loadFileButtonOpensTheChooserAndShowsProgress() {
        val host = FakeInstrumentHost()
        show(host)
        compose.onNodeWithText("Datei laden").performClick()
        assertTrue(host.log.contains("loadFiles"))
        host.loadStatus = "Analysiere song.mp3 … 40 %"
        compose.waitForIdle()
        compose.onNodeWithText("Analysiere song.mp3 … 40 %").assertExists()
    }

    @Test fun beatGridPresetsAndTapsProgramThePattern() {
        val host = FakeInstrumentHost()
        show(host)
        compose.onNodeWithText("Beat bauen").assertDoesNotExist()
        compose.onNodeWithText("step").performClick()
        compose.onNodeWithText("Beat bauen").assertExists()
        // starts as the plain walk through the cells: every step plays
        assertEquals(0xFFFF, host.sentPatterns.last().second)
        compose.onNodeWithText("Vierer").performClick()
        assertEquals(0x1111, host.sentPatterns.last().second)
        compose.onNodeWithText("Leer").performClick()
        assertEquals(0, host.sentPatterns.last().second)
        // a tap on the grid: step 1, cell 2 (eight rows of 22 dp below a 16 dp header, a 22 dp gutter on the left)
        compose.onNode(hasContentDescription("Beat-Raster", substring = true)).performTouchInput {
            val gutter = 22.dp.toPx()
            val cw = (width - gutter) / 16f
            click(Offset(gutter + 1.5f * cw, 16.dp.toPx() + 2.5f * 22.dp.toPx()))
        }
        val (voice, mask, cells) = host.sentPatterns.last()
        assertEquals(0, voice)
        assertEquals(1 shl 1, mask)
        assertEquals(2L shl 4, cells)
        // tapping the same dot again makes the step a rest
        compose.onNode(hasContentDescription("Beat-Raster", substring = true)).performTouchInput {
            val gutter = 22.dp.toPx()
            val cw = (width - gutter) / 16f
            click(Offset(gutter + 1.5f * cw, 16.dp.toPx() + 2.5f * 22.dp.toPx()))
        }
        assertEquals(0, host.sentPatterns.last().second)
    }

    @Test fun beatStartSetsTheVoiceUpForACrispRhythmAndPlays() {
        val host = FakeInstrumentHost()
        show(host)
        compose.onNodeWithText("step").performClick()
        compose.onNodeWithText("Beat-Start").performClick()
        assertEquals("drums", 1f, host.value("granular.v0.source"), 0f)
        assertEquals("16 cells", 1f, host.value("granular.v0.cell_count"), 0f)
        assertEquals("beat sync", 1f, host.value("granular.v0.sync_mode"), 0f)
        assertEquals(0f, host.value("granular.v0.position"), 0f)
        assertEquals(0f, host.value("granular.v0.spray_ms"), 0f)
        assertEquals("one grain per step: the length of a 1/8 step at 120 bpm", 250f, host.value("granular.v0.size_ms"), 1f)
        assertEquals("playing", 1f, host.value("granular.v0.gate"), 0f)
        assertTrue(host.sentPatterns.last().second != 0)
        compose.onAllNodesWithText("Stopp")[0].performClick()
        assertEquals(0f, host.value("granular.v0.gate"), 0f)
    }

    @Test fun padsPlayTheirCellAndRelease() {
        val host = FakeInstrumentHost(mapOf("granular.v0.cell_mode" to 2f))
        show(host)
        compose.onNode(hasContentDescription("cell E3")).performTouchInput { down(center); up() }
        assertEquals(4f, host.value("granular.v0.cell_index"), 0f)
        assertTrue(host.log.indexOf("noteOn(0)") in 0 until host.log.indexOf("noteOff(0)"))
    }

    @Test fun knobsAreOperableThroughAccessibilityAndDoubleTapResets() {
        val host = FakeInstrumentHost()
        show(host)
        val chaos = compose.onNode(hasContentDescription("chaos", substring = true))
        chaos.performSemanticsAction(SemanticsActions.SetProgress) { it(0.6f) }
        assertEquals(0.6f, host.value("granular.chaos"), 1e-4f)
        chaos.performScrollTo().performTouchInput { doubleClick() }
        assertEquals(0f, host.value("granular.chaos"), 0f)
        // tune snaps to whole semitones
        compose.onNode(hasContentDescription("tune", substring = true)).performSemanticsAction(SemanticsActions.SetProgress) { it(0.7f) }
        assertEquals("0.7 snaps to semitone 34/48 -> +10 st", 10f, host.value("granular.v0.pitch_st"), 1e-3f)
    }

    @Test fun touchOnTheScreenPlaysAndDragsThePositionInHoldMode() {
        val host = FakeInstrumentHost()
        show(host)
        compose.onNode(hasContentDescription("source overview", substring = true)).performTouchInput {
            down(Offset(width * 0.25f, height * 0.5f))
            moveTo(Offset(width * 0.75f, height * 0.5f))
            up()
        }
        assertEquals(0.75f, host.value("granular.v0.position"), 0.03f)
        assertTrue(host.log.contains("noteOn(0)") && host.log.contains("noteOff(0)"))
    }

    @Test fun grainsKnobSwitchesBetweenRateAndTempoDivision() {
        val host = FakeInstrumentHost()
        show(host)
        compose.onNode(hasContentDescription("grains", substring = true)).performSemanticsAction(SemanticsActions.SetProgress) { it(1f) }
        assertTrue("density rises in free mode", host.value("granular.v0.density") > 1000f)
        compose.onNodeWithText("sync").performClick()
        compose.onNode(hasContentDescription("grains", substring = true)).performSemanticsAction(SemanticsActions.SetProgress) { it(1f) }
        assertEquals("fastest division is 1/32 (enum index 3)", 3f, host.value("granular.v0.sync_div"), 0f)
        compose.onNode(hasContentDescription("grains", substring = true)).performSemanticsAction(SemanticsActions.SetProgress) { it(0f) }
        assertEquals("slowest is 1/4 (enum index 0)", 0f, host.value("granular.v0.sync_div"), 0f)
    }

    @Test fun envelopeArrowsCycleThroughAllSixShapes() {
        val host = FakeInstrumentHost()
        show(host)
        val seen = mutableSetOf<Float>()
        repeat(6) {
            seen += host.value("granular.v0.env_shape")
            compose.onNodeWithText("›").performClick() // hold mode: only the envelope selector has arrows
        }
        assertEquals(6, seen.size)
        assertEquals(0f, host.value("granular.v0.env_shape"), 0f) // wrapped around
        compose.onNodeWithText("‹").performClick()
        assertEquals(5f, host.value("granular.v0.env_shape"), 0f)
    }
}
