package com.autodj.app.design

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.animateColorAsState
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.gestures.waitForUpOrCancellation
import androidx.compose.foundation.hoverable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsHoveredAsState
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.lerp
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.layout.positionInParent
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.ProgressBarRangeInfo
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.onClick
import androidx.compose.ui.semantics.progressBarRangeInfo
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.selected as semSelected
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.setProgress
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import kotlin.math.roundToInt

// ---- text ---------------------------------------------------------------------------------------------------------------

/** Text with the design's styles. The colour defaults to the ink of the tile it sits on. */
@Composable
fun Txt(
    text: String,
    modifier: Modifier = Modifier,
    style: TextStyle = AutoDjType.body,
    color: Color? = null,
    second: Boolean = false,
    maxLines: Int = Int.MAX_VALUE,
    textAlign: TextAlign? = null,
) {
    val c = color ?: (if (second) LocalInk2.current ?: Ds.c.ink2 else LocalInk.current ?: Ds.c.ink)
    Text(text, modifier, color = c, style = style, maxLines = maxLines, overflow = if (maxLines == Int.MAX_VALUE) TextOverflow.Clip else TextOverflow.Ellipsis, textAlign = textAlign)
}

// ---- motion helpers -----------------------------------------------------------------------------------------------------

/** Scale a little while pressed, back with a spring when let go. */
@Composable
fun Modifier.pressScale(source: MutableInteractionSource, scale: Float = AutoDjMotion.pressScale): Modifier {
    val pressed by source.collectIsPressedAsState()
    val s by animateFloatAsState(
        if (pressed && !Ds.reduceMotion) scale else 1f,
        if (pressed) tween(AutoDjMotion.instant) else AutoDjMotion.pressSpring, label = "press",
    )
    return graphicsLayer {
        scaleX = s
        scaleY = s
    }
}

/**
 * The tile rises 16 dp and fades in, [index] steps after the first (45 ms each, at most 8). Runs once, when the element first
 * appears; with reduced motion it is simply there.
 */
@Composable
fun Modifier.riseIn(index: Int = 0): Modifier {
    val t = remember { Animatable(if (Ds.reduceMotion) 1f else 0f) }
    LaunchedEffect(Unit) {
        if (t.value < 1f) t.animateTo(1f, tween(AutoDjMotion.enter, delayMillis = AutoDjMotion.stagger * index.coerceIn(0, 8), easing = AutoDjMotion.out))
    }
    val rise = with(LocalDensity.current) { AutoDjMotion.rise.dp.toPx() }
    return graphicsLayer {
        alpha = t.value
        translationY = (1f - t.value) * rise
    }
}

// ---- tile ---------------------------------------------------------------------------------------------------------------

enum class TileKind { Plain, Accent, Ink, Screen }

/** Every surface of the board. [pulse] lets the tone breathe with the beat (transport BPM, deck head). */
@Composable
fun Tile(
    modifier: Modifier = Modifier,
    kind: TileKind = TileKind.Plain,
    padding: Dp = AutoDjSpace.tilePad,
    pulse: Boolean = false,
    content: @Composable ColumnScope.() -> Unit,
) {
    val c = Ds.c
    val base = when (kind) {
        TileKind.Plain -> c.tile
        TileKind.Accent -> c.accent
        TileKind.Ink -> c.ink
        TileKind.Screen -> c.screen
    }
    val text = when (kind) {
        TileKind.Plain -> c.ink
        TileKind.Accent -> c.onAccent
        TileKind.Ink -> c.onInk
        TileKind.Screen -> c.screenInk
    }
    val text2 = when (kind) {
        TileKind.Plain -> c.ink2
        TileKind.Accent -> c.onAccent.copy(alpha = .8f)
        TileKind.Ink -> c.onInk.copy(alpha = .7f)
        TileKind.Screen -> c.screenDim
    }
    val shade = if (pulse && kind == TileKind.Plain) {
        Modifier.drawBehind { drawRect(pulseTile(base, c.ink, Beat.pulse)) }
    } else {
        Modifier.background(base)
    }
    CompositionLocalProvider(LocalInk provides text, LocalInk2 provides text2) {
        Column(modifier.clip(AutoDjShape).then(shade).padding(padding), content = content)
    }
}

/** A display: a screen-coloured tile without padding; its content runs to the edge. */
@Composable
fun Display(modifier: Modifier = Modifier, content: @Composable BoxScope.() -> Unit) {
    val c = Ds.c
    CompositionLocalProvider(LocalInk provides c.screenInk, LocalInk2 provides c.screenDim) {
        Box(modifier.clip(AutoDjShape).background(c.screen), content = content)
    }
}

// ---- buttons ------------------------------------------------------------------------------------------------------------

/** 44 dp (small: 36 dp) button. [on] fills it with ink, [primary] with the accent (one per tile at most), [danger] with the warning red. */
@Composable
fun Btn(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    on: Boolean = false,
    primary: Boolean = false,
    danger: Boolean = false,
    enabled: Boolean = true,
    small: Boolean = false,
    style: TextStyle = AutoDjType.body,
    description: String? = null,
) {
    val c = Ds.c
    val source = remember { MutableInteractionSource() }
    val hovered by source.collectIsHoveredAsState()
    val fill by animateColorAsState(
        when {
            primary -> c.accent
            danger -> c.bad
            on -> c.ink
            hovered && enabled -> c.tile3
            else -> c.tile2
        },
        tween(AutoDjMotion.ms(AutoDjMotion.fast)), label = "btn",
    )
    val textColor = when {
        primary -> c.onAccent
        danger -> c.onAccent
        on -> c.onInk
        else -> c.ink
    }
    Box(
        modifier
            .heightWithMinimum(if (small) AutoDjSpace.controlSm else AutoDjSpace.control)
            .pressScale(source)
            .clip(AutoDjShape)
            .background(fill)
            .hoverable(source)
            .clickable(interactionSource = source, indication = null, enabled = enabled, role = Role.Button) { onClick() }
            .then(if (description != null) Modifier.semantics { contentDescription = description } else Modifier)
            .padding(horizontal = if (small) 12.dp else 16.dp),
        contentAlignment = Alignment.Center,
    ) { Txt(text, style = style, color = if (enabled) textColor else textColor.copy(alpha = .4f), maxLines = 1) }
}

private fun Modifier.heightWithMinimum(h: Dp) = this.then(Modifier.height(h))

// ---- sliding selection: segments and navigation --------------------------------------------------------------------------

/**
 * Entries in a row with one fill behind the selected one. The fill glides to the new entry in 160 ms (the tab bar uses 260 ms).
 * [track] is the colour of the strip behind (null = none).
 */
@Composable
private fun SlidingChoice(
    options: List<String>,
    selected: Int,
    onSelect: (Int) -> Unit,
    modifier: Modifier,
    itemHeight: Dp,
    track: Color?,
    fill: Color,
    onFill: Color,
    off: Color,
    durationMs: Int,
    textStyle: TextStyle,
    equalWidth: Boolean,
    inset: Dp,
    itemPadding: Dp,
) {
    val xs = remember(options.size) { mutableStateListOf<Int>().apply { repeat(options.size) { add(0) } } }
    val ws = remember(options.size) { mutableStateListOf<Int>().apply { repeat(options.size) { add(0) } } }
    val sel = selected.coerceIn(0, (options.size - 1).coerceAtLeast(0))
    val x by animateFloatAsState(xs.getOrElse(sel) { 0 }.toFloat(), tween(AutoDjMotion.ms(durationMs), easing = AutoDjMotion.out), label = "slide-x")
    val w by animateFloatAsState(ws.getOrElse(sel) { 0 }.toFloat(), tween(AutoDjMotion.ms(durationMs), easing = AutoDjMotion.out), label = "slide-w")
    Box(modifier.clip(AutoDjShape).then(if (track != null) Modifier.background(track) else Modifier).padding(inset)) {
        Box(
            Modifier
                .offset { IntOffset(x.roundToInt(), 0) }
                .width(with(LocalDensity.current) { w.toDp() })
                .height(itemHeight)
                .clip(AutoDjShape)
                .background(fill),
        )
        Row {
            options.forEachIndexed { i, text ->
                val on = i == sel
                val color by animateColorAsState(if (on) onFill else off, tween(AutoDjMotion.ms(AutoDjMotion.fast)), label = "seg-text")
                Box(
                    (if (equalWidth) Modifier.weight(1f) else Modifier)
                        .height(itemHeight)
                        .onGloballyPositioned {
                            val p = it.positionInParent()
                            if (xs[i] != p.x.roundToInt()) xs[i] = p.x.roundToInt()
                            if (ws[i] != it.size.width) ws[i] = it.size.width
                        }
                        .clip(AutoDjShape)
                        .semantics { semSelected = on }
                        .clickable(indication = null, interactionSource = remember { MutableInteractionSource() }, role = Role.RadioButton) { onSelect(i) }
                        .padding(horizontal = itemPadding),
                    contentAlignment = Alignment.Center,
                ) { Txt(text, style = textStyle, color = color, maxLines = 1) }
            }
        }
    }
}

/** A tile-2 block with 2 dp inset; the selected entry is filled with ink. Replaces every group of pills. */
@Composable
fun Segment(options: List<String>, selected: Int, onSelect: (Int) -> Unit, modifier: Modifier = Modifier, equalWidth: Boolean = false, small: Boolean = false, compact: Boolean = false) {
    val c = Ds.c
    SlidingChoice(
        options, selected, onSelect, modifier, itemHeight = if (small) 32.dp else 36.dp, track = c.tile2, fill = c.ink, onFill = c.onInk, off = c.ink,
        durationMs = AutoDjMotion.fast, textStyle = if (compact) AutoDjType.small else AutoDjType.body, equalWidth = equalWidth, inset = 2.dp, itemPadding = if (compact) 9.dp else 14.dp,
    )
}

/** The text tabs of the top bar: the active page is an ink block that glides across in 260 ms. */
@Composable
fun NavTabs(options: List<String>, selected: Int, onSelect: (Int) -> Unit, modifier: Modifier = Modifier, equalWidth: Boolean = false, textStyle: TextStyle = AutoDjType.body) {
    val c = Ds.c
    SlidingChoice(
        options, selected, onSelect, modifier.background(c.tile), itemHeight = 40.dp, track = null, fill = c.ink, onFill = c.onInk, off = c.ink,
        durationMs = AutoDjMotion.base, textStyle = textStyle, equalWidth = equalWidth, inset = 8.dp, itemPadding = if (equalWidth) 2.dp else 16.dp,
    )
}

// ---- beat dots ----------------------------------------------------------------------------------------------------------

/** Four squares: the running beat is green, the downbeat of the bar ink. Counter and pulse anchor in one. */
@Composable
fun BeatDots(modifier: Modifier = Modifier, size: Dp = 14.dp) {
    val c = Ds.c
    Row(modifier.semantics { contentDescription = "Schlag ${Beat.index + 1} von 4" }, horizontalArrangement = Arrangement.spacedBy(5.dp)) {
        for (i in 0 until 4) {
            val lit = Beat.playing && Beat.index == i
            val target = when {
                lit && i == 0 -> c.ink
                lit -> c.accent
                else -> c.tile3
            }
            val color by animateColorAsState(target, tween(AutoDjMotion.ms(AutoDjMotion.instant)), label = "dot")
            Box(Modifier.size(size).clip(androidx.compose.foundation.shape.RoundedCornerShape(3.dp)).background(color))
        }
    }
}

// ---- chips --------------------------------------------------------------------------------------------------------------

/** A camelot key as data colour. */
@Composable
fun KeyChip(camelot: String, modifier: Modifier = Modifier) {
    Box(
        modifier.height(24.dp).clip(androidx.compose.foundation.shape.RoundedCornerShape(4.dp)).background(camelotChip(camelot)).padding(horizontal = 8.dp),
        contentAlignment = Alignment.Center,
    ) { Txt(camelot, style = AutoDjType.small, color = Color(0xFF2A1D17), maxLines = 1) }
}

enum class StatusKind { Good, Warn, Bad, Idle }

/** A 24 dp status mark: a filled square and a short word. */
@Composable
fun StatusTag(kind: StatusKind, text: String, modifier: Modifier = Modifier) {
    val c = Ds.c
    val color = when (kind) {
        StatusKind.Good -> c.accent
        StatusKind.Warn -> c.warn
        StatusKind.Bad -> c.bad
        StatusKind.Idle -> c.tile3
    }
    Row(modifier.height(24.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        Box(Modifier.size(10.dp).clip(androidx.compose.foundation.shape.RoundedCornerShape(2.dp)).background(color))
        Txt(text, style = AutoDjType.small, second = true, maxLines = 1)
    }
}

// ---- faders -------------------------------------------------------------------------------------------------------------

/** Horizontal bar fader, 28 dp tall; the whole height is the target. Bipolar: filled from the middle, with a mark at the centre. */
@Composable
fun BarFader(
    value: Float,
    onChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
    bipolar: Boolean = false,
    label: String = "Regler",
    valueText: String = "",
    onReset: (() -> Unit)? = null,
    height: Dp = 28.dp,
    fillColor: Color? = null,
) {
    val c = Ds.c
    val current by rememberUpdatedState(onChange)
    val v = value.coerceIn(0f, 1f)
    val shown by animateFloatAsState(v, tween(AutoDjMotion.ms(AutoDjMotion.instant)), label = "fader")
    Canvas(
        modifier
            .height(height)
            .semantics(mergeDescendants = true) {
                contentDescription = if (valueText.isEmpty()) label else "$label, $valueText"
                progressBarRangeInfo = ProgressBarRangeInfo(v, 0f..1f)
                setProgress { target ->
                    current(target.coerceIn(0f, 1f))
                    true
                }
            }
            .pointerInput(Unit) {
                detectDragGestures(
                    onDragStart = { current((it.x / size.width).coerceIn(0f, 1f)) },
                    onDrag = { change, _ ->
                        change.consume()
                        current((change.position.x / size.width).coerceIn(0f, 1f))
                    },
                )
            }
            .pointerInput(onReset) {
                detectTapGestures(onDoubleTap = { onReset?.invoke() }, onTap = { current((it.x / size.width).coerceIn(0f, 1f)) })
            },
    ) {
        val r = CornerRadius(6.dp.toPx())
        drawRoundRect(c.tile2, size = size, cornerRadius = r)
        val ink = fillColor ?: c.ink
        if (bipolar) {
            val mid = size.width / 2f
            val x = shown * size.width
            drawRoundRect(ink, Offset(minOf(mid, x), 0f), Size(kotlin.math.abs(x - mid), size.height), r)
            drawRect(c.ink2, Offset(mid - 2.dp.toPx(), size.height * 0.25f), Size(4.dp.toPx(), size.height * 0.5f))
        } else {
            drawRoundRect(ink, Offset.Zero, Size(shown * size.width, size.height), r)
        }
    }
}

/** Vertical fader with a cap, 40 dp wide (EQ, level). [killed] turns the cap warning red. */
@Composable
fun CapFader(
    value: Float,
    onChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
    label: String = "Regler",
    valueText: String = "",
    killed: Boolean = false,
    onReset: (() -> Unit)? = null,
    width: Dp = 40.dp,
) {
    val c = Ds.c
    val current by rememberUpdatedState(onChange)
    val v = value.coerceIn(0f, 1f)
    val shown by animateFloatAsState(v, tween(AutoDjMotion.ms(AutoDjMotion.instant)), label = "cap")
    val cap by animateColorAsState(if (killed) c.bad else c.ink, tween(AutoDjMotion.ms(AutoDjMotion.fast)), label = "cap-color")
    Canvas(
        modifier
            .width(width)
            .semantics(mergeDescendants = true) {
                contentDescription = if (valueText.isEmpty()) label else "$label, $valueText"
                progressBarRangeInfo = ProgressBarRangeInfo(v, 0f..1f)
                setProgress { target ->
                    current(target.coerceIn(0f, 1f))
                    true
                }
            }
            .pointerInput(Unit) {
                val capH = 18.dp.toPx()
                fun at(y: Float) = (1f - ((y - capH / 2f) / (size.height - capH))).coerceIn(0f, 1f)
                detectDragGestures(
                    onDragStart = { current(at(it.y)) },
                    onDrag = { change, _ ->
                        change.consume()
                        current(at(change.position.y))
                    },
                )
            }
            .pointerInput(onReset) { detectTapGestures(onDoubleTap = { onReset?.invoke() }) },
    ) {
        val r = CornerRadius(6.dp.toPx())
        drawRoundRect(c.tile2, size = size, cornerRadius = r)
        val capH = 18.dp.toPx()
        val y = (1f - shown) * (size.height - capH)
        drawRoundRect(cap, Offset(0f, y), Size(size.width, capH), CornerRadius(4.dp.toPx()))
    }
}

/** A thin level strip, 8 dp wide, filled from the bottom; it follows the audio linearly, without easing. */
@Composable
fun LevelStrip(level: Float, modifier: Modifier = Modifier) {
    val c = Ds.c
    Canvas(modifier.width(8.dp)) {
        drawRoundRect(c.tile2, size = size, cornerRadius = CornerRadius(3.dp.toPx()))
        val h = size.height * level.coerceIn(0f, 1f)
        drawRoundRect(c.accent, Offset(0f, size.height - h), Size(size.width, h), CornerRadius(3.dp.toPx()))
    }
}

// ---- pads ---------------------------------------------------------------------------------------------------------------

/** A pad: group colour, name top left, key top right. Pressing brightens it at once (+35 %) and shrinks it a little; letting go springs back. */
@Composable
fun Pad(
    name: String,
    key: String,
    color: Color,
    onColor: Color,
    onPress: () -> Unit,
    onRelease: () -> Unit,
    modifier: Modifier = Modifier,
    lit: Boolean = false,
) {
    val press by rememberUpdatedState(onPress)
    val release by rememberUpdatedState(onRelease)
    var down by remember { mutableStateOf(false) }
    val glow by animateFloatAsState(if (down || lit) 1f else 0f, if (down || lit) tween(0) else tween(AutoDjMotion.ms(180), easing = AutoDjMotion.out), label = "pad-glow")
    val scale by animateFloatAsState(if (down && !Ds.reduceMotion) .965f else 1f, if (down) tween(AutoDjMotion.instant) else AutoDjMotion.pressSpring, label = "pad-scale")
    Box(
        modifier
            .graphicsLayer {
                scaleX = scale
                scaleY = scale
            }
            .clip(AutoDjShape)
            .drawBehind { drawRect(lerp(color, Color.White, glow * .35f)) }
            .semantics {
                role = Role.Button
                contentDescription = "Pad $name"
                onClick(label = "spielen") {
                    press()
                    release()
                    true
                }
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
            }
            .padding(12.dp),
    ) {
        Txt(name, style = AutoDjType.row, color = onColor, maxLines = 1, modifier = Modifier.align(Alignment.TopStart))
        if (key.isNotEmpty()) Txt(key, style = AutoDjType.small, color = onColor.copy(alpha = .7f), modifier = Modifier.align(Alignment.TopEnd))
    }
}

/** A large hold surface (beat roll, stutter): active while pressed. */
@Composable
fun HoldSurface(
    title: String,
    color: Color,
    onColor: Color,
    onPress: () -> Unit,
    onRelease: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val press by rememberUpdatedState(onPress)
    val release by rememberUpdatedState(onRelease)
    var down by remember { mutableStateOf(false) }
    val glow by animateFloatAsState(if (down) 1f else 0f, if (down) tween(0) else tween(AutoDjMotion.ms(180), easing = AutoDjMotion.out), label = "hold-glow")
    val scale by animateFloatAsState(if (down && !Ds.reduceMotion) .985f else 1f, if (down) tween(AutoDjMotion.instant) else AutoDjMotion.pressSpring, label = "hold-scale")
    Box(
        modifier
            .graphicsLayer {
                scaleX = scale
                scaleY = scale
            }
            .clip(AutoDjShape)
            .drawBehind { drawRect(lerp(color, Color.White, glow * .30f)) }
            .semantics {
                role = Role.Button
                contentDescription = "$title halten"
                onClick(label = "kurz auslösen") {
                    press()
                    release()
                    true
                }
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
            }
            .padding(16.dp),
    ) {
        Txt(title, style = AutoDjType.lead.copy(fontSize = androidx.compose.ui.unit.TextUnit(28f, androidx.compose.ui.unit.TextUnitType.Sp)), color = onColor, modifier = Modifier.align(Alignment.BottomStart))
    }
}

/** A progress bar, 6 dp tall: [fraction] of the track filled; null = idle. Colours default to ink on tile 2. */
@Composable
fun ProgressBar(fraction: Float, modifier: Modifier = Modifier, track: Color? = null, fill: Color? = null) {
    val c = Ds.c
    val f by animateFloatAsState(fraction.coerceIn(0f, 1f), tween(AutoDjMotion.ms(AutoDjMotion.instant)), label = "progress")
    Canvas(modifier.height(6.dp).fillMaxWidth().semantics { progressBarRangeInfo = ProgressBarRangeInfo(fraction.coerceIn(0f, 1f), 0f..1f) }) {
        drawRoundRect(track ?: c.tile2, size = size, cornerRadius = CornerRadius(3.dp.toPx()))
        drawRoundRect(fill ?: c.ink, size = Size(size.width * f, size.height), cornerRadius = CornerRadius(3.dp.toPx()))
    }
}

/** A tile with a title and an optional right-hand action, the usual head of a card. */
@Composable
fun TileTitle(title: String, modifier: Modifier = Modifier, subtitle: String? = null, trailing: @Composable () -> Unit = {}) {
    Row(modifier.fillMaxWidth(), verticalAlignment = Alignment.Top) {
        Column(Modifier.weight(1f)) {
            Txt(title, style = AutoDjType.lead)
            if (subtitle != null) Txt(subtitle, style = AutoDjType.small, second = true, modifier = Modifier.padding(top = 2.dp))
        }
        trailing()
    }
}
