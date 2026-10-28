package com.autodj.desktop

import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.platform.Font
import com.autodj.app.design.AutoDjType

/** Hanken Grotesk, Regular and Medium (resources/fonts). Called once before the first window is composed. */
fun installAutoDjFonts() {
    AutoDjType.family = FontFamily(
        Font(resource = "fonts/HankenGrotesk-Regular.ttf", weight = FontWeight.Normal),
        Font(resource = "fonts/HankenGrotesk-Medium.ttf", weight = FontWeight.Medium),
    )
}
