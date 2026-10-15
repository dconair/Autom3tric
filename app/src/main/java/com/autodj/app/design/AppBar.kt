package com.autodj.app.design

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import java.util.Locale

/** "120.0" for the BPM tile. */
fun bpmText(bpm: Double) = String.format(Locale.US, "%.1f", bpm)

/** The tile with the running tempo: a large number, and what drives it. It breathes with the beat. */
@Composable
fun BpmTile(bpm: Double, label: String, modifier: Modifier = Modifier, phone: Boolean = false) {
    val h = if (phone) 48.dp else AutoDjSpace.bar
    Tile(modifier.height(h), padding = 0.dp, pulse = true) {
        Row(
            Modifier.height(h).padding(horizontal = if (phone) 12.dp else 16.dp).widthIn(min = if (phone) 64.dp else 150.dp),
            verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Txt(bpmText(bpm), style = if (phone) AutoDjType.lead else AutoDjType.head, maxLines = 1)
            if (!phone) {
                Column {
                    Txt("BPM", style = AutoDjType.small, second = true, maxLines = 1)
                    Txt(label, style = AutoDjType.small, second = true, maxLines = 1)
                }
            }
        }
    }
}

/**
 * The top bar of the desktop window (56 dp): name tile, tab tile, then on the right the beat dots, the BPM tile and the audio action.
 * Everything is a tile of the board, there is no separate bar surface.
 */
@Composable
fun DesktopTopBar(
    tabs: List<String>,
    selected: Int,
    onSelect: (Int) -> Unit,
    bpm: Double,
    bpmLabel: String,
    audioOn: Boolean,
    onAudio: () -> Unit,
    modifier: Modifier = Modifier,
    trailing: @Composable () -> Unit = {},
) {
    Row(modifier.fillMaxWidth().height(AutoDjSpace.bar), horizontalArrangement = Arrangement.spacedBy(AutoDjSpace.gap), verticalAlignment = Alignment.CenterVertically) {
        Tile(Modifier.height(AutoDjSpace.bar), padding = 0.dp) {
            Box(Modifier.height(AutoDjSpace.bar).padding(horizontal = 16.dp), contentAlignment = Alignment.Center) { Txt("Auto DJ", style = AutoDjType.lead, maxLines = 1) }
        }
        Tile(Modifier.height(AutoDjSpace.bar), padding = 0.dp) { NavTabs(tabs, selected, onSelect, Modifier.height(AutoDjSpace.bar)) }
        Box(Modifier.weight(1f))
        trailing()
        Tile(Modifier.height(AutoDjSpace.bar), padding = 0.dp) {
            Box(Modifier.height(AutoDjSpace.bar).padding(horizontal = 16.dp), contentAlignment = Alignment.Center) { BeatDots() }
        }
        BpmTile(bpm, bpmLabel)
        Btn(if (audioOn) "Audio an" else "Audio aus", onClick = onAudio, primary = audioOn, modifier = Modifier.height(AutoDjSpace.bar), style = AutoDjType.bodyMedium)
    }
}

/** The phone's top bar: name, BPM and the audio action. */
@Composable
fun PhoneTopBar(bpm: Double, audioOn: Boolean, onAudio: () -> Unit, modifier: Modifier = Modifier, trailing: @Composable () -> Unit = {}) {
    Row(modifier.fillMaxWidth().height(48.dp), horizontalArrangement = Arrangement.spacedBy(AutoDjSpace.phoneGap), verticalAlignment = Alignment.CenterVertically) {
        Tile(Modifier.height(48.dp), padding = 0.dp) {
            Box(Modifier.height(48.dp).padding(horizontal = 14.dp), contentAlignment = Alignment.Center) { Txt("Auto DJ", style = AutoDjType.bodyMedium, maxLines = 1) }
        }
        Tile(Modifier.height(48.dp), padding = 0.dp) {
            Box(Modifier.height(48.dp).padding(horizontal = 12.dp), contentAlignment = Alignment.Center) { BeatDots(size = 12.dp) }
        }
        Box(Modifier.weight(1f))
        trailing()
        BpmTile(bpm, "", phone = true)
        Btn(if (audioOn) "Audio an" else "Audio aus", onClick = onAudio, primary = audioOn, modifier = Modifier.height(48.dp), style = AutoDjType.bodyMedium)
    }
}

/** The phone's bottom navigation: one tile with the six tab names, the active one an ink block. No icons. */
@Composable
fun PhoneBottomNav(tabs: List<String>, selected: Int, onSelect: (Int) -> Unit, modifier: Modifier = Modifier) {
    Tile(modifier.fillMaxWidth(), padding = 0.dp) {
        NavTabs(tabs, selected, onSelect, Modifier.fillMaxWidth(), equalWidth = true, textStyle = AutoDjType.small)
    }
}
