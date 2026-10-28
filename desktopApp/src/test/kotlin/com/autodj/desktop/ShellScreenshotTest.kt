package com.autodj.desktop

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.ImageComposeScene
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.dp
import com.autodj.app.design.AutoDjSpace
import com.autodj.app.design.Beat
import com.autodj.app.design.DesktopTopBar
import com.autodj.app.design.Ds
import com.autodj.app.design.PhoneBottomNav
import com.autodj.app.design.PhoneTopBar
import org.jetbrains.skia.EncodedImageFormat
import java.io.File
import kotlin.test.Test
import kotlin.test.assertTrue

/** The bars of the two apps (top bar of the desktop window, phone top and bottom bar), in day and night. */
@OptIn(ExperimentalComposeUiApi::class)
class ShellScreenshotTest {
    private fun render(name: String, width: Int, height: Int, content: @androidx.compose.runtime.Composable () -> Unit) {
        val scene = ImageComposeScene(width = width, height = height, density = Density(1f)) { Themed { content() } }
        try {
            val bytes = checkNotNull(scene.settled().encodeToData(EncodedImageFormat.PNG)).bytes
            val dir = File(System.getProperty("autodj.screenshotDir") ?: "build/screenshots").apply { mkdirs() }
            File(dir, "$name.png").writeBytes(bytes)
            assertTrue(bytes.size > 2_000, "render of $name looks blank")
        } finally {
            scene.close()
        }
    }

    private val tabs = listOf("Bibliothek", "Auto-Mix", "Decks", "Effekte", "Pads", "Granular")

    @Test fun desktopBar() {
        Beat.update(0.1f, 0, true, true)
        for (night in listOf(false, true)) {
            Ds.night = night
            render("shell_desktop_${if (night) "night" else "day"}", 1180, 90) {
                DesktopTopBar(tabs, 2, {}, 124.0, "Deck A", audioOn = true, onAudio = {}, modifier = Modifier.padding(AutoDjSpace.margin))
            }
        }
        Ds.night = false
        Beat.update(0f, 0, false, false)
    }

    @Test fun phoneBars() {
        render("shell_phone", 390, 140) {
            Column(Modifier.fillMaxWidth().padding(AutoDjSpace.phoneMargin)) {
                PhoneTopBar(120.0, audioOn = true, onAudio = {})
                androidx.compose.foundation.layout.Spacer(Modifier.height(16.dp))
                PhoneBottomNav(listOf("Songs", "Mix", "Decks", "FX", "Pads", "Granular"), 2, {})
            }
        }
    }
}
