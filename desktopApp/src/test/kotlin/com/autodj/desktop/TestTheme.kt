package com.autodj.desktop

import androidx.compose.runtime.Composable
import com.autodj.app.ui.AutoDjTheme

/** The production theme, with the app's typeface installed: what every screenshot test renders inside. */
@Composable
fun Themed(content: @Composable () -> Unit) {
    installFontsOnce()
    AutoDjTheme(content)
}

private var fontsInstalled = false

private fun installFontsOnce() {
    if (!fontsInstalled) {
        installAutoDjFonts()
        fontsInstalled = true
    }
}

/** Renders a few frames so that enter animations (tiles rising in) have finished, and returns the last one. */
@OptIn(androidx.compose.ui.ExperimentalComposeUiApi::class)
fun androidx.compose.ui.ImageComposeScene.settled(): org.jetbrains.skia.Image {
    var t = 0L
    repeat(8) {
        render(t)
        t += 250_000_000L
    }
    return render(t)
}
