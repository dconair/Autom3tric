package com.autodj.desktop

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.ImageComposeScene
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.Density
import com.autodj.app.ui.AutoDjTheme
import com.autodj.app.ui.GuideOverlay
import com.autodj.app.ui.GuideState
import com.autodj.app.ui.Guides
import com.autodj.app.ui.LocalGuide
import org.jetbrains.skia.EncodedImageFormat
import java.io.File
import kotlin.test.Test
import kotlin.test.assertTrue

/** The first-start guide at desktop window size: every step of both lessons renders something (offscreen, CPU raster). */
@OptIn(ExperimentalComposeUiApi::class)
class GuideDesktopTest {
    @Test fun everyStepRenders() {
        val state = GuideState(seen = false)
        val scene = ImageComposeScene(width = 1100, height = 760, density = Density(1f)) {
            AutoDjTheme {
                CompositionLocalProvider(LocalGuide provides state) {
                    Box(Modifier.fillMaxSize().background(com.autodj.app.design.Ds.c.ground)) { GuideOverlay(state, onAction = {}, phase = 0.7f) }
                }
            }
        }
        try {
            val dir = File(System.getProperty("autodj.screenshotDir") ?: "build/screenshots").apply { mkdirs() }
            Guides.all.forEachIndexed { l, lesson ->
                lesson.steps.indices.forEach { s ->
                    state.show(l, s)
                    val bytes = checkNotNull(scene.settled().encodeToData(EncodedImageFormat.PNG)).bytes
                    File(dir, "desktop_guide_${l}_$s.png").writeBytes(bytes)
                    assertTrue(bytes.size > 8_000, "guide $l/$s looks blank (${bytes.size} bytes)")
                }
            }
        } finally {
            scene.close()
        }
    }
}
