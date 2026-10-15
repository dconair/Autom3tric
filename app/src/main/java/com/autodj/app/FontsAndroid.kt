package com.autodj.app

import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import com.autodj.app.design.AutoDjType

/** Hanken Grotesk, Regular and Medium (res/font). Called once before the first screen is composed. */
fun installAutoDjFonts() {
    AutoDjType.family = FontFamily(
        Font(R.font.hankengrotesk_regular, FontWeight.Normal),
        Font(R.font.hankengrotesk_medium, FontWeight.Medium),
    )
}
