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
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.height
import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.tween
import androidx.compose.runtime.mutableStateOf
import com.autodj.app.design.*
import com.autodj.app.ui.dj.clockInfo
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
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

private enum class Tab { Library, Mix, Decks, Effects, Pads, Granular, Debug }

/** Phone navigation labels (the same six pages as on the desktop, short names). */
private val NavLabels = listOf("Songs", "Mix", "Decks", "FX", "Pads", "Granular")

/** The page behind "Mehr": guide, day or night, calm motion, developer tools. */
@Composable
private fun MoreTile(onGuide: () -> Unit, onDeveloper: () -> Unit, modifier: Modifier = Modifier) {
    Tile(modifier.fillMaxWidth()) {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
            Segment(listOf("Tag", "Nacht"), if (Ds.night) 1 else 0, { Ds.night = it == 1 })
            Btn("Bewegung reduzieren", onClick = { Ds.reduceMotion = !Ds.reduceMotion }, on = Ds.reduceMotion, style = AutoDjType.small)
        }
        Row(Modifier.padding(top = 8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Btn("Anleitung", onClick = onGuide)
            Btn("Entwickler", onClick = onDeveloper)
        }
    }
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
    var more by rememberSaveable { mutableStateOf(false) }
    CompositionLocalProvider(LocalGuide provides guide) {
    Box(Modifier.fillMaxSize()) {
    Column(Modifier.fillMaxSize().background(Ds.c.ground).statusBarsPadding()) {
        val (bpm, _) = clockInfo(vm.dj)
        PhoneTopBar(
            bpm, audioOn = vm.running, onAudio = { vm.toggleAudio() },
            modifier = Modifier.padding(AutoDjSpace.phoneMargin),
            trailing = { Btn(if (more) "Weniger" else "Mehr", onClick = { more = !more }, on = more, modifier = Modifier.height(48.dp)) },
        )
        Column(Modifier.animateContentSize(tween(AutoDjMotion.ms(AutoDjMotion.base), easing = AutoDjMotion.out))) {
            if (more) MoreTile(
                onGuide = { more = false; guide.show() },
                onDeveloper = { more = false; tab = Tab.Debug.ordinal },
                modifier = Modifier.padding(horizontal = AutoDjSpace.phoneMargin).padding(bottom = AutoDjSpace.phoneGap),
            )
            vm.message?.let { msg ->
                Tile(Modifier.fillMaxWidth().padding(horizontal = AutoDjSpace.phoneMargin).padding(bottom = AutoDjSpace.phoneGap), padding = 10.dp) {
                    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Txt(msg, style = AutoDjType.small, modifier = Modifier.weight(1f))
                        Btn("OK", onClick = { vm.message = null }, small = true)
                    }
                }
            }
        }
        Box(Modifier.weight(1f).fillMaxWidth()) {
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
        if (tab == Tab.Debug.ordinal) {
            Tile(Modifier.fillMaxWidth().navigationBarsPadding().padding(AutoDjSpace.phoneMargin), padding = 8.dp) {
                Btn("Zurück zur App", onClick = { tab = Tab.Library.ordinal }, modifier = Modifier.fillMaxWidth())
            }
        } else {
            PhoneBottomNav(NavLabels, tab, { tab = it }, Modifier.navigationBarsPadding().padding(AutoDjSpace.phoneMargin))
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
