package com.autodj.app.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.animation.animateColorAsState
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.widthIn
import com.autodj.app.design.AutoDjShape
import com.autodj.app.design.AutoDjSpace
import com.autodj.app.design.Btn
import com.autodj.app.design.Display
import androidx.compose.foundation.layout.fillMaxSize
import com.autodj.app.design.Segment
import com.autodj.app.design.Tile
import com.autodj.app.design.TileTitle
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.gestures.waitForUpOrCancellation
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.PointerEventType
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.ProgressBarRangeInfo
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.onClick
import androidx.compose.ui.semantics.progressBarRangeInfo
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.setProgress
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autodj.app.design.AutoDjMotion
import com.autodj.app.design.AutoDjType
import com.autodj.app.design.Ds
import com.autodj.app.design.Txt
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.min
import kotlin.math.roundToInt

// ---------------------------------------------------------------------------------------------------------------
// Pure helpers (unit-tested without a device)
// ---------------------------------------------------------------------------------------------------------------

/** Snaps a knob position to `steps` equal intervals (0 = continuous). */
fun quantizeKnob(v: Float, steps: Int): Float {
    val c = v.coerceIn(0f, 1f)
    return if (steps <= 0) c else (c * steps).roundToInt().toFloat() / steps
}

/** Angle of the knob indicator in degrees, clockwise from 12 o'clock: -135 (min) .. +135 (max). */
fun knobAngleDegrees(v: Float): Float = -135f + 270f * v.coerceIn(0f, 1f)

/**
 * Grain envelope for the selector drawing; mirrors native EnvelopeTables::evaluate (indices Hann, Gauss, Tukey,
 * Trapezoid, Square, Triangle; unknown indices fall back to Hann like the engine does).
 */
fun envelopeShape(shape: Int, x: Float): Float {
    val t = x.coerceIn(0f, 1f)
    return when (shape) {
        1 -> {
            fun raw(u: Float): Float { val d = (u - 0.5f) / 0.17f; return exp(-0.5f * d * d) }
            val edge = raw(0f)
            (raw(t) - edge) / (1f - edge)
        }
        2 -> {
            val a = 0.5f
            when {
                t < a / 2 -> 0.5f * (1f + cos((2.0 * PI / a * (t - a / 2)).toFloat()))
                t > 1f - a / 2 -> 0.5f * (1f + cos((2.0 * PI / a * (t - 1f + a / 2)).toFloat()))
                else -> 1f
            }
        }
        3 -> min(1f, min(t / 0.25f, (1f - t) / 0.25f))
        4 -> min(1f, min(t / 0.03f, (1f - t) / 0.03f))
        5 -> 1f - abs(2f * t - 1f)
        else -> 0.5f * (1f - cos((2.0 * PI * t).toFloat()))
    }
}

/** MIDI-style note names for the cell pads, starting at C3 (the first key of the cell range). */
fun cellNoteName(index: Int): String {
    val names = arrayOf("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
    return names[index % 12] + (3 + index / 12)
}

// ---------------------------------------------------------------------------------------------------------------
// Knob
// ---------------------------------------------------------------------------------------------------------------

/**
 * Round dark knob with a single indicator tick, label and value underneath. Drag up/right to increase,
 * down/left to decrease; double tap resets. `value` is normalised 0..1; `steps` > 0 makes it discrete.
 */
@Composable
fun Knob(
    label: String,
    valueText: String,
    value: Float,
    onValueChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
    size: Dp = 64.dp,
    steps: Int = 0,
    onReset: (() -> Unit)? = null,
    help: String? = null,
) {
    val currentValue by rememberUpdatedState(value)
    val currentOnChange by rememberUpdatedState(onValueChange)
    val shown by animateFloatAsState(value, tween(AutoDjMotion.ms(AutoDjMotion.fast), easing = AutoDjMotion.out), label = "knob")
    val raw = remember { floatArrayOf(0f) }
    val dragRange = with(LocalDensity.current) { 180.dp.toPx() }
    val a11y = Modifier.semantics(mergeDescendants = true) {
        contentDescription = "$label, $valueText"
        progressBarRangeInfo = ProgressBarRangeInfo(value.coerceIn(0f, 1f), 0f..1f, if (steps > 0) steps - 1 else 0)
        setProgress { target ->
            currentOnChange(quantizeKnob(target, steps))
            true
        }
    }
    Column(modifier.then(a11y).then(if (help != null) Modifier.helpText(help) else Modifier), horizontalAlignment = Alignment.CenterHorizontally) {
        Canvas(
            Modifier
                .size(size)
                .pointerInput(steps) {
                    // mouse wheel / two-finger scroll turns the knob as well (desktop); each notch moves it a little
                    awaitPointerEventScope {
                        while (true) {
                            val e = awaitPointerEvent()
                            if (e.type == PointerEventType.Scroll) {
                                val dy = e.changes.fold(0f) { acc, c -> acc + c.scrollDelta.y }
                                if (dy != 0f) {
                                    val unit = if (steps > 0) 1f / steps else 0.02f
                                    val next = quantizeKnob(currentValue - dy.coerceIn(-3f, 3f) * unit, steps)
                                    if (next != currentValue) currentOnChange(next)
                                    e.changes.forEach { it.consume() }
                                }
                            }
                        }
                    }
                }
                .pointerInput(steps) {
                    detectDragGestures(
                        onDragStart = { raw[0] = currentValue },
                        onDrag = { change, drag ->
                            change.consume()
                            raw[0] = (raw[0] + (drag.x - drag.y) / dragRange).coerceIn(0f, 1f)
                            val next = quantizeKnob(raw[0], steps)
                            if (next != currentValue) currentOnChange(next)
                        },
                    )
                }
                .pointerInput(onReset) { detectTapGestures(onDoubleTap = { onReset?.invoke() }) },
        ) {
            val r = this.size.minDimension / 2f
            drawCircle(Ds.c.ink, radius = r)
            val rad = Math.toRadians(knobAngleDegrees(shown).toDouble())
            val dir = Offset(kotlin.math.sin(rad).toFloat(), -kotlin.math.cos(rad).toFloat())
            val c = center
            drawLine(
                color = Ds.c.onInk,
                start = c + dir * (r * 0.5f),
                end = c + dir * (r * 0.86f),
                strokeWidth = (r * 0.07f).coerceAtLeast(2.5f),
                cap = StrokeCap.Round,
            )
        }
        if (label.isNotEmpty()) Txt(label, style = AutoDjType.small, second = true, textAlign = TextAlign.Center, modifier = Modifier.padding(top = 6.dp))
        if (valueText.isNotEmpty()) Txt(valueText, style = AutoDjType.small, textAlign = TextAlign.Center)
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Segmented control, stepper, pads, envelope selector (all on the components of the design language)
// ---------------------------------------------------------------------------------------------------------------

/** 2-4 options with a sliding fill; [selected] < 0 shows them as plain buttons (none chosen, e.g. a loaded song is the source). */
@Composable
fun Segmented(
    options: List<String>,
    selected: Int,
    onSelect: (Int) -> Unit,
    modifier: Modifier = Modifier,
    help: String? = null,
) {
    val m = modifier.then(if (help != null) Modifier.helpText(help) else Modifier)
    if (selected < 0) {
        Row(m, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            options.forEachIndexed { i, text -> Btn(text, onClick = { onSelect(i) }, small = true) }
        }
    } else {
        Segment(options, selected, onSelect, m)
    }
}

/** "‹ 1/4 ›" - cycles through a list of values. */
@Composable
fun Stepper(text: String, onPrev: () -> Unit, onNext: () -> Unit, modifier: Modifier = Modifier, help: String? = null) {
    Row(
        modifier.then(if (help != null) Modifier.helpText(help) else Modifier),
        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Btn("‹", onClick = onPrev, small = true, modifier = Modifier.width(36.dp), description = "zurück")
        Txt(text, style = AutoDjType.body, textAlign = TextAlign.Center, modifier = Modifier.widthIn(min = 44.dp), maxLines = 1)
        Btn("›", onClick = onNext, small = true, modifier = Modifier.width(36.dp), description = "weiter")
    }
}

@Composable
fun OutlinedPill(text: String, onClick: () -> Unit, modifier: Modifier = Modifier, selected: Boolean = false, help: String? = null, small: Boolean = false, primary: Boolean = false) {
    Btn(text, onClick = onClick, on = selected, primary = primary, small = small, modifier = modifier.then(if (help != null) Modifier.helpText(help) else Modifier))
}

/** Dark key for one cell. Calls [onPress] on touch-down and [onRelease] on lift/cancel. */
@Composable
fun PadKey(label: String, active: Boolean, onPress: () -> Unit, onRelease: () -> Unit, modifier: Modifier = Modifier) {
    var down by remember { mutableStateOf(false) }
    val press by rememberUpdatedState(onPress)
    val release by rememberUpdatedState(onRelease)
    val c = Ds.c
    val fill by animateColorAsState(if (down || active) c.ink2 else c.ink, tween(AutoDjMotion.ms(if (down) AutoDjMotion.instant else AutoDjMotion.fast)), label = "padkey")
    Box(
        modifier
            .height(AutoDjSpace.control)
            .clip(AutoDjShape)
            .background(fill)
            .semantics {
                role = Role.Button
                contentDescription = "cell $label"
                onClick(label = "play") { press(); release(); true }
            }
            .pointerInput(Unit) {
                awaitEachGesture {
                    awaitFirstDown()
                    down = true
                    press()
                    waitForUpOrCancellation()
                    down = false
                    release()
                }
            },
        contentAlignment = Alignment.Center,
    ) { Txt(label, style = AutoDjType.small, color = c.onInk, maxLines = 1) }
}

/**
 * Shape picker for the grain envelope: a tile with a display that shows the current shape (a tap on it, or ‹ ›, picks the next), and
 * [below] for whatever belongs to the same tile.
 */
@Composable
fun EnvelopeSelector(
    names: List<String>, selected: Int, onSelect: (Int) -> Unit, modifier: Modifier = Modifier, help: String? = null,
    below: @Composable ColumnScope.() -> Unit = {},
) {
    val count = names.size.coerceAtLeast(1)
    val c = Ds.c
    Tile(modifier.then(if (help != null) Modifier.helpText(help) else Modifier)) {
        TileTitle("Hüllkurve") {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                Txt(names.getOrElse(selected) { "" }.lowercase(), style = AutoDjType.small, second = true, modifier = Modifier.padding(end = 6.dp))
                Btn("‹", onClick = { onSelect((selected + count - 1) % count) }, small = true, modifier = Modifier.width(36.dp), description = "vorige Form")
                Btn("›", onClick = { onSelect((selected + 1) % count) }, small = true, modifier = Modifier.width(36.dp), description = "nächste Form")
            }
        }
        Display(Modifier.fillMaxWidth().padding(top = 12.dp).height(80.dp).clickable(indication = null, interactionSource = remember { MutableInteractionSource() }, role = Role.Button) { onSelect((selected + 1) % count) }) {
            Canvas(Modifier.fillMaxSize().padding(horizontal = 16.dp, vertical = 12.dp)) {
                val path = Path()
                val n = 96
                for (i in 0..n) {
                    val x = i / n.toFloat()
                    val px = x * size.width
                    val py = size.height * (1f - envelopeShape(selected, x))
                    if (i == 0) path.moveTo(px, py) else path.lineTo(px, py)
                }
                drawPath(path, c.screenInk, style = Stroke(width = 2f * density, cap = StrokeCap.Round))
            }
        }
        Column(Modifier.padding(top = 12.dp), content = below)
    }
}

@Composable
fun SpacerRow(content: @Composable () -> Unit) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly, verticalAlignment = Alignment.Top) { content() }
}
