package com.autodj.app.ui

import android.graphics.Bitmap
import android.graphics.Canvas
import android.os.Looper
import androidx.activity.ComponentActivity
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.hasContentDescription
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import com.autodj.app.dj.DjColors
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File
import java.time.Duration

/** The first-start guide: opens by itself once, walks through its steps, offers "try it now", renders every animation. */
@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [34], qualifiers = "w411dp-h900dp-xhdpi")
class GuideTest {
    @get:Rule val compose = createAndroidComposeRule<ComponentActivity>()

    private fun shot(name: String) {
        val dir = File(System.getProperty("autodj.screenshotDir") ?: "build/screenshots").apply { mkdirs() }
        compose.waitForIdle()
        shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(200))
        val view = compose.activity.window.decorView
        val bmp = Bitmap.createBitmap(view.width, view.height, Bitmap.Config.ARGB_8888)
        view.draw(Canvas(bmp))
        File(dir, "$name.png").outputStream().use { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }
    }

    private var phase by androidx.compose.runtime.mutableStateOf(0.5f)

    private fun show(state: GuideState, phase: Float, onAction: (GuideTarget) -> Unit = {}) {
        this.phase = phase
        compose.setContent {
            AutoDjTheme {
                CompositionLocalProvider(LocalGuide provides state) {
                    Box(Modifier.fillMaxSize().background(DjColors.bg)) {
                        TabTip("test", "Ein Hinweis im Reiter.")
                        GuideOverlay(state, onAction, phase = this@GuideTest.phase)
                    }
                }
            }
        }
        compose.waitForIdle()
    }

    @Test fun opensOnItsOwnOnlyBeforeItWasSeen() {
        assertTrue(GuideState(seen = false).open)
        assertFalse(GuideState(seen = true).open)
    }

    @Test fun walksThroughTheStepsAndMarksTheGuideSeen() {
        var seen = 0
        val state = GuideState(seen = false) { seen++ }
        show(state, 0.5f)
        compose.onNodeWithText(Guides.autoDj.steps[0].title).assertExists()
        for (i in 1 until Guides.autoDj.steps.size) {
            compose.onNodeWithText("Weiter").performClick()
            compose.onNodeWithText(Guides.autoDj.steps[i].title).assertExists()
        }
        compose.onNodeWithText("Zurück").performClick()
        compose.onNodeWithText(Guides.autoDj.steps[Guides.autoDj.steps.size - 2].title).assertExists()
        compose.onNodeWithText("Weiter").performClick()
        compose.onNodeWithText("Fertig").performClick()
        assertFalse(state.open)
        assertEquals(1, seen)
    }

    @Test fun tryItNowRunsTheActionAndClosesTheGuide() {
        val actions = mutableListOf<GuideTarget>()
        val state = GuideState(seen = false)
        show(state, 0.5f) { actions += it }
        compose.onNodeWithText("Demo-Songs laden").performClick()
        assertEquals(listOf(GuideTarget.LoadDemoSongs), actions)
        assertFalse(state.open)
    }

    @Test fun lessonChipSwitchesToThePlugin() {
        val state = GuideState(seen = false)
        show(state, 0.5f)
        compose.onNodeWithText("Granular-Plugin").performClick()
        compose.onNodeWithText(Guides.plugin.steps[0].title).assertExists()
    }

    @Test fun everyStepOfEveryLessonHasADescribedAnimationAndRenders() {
        val state = GuideState(seen = false)
        show(state, 0.5f)
        Guides.all.forEachIndexed { l, lesson ->
            lesson.steps.forEachIndexed { s, step ->
                compose.runOnIdle { state.show(l, s) }
                compose.onNode(hasContentDescription("Animation zu: ${step.title}")).assertExists()
                for (ph in listOf(0.3f, 0.6f, 0.8f)) {
                    compose.runOnIdle { phase = ph }
                    shot("guide_${l}_${s}_${(ph * 10).toInt()}")
                }
            }
        }
    }

    @Test fun tipsCanBeDismissed() {
        val state = GuideState(seen = true)
        show(state, 0.5f)
        compose.onNodeWithText("Ein Hinweis im Reiter.").assertExists()
        compose.onNode(hasContentDescription("Hinweis ausblenden")).performClick()
        assertTrue("test" in state.dismissedTips)
        compose.onNodeWithText("Ein Hinweis im Reiter.").assertDoesNotExist()
    }
}
