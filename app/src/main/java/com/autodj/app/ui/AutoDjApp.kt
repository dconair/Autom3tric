package com.autodj.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autodj.app.AutoDjViewModel
import com.autodj.app.ui.dj.DecksScreen
import com.autodj.app.ui.dj.EffectsScreen
import com.autodj.app.ui.dj.LibraryScreen
import com.autodj.app.ui.dj.MixScreen
import com.autodj.app.ui.dj.PadsScreen
import java.util.Locale

private enum class Tab(val label: String, val glyph: String) {
    Library("Bibliothek", "≣"),
    Mix("Auto-Mix", "⇄"),
    Decks("Decks", "◉"),
    Effects("Effekte", "✦"),
    Pads("Pads", "▦"),
    Granular("Granular", "◌"),
    Debug("Entwickler", "⚙"),
}

@Composable
fun AutoDjApp(
    vm: AutoDjViewModel,
    onPickFiles: () -> Unit = {},
    onPickGranularFiles: () -> Unit = {},
    onExport: (bits: Int) -> Unit = {},
    guideSeen: Boolean = true,
    onGuideSeen: () -> Unit = {},
) {
    var tab by rememberSaveable { mutableIntStateOf(0) }
    val guide = remember { GuideState(guideSeen, onGuideSeen) }
    var beatRequest by rememberSaveable { mutableIntStateOf(0) }
    CompositionLocalProvider(LocalGuide provides guide) {
    Box(Modifier.fillMaxSize()) {
    Scaffold(
        containerColor = MaterialTheme.colorScheme.background,
        topBar = { TopBar(vm, onGuide = { guide.show() }) },
        bottomBar = {
            NavigationBar(containerColor = MaterialTheme.colorScheme.surface) {
                Tab.entries.forEachIndexed { i, t ->
                    NavigationBarItem(
                        selected = tab == i,
                        onClick = { tab = i },
                        icon = { Text(t.glyph, fontSize = 20.sp) },
                        label = { Text(t.label) },
                    )
                }
            }
        },
    ) { padding ->
        Box(Modifier.fillMaxSize().padding(padding)) {
            when (Tab.entries[tab]) {
                Tab.Library -> LibraryScreen(
                    vm.dj,
                    onPickFiles = onPickFiles,
                    onGranular = { item -> vm.sendToGranular(item) { ok -> if (ok) tab = Tab.Granular.ordinal } },
                    onLoadDeck = { item, deck ->
                        vm.dj.decks.load(deck, item)
                        tab = Tab.Decks.ordinal
                    },
                    onQueue = { item -> vm.dj.mix.add(item.id) },
                    isQueued = { item -> vm.dj.mix.contains(item.id) },
                )
                Tab.Decks -> DecksScreen(vm.dj, onGoToLibrary = { tab = Tab.Library.ordinal })
                Tab.Effects -> EffectsScreen(vm.dj, onCaptureToGranular = { seconds ->
                    vm.captureToGranular(seconds) { ok -> if (ok) tab = Tab.Granular.ordinal }
                })
                Tab.Pads -> PadsScreen(vm.dj)
                Tab.Mix -> MixScreen(vm.dj, onGoToLibrary = { tab = Tab.Library.ordinal }, onExport = onExport)
                Tab.Granular -> InstrumentScreen(
                    remember(vm) { vm.asInstrumentHost(onPickGranularFiles) },
                    onShowGuide = { guide.show(Guides.PLUGIN) }, beatRequest = beatRequest,
                )
                Tab.Debug -> DeveloperTools(vm)
            }
        }
    }
    GuideOverlay(guide, onAction = { target ->
        when (target) {
            GuideTarget.LoadDemoSongs -> {
                vm.dj.addDemoTracks()
                tab = Tab.Library.ordinal
            }
            GuideTarget.Library -> tab = Tab.Library.ordinal
            GuideTarget.AutoMix -> tab = Tab.Mix.ordinal
            GuideTarget.Decks -> tab = Tab.Decks.ordinal
            GuideTarget.Effects -> tab = Tab.Effects.ordinal
            GuideTarget.Pads -> tab = Tab.Pads.ordinal
            GuideTarget.Plugin -> tab = Tab.Granular.ordinal
            GuideTarget.PluginLoadFile -> {
                tab = Tab.Granular.ordinal
                onPickGranularFiles()
            }
            GuideTarget.PluginBeatStart -> {
                tab = Tab.Granular.ordinal
                beatRequest += 1
            }
        }
    })
    }
    }
}

/** The early engine screens (all parameters, transport) and the debug page, behind one tab. */
@Composable
private fun DeveloperTools(vm: AutoDjViewModel) {
    var page by rememberSaveable { mutableIntStateOf(0) }
    val pages = listOf("Debug", "Parameter", "Transport")
    Column(Modifier.fillMaxSize()) {
        Row(Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 6.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            pages.forEachIndexed { i, label ->
                if (i == page) Button(onClick = { page = i }) { Text(label) } else TextButton(onClick = { page = i }) { Text(label) }
            }
        }
        Box(Modifier.weight(1f)) {
            when (page) {
                0 -> DebugScreen(vm)
                1 -> GranularScreen(vm)
                else -> TransportScreen(vm)
            }
        }
    }
}

@Composable
private fun TopBar(vm: AutoDjViewModel, onGuide: () -> Unit) {
    Column(Modifier.statusBarsPadding().background(MaterialTheme.colorScheme.background)) {
        Row(
            Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Button(
                onClick = { vm.toggleAudio() },
                colors = if (vm.running) ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error) else ButtonDefaults.buttonColors(),
            ) { Text(if (vm.running) "Stop audio" else "Start audio") }
            DebugChip(vm)
            Spacer(Modifier.weight(1f))
            TextButton(onClick = onGuide) { Text("? Anleitung") }
        }
        vm.message?.let { msg ->
            Row(
                Modifier.fillMaxWidth().padding(horizontal = 16.dp).clip(RoundedCornerShape(8.dp))
                    .background(MaterialTheme.colorScheme.surfaceVariant).padding(start = 12.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(msg, Modifier.weight(1f), style = MaterialTheme.typography.bodySmall)
                TextButton(onClick = { vm.message = null }) { Text("OK") }
            }
        }
    }
}

/** Always-visible health chip: CPU load of the audio callback, xruns, active grains, adaptive-quality level, latency. */
@Composable
private fun DebugChip(vm: AutoDjViewModel) {
    val s = vm.status
    val d = vm.driver
    val load = s.load
    val color = when {
        !vm.running -> Palette.muted
        load < 0.5f && s.qualityLevel == 0 -> Palette.good
        load < 0.8f -> Palette.warn
        else -> Palette.bad
    }
    val lat = if (d.latencyMs >= 0) String.format(Locale.US, "%.0f ms", d.latencyMs) else "-"
    Box(
        Modifier.clip(RoundedCornerShape(50)).background(MaterialTheme.colorScheme.surfaceVariant).padding(horizontal = 12.dp, vertical = 6.dp),
    ) {
        Text(
            String.format(Locale.US, "CPU %.0f%% · xr %d · %d gr · Q%d · %s", 100 * load, s.xruns + s.deadlineMisses, s.activeGrains, s.qualityLevel, lat),
            color = color,
            fontSize = 12.sp,
        )
    }
}
