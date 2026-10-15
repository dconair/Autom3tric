package com.autodj.app.ui

import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import com.autodj.app.design.AutoDjDesignTheme
import com.autodj.app.design.Ds

/** The one theme of the app (see design/Design.kt). Kept under this name for the callers. */
@Composable
fun AutoDjTheme(content: @Composable () -> Unit) = AutoDjDesignTheme(content)

/**
 * Old colour names of the instrument screen, mapped onto the roles of the design language so that pieces that have not been
 * converted yet still sit on the same board. New code uses `Ds.c` directly.
 */
object Palette {
    val body: Color get() = Ds.c.ground
    val bodyRaised: Color get() = Ds.c.tile
    val bodySunken: Color get() = Ds.c.tile2
    val ink: Color get() = Ds.c.ink
    val inkSoft: Color get() = Ds.c.ink
    val muted: Color get() = Ds.c.ink2
    val outline: Color get() = Ds.c.tile3
    val pad: Color get() = Ds.c.ink
    val padActive: Color get() = Ds.c.accent
    val padText: Color get() = Ds.c.onInk
    val screen: Color get() = Ds.c.screen
    val screenGrid: Color get() = Ds.c.screen2
    val screenText: Color get() = Ds.c.screenDim
    val wave: Color get() = Ds.c.screenInk
    val copper: Color get() = Ds.c.accent
    val good: Color get() = Ds.c.accent
    val warn: Color get() = Ds.c.warn
    val bad: Color get() = Ds.c.bad
}

/** One accent per voice, readable on the dark screen. */
val VoiceColors: List<Color> get() = Ds.c.let { listOf(it.deckALit, it.accentLit, it.deckBLit, it.padFxLit) }
