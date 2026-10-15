package com.autodj.app.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
import androidx.compose.material3.Text
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
// Segmented control, stepper, pads, envelope selector
// ---------------------------------------------------------------------------------------------------------------

private val PillShape = RoundedCornerShape(10.dp)

/** Pill with 2-4 options; the selected one sits on a slightly darker plate. */
@Composable
fun Segmented(
    options: List<String>,
    selected: Int,
    onSelect: (Int) -> Unit,
    modifier: Modifier = Modifier,
    help: String? = null,
) {
    Row(modifier.clip(PillShape).border(1.dp, Palette.outline, PillShape).then(if (help != null) Modifier.helpText(help) else Modifier)) {
        options.forEachIndexed { i, text ->
            val on = i == selected
            Box(
                Modifier
                    .heightIn(min = 40.dp)
                    .background(if (on) Palette.bodySunken else Color.Transparent)
                    .clickable(role = Role.RadioButton) { onSelect(i) }
                    .padding(horizontal = 12.dp),
                contentAlignment = Alignment.Center,
            ) {
                Text(text, color = if (on) Palette.ink else Palette.muted, fontSize = 14.sp)
            }
        }
    }
}

/** "‹ 1/4 ›" - cycles through a list of values. */
@Composable
fun Stepper(text: String, onPrev: () -> Unit, onNext: () -> Unit, modifier: Modifier = Modifier, help: String? = null) {
    Row(
        modifier.clip(PillShape).border(1.dp, Palette.outline, PillShape).then(if (help != null) Modifier.helpText(help) else Modifier),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.heightIn(min = 40.dp).clickable(role = Role.Button, onClickLabel = "previous") { onPrev() }.padding(horizontal = 12.dp), Alignment.Center) {
            Text("‹", color = Palette.muted, fontSize = 18.sp)
        }
        Box(Modifier.width(52.dp), Alignment.Center) { Text(text, color = Palette.ink, fontSize = 14.sp) }
        Box(Modifier.heightIn(min = 40.dp).clickable(role = Role.Button, onClickLabel = "next") { onNext() }.padding(horizontal = 12.dp), Alignment.Center) {
            Text("›", color = Palette.muted, fontSize = 18.sp)
        }
    }
}

@Composable
fun OutlinedPill(text: String, onClick: () -> Unit, modifier: Modifier = Modifier, selected: Boolean = false, help: String? = null) {
    Box(
        modifier
            .then(if (help != null) Modifier.helpText(help) else Modifier)
            .heightIn(min = 40.dp)
            .clip(PillShape)
            .background(if (selected) Palette.bodySunken else Color.Transparent)
            .border(1.dp, Palette.outline, PillShape)
            .clickable(role = Role.Button) { onClick() }
            .padding(horizontal = 14.dp),
        contentAlignment = Alignment.Center,
    ) { Text(text, color = if (selected) Palette.ink else Palette.muted, fontSize = 14.sp) }
}

/** Dark key for one cell. Calls [onPress] on touch-down and [onRelease] on lift/cancel. */
@Composable
fun PadKey(label: String, active: Boolean, onPress: () -> Unit, onRelease: () -> Unit, modifier: Modifier = Modifier) {
    var down by remember { mutableStateOf(false) }
    val press by rememberUpdatedState(onPress)
    val release by rememberUpdatedState(onRelease)
    Box(
        modifier
            .height(46.dp)
            .clip(RoundedCornerShape(9.dp))
            .background(if (down || active) Palette.padActive else Palette.pad)
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
    ) { Text(label, color = if (down || active) Palette.ink else Palette.padText, fontSize = 13.sp) }
}

/** Shape picker for the grain envelope: drawing of the current shape with ‹ › arrows. */
@Composable
fun EnvelopeSelector(names: List<String>, selected: Int, onSelect: (Int) -> Unit, modifier: Modifier = Modifier, help: String? = null) {
    val count = names.size.coerceAtLeast(1)
    val shape = RoundedCornerShape(14.dp)
    Row(
        modifier.clip(shape).border(1.dp, Palette.outline, shape).then(if (help != null) Modifier.helpText(help) else Modifier).padding(horizontal = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.heightIn(min = 56.dp).clickable(role = Role.Button, onClickLabel = "previous shape") { onSelect((selected + count - 1) % count) }.padding(horizontal = 12.dp), Alignment.Center) {
            Text("‹", color = Palette.muted, fontSize = 20.sp)
        }
        Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally) {
            Canvas(Modifier.fillMaxWidth().height(40.dp).padding(top = 8.dp, bottom = 2.dp, start = 8.dp, end = 8.dp)) {
                val path = Path()
                val n = 64
                for (i in 0..n) {
                    val x = i / n.toFloat()
                    val px = x * size.width
                    val py = size.height * (1f - envelopeShape(selected, x))
                    if (i == 0) path.moveTo(px, py) else path.lineTo(px, py)
                }
                drawPath(path, Palette.ink, style = Stroke(width = 2.2f * density, cap = StrokeCap.Round))
            }
            Text(names.getOrElse(selected) { "" }.lowercase(), color = Palette.muted, fontSize = 13.sp, modifier = Modifier.padding(bottom = 6.dp))
        }
        Box(Modifier.heightIn(min = 56.dp).clickable(role = Role.Button, onClickLabel = "next shape") { onSelect((selected + 1) % count) }.padding(horizontal = 12.dp), Alignment.Center) {
            Text("›", color = Palette.muted, fontSize = 20.sp)
        }
    }
}

/** Thin vertical divider between knob groups. */
@Composable
fun GroupDivider(modifier: Modifier = Modifier) {
    Box(modifier.width(1.dp).heightIn(min = 96.dp).background(Palette.outline.copy(alpha = 0.7f)))
}

@Composable
fun SpacerRow(content: @Composable () -> Unit) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly, verticalAlignment = Alignment.Top) { content() }
}
