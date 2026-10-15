package com.autodj.app.ui

import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import com.autodj.app.design.AutoDjDesignTheme
import com.autodj.app.design.Ds

/** The one theme of the app (see design/Design.kt). Kept under this name for the callers. */
@Composable
fun AutoDjTheme(content: @Composable () -> Unit) = AutoDjDesignTheme(content)

/** One accent per voice, readable on the dark screen. */
val VoiceColors: List<Color> get() = Ds.c.let { listOf(it.deckALit, it.accentLit, it.deckBLit, it.padFxLit) }
