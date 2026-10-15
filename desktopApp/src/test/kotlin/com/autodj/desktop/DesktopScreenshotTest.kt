package com.autodj.desktop

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.ImageComposeScene
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.Density
import com.autodj.app.ui.AutoDjTheme
import com.autodj.app.ui.FakeInstrumentHost
import com.autodj.app.ui.InstrumentScreen
import com.autodj.app.ui.Palette
import org.jetbrains.skia.EncodedImageFormat
import java.io.File
import kotlin.test.Test
import kotlin.test.assertTrue

/**
 * Renders the shared instrument UI at desktop window size with Compose Multiplatform's offscreen scene (CPU raster,
 * no window or GPU needed) and a fake engine host. Writes build/screenshots/desktop_*.png; the test fails if the
 * render is blank. Shows the layout, not the fonts of a particular Mac.
 */
@OptIn(ExperimentalComposeUiApi::class)
class DesktopScreenshotTest {
    private fun render(name: String, host: FakeInstrumentHost, width: Int, height: Int) {
        val scene = ImageComposeScene(width = width, height = height, density = Density(1f)) {
            AutoDjTheme { Box(Modifier.fillMaxSize().background(Palette.body)) { InstrumentScreen(host) } }
        }
        try {
            val image = scene.render()
            val bytes = checkNotNull(image.encodeToData(EncodedImageFormat.PNG)).bytes
            val dir = File(System.getProperty("autodj.screenshotDir") ?: "build/screenshots").apply { mkdirs() }
            File(dir, "$name.png").writeBytes(bytes)
            // a blank scene would compress to a few hundred bytes; the instrument draws knobs, a waveform and a grain cloud
            assertTrue(bytes.size > 20_000, "render of $name looks blank (${bytes.size} bytes)")
        } finally {
            scene.close()
        }
    }

    @Test fun wideLayoutHold() = render("desktop_hold", FakeInstrumentHost(), 1100, 860)

    @Test fun wideLayoutStepSixteenCells() =
        render("desktop_step16", FakeInstrumentHost(mapOf("granular.v0.cell_mode" to 1f, "granular.v0.cell_count" to 1f, "granular.chaos" to 0.3f), cell = 5), 1100, 860)
}
