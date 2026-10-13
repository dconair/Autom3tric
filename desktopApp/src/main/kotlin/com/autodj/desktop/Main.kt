package com.autodj.desktop

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.DpSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.FrameWindowScope
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import androidx.compose.ui.window.rememberWindowState
import com.autodj.app.dj.DjColors
import com.autodj.app.ui.AutoDjTheme
import com.autodj.app.ui.InstrumentScreen
import com.autodj.app.ui.dj.DecksScreen
import com.autodj.app.ui.dj.DjTabBar
import com.autodj.app.ui.dj.EffectsScreen
import com.autodj.app.ui.dj.LibraryScreen
import com.autodj.app.ui.dj.MixScreen
import com.autodj.app.ui.dj.PadsScreen
import kotlinx.coroutines.delay
import java.awt.FileDialog
import java.io.File
import java.util.Locale

private val Tabs = listOf("Bibliothek", "Auto-Mix", "Decks", "Effekte", "Pads", "Granular")

fun main() = application {
    val host = remember {
        DesktopHost().also { it.startAudio() }
    }
    Window(
        onCloseRequest = {
            host.close()
            exitApplication()
        },
        title = "AutoDJ",
        state = rememberWindowState(size = DpSize(1180.dp, 800.dp)),
    ) {
        AutoDjTheme { DesktopApp(host) }
    }
}

/** The native "save as" dialog for the exported mix; returns the chosen absolute path or null. */
private fun FrameWindowScope.pickExportFile(): String? {
    val dialog = FileDialog(window, "Mix als WAV speichern", FileDialog.SAVE)
    dialog.file = "AutoDJ-Mix.wav"
    dialog.isVisible = true
    val name = dialog.file ?: return null
    val dir = dialog.directory ?: return null
    return File(dir, if (name.lowercase().endsWith(".wav")) name else "$name.wav").absolutePath
}

/** The native "Songs hinzufügen" dialog (multi-select). Blocks until the user is done; returns absolute paths. */
private fun FrameWindowScope.pickAudioFiles(): List<String> {
    val dialog = FileDialog(window, "Songs hinzufügen", FileDialog.LOAD)
    dialog.isMultipleMode = true
    dialog.setFilenameFilter { _, name -> name.substringAfterLast('.', "").lowercase() in AudioExtensions }
    dialog.isVisible = true
    return dialog.files.orEmpty().map(File::getAbsolutePath)
}

private val AudioExtensions = setOf("mp3", "wav", "wave", "flac", "ogg", "oga", "m4a", "aac", "aif", "aiff", "caf", "mp4")

@Composable
private fun FrameWindowScope.DesktopApp(host: DesktopHost) {
    var tab by remember { mutableIntStateOf(0) }
    LaunchedEffect(host) {
        while (true) {
            delay(33)
            host.poll()
        }
    }
    Column(Modifier.fillMaxSize().background(DjColors.bg)) {
        DjTabBar(Tabs, tab, { tab = it }) { StatusChip(host) }
        Box(Modifier.weight(1f)) {
            when (tab) {
                0 -> LibraryScreen(
                    host.dj,
                    onPickFiles = { host.dj.importSources(pickAudioFiles()) },
                    onGranular = { item ->
                        host.dj.sendToGranular(item) { slot, error ->
                            if (slot != null) {
                                host.engine.spec("granular.v0.source")?.let { host.setParam(it, slot.toFloat()) }
                                tab = 5
                            } else {
                                host.report(error ?: "Konnte den Song nicht laden")
                            }
                        }
                    },
                    onLoadDeck = { item, deck ->
                        host.dj.decks.load(deck, item)
                        tab = 2
                    },
                    onQueue = { item -> host.dj.mix.add(item.id) },
                    isQueued = { item -> host.dj.mix.contains(item.id) },
                )
                1 -> MixScreen(
                    host.dj,
                    onGoToLibrary = { tab = 0 },
                    onExport = { bits -> pickExportFile()?.let { host.dj.mix.export(it, bits) } },
                )
                2 -> DecksScreen(host.dj, onGoToLibrary = { tab = 0 })
                3 -> EffectsScreen(host.dj, onCaptureToGranular = { seconds ->
                    host.dj.captureToGranular(seconds) { slot, error ->
                        if (slot != null) {
                            host.engine.spec("granular.v0.source")?.let { host.setParam(it, slot.toFloat()) }
                            tab = 5
                        } else {
                            host.report(error ?: "Konnte nichts einfangen")
                        }
                    }
                })
                4 -> PadsScreen(host.dj)
                else -> InstrumentScreen(host)
            }
        }
        val m = host.message
        if (m != null) {
            Text(m, color = DjColors.warn, fontSize = 12.sp, modifier = Modifier.fillMaxWidth().background(DjColors.panel).padding(horizontal = 16.dp, vertical = 6.dp))
        }
    }
}

@Composable
private fun StatusChip(host: DesktopHost) {
    val d = host.driver
    val s = host.status
    val line = if (d.running) {
        String.format(Locale.US, "%s · %d Hz · %.1f ms · CPU %.0f %%", d.api, d.sampleRate, d.latencyMs, s.load * 100.0)
    } else {
        "Audio aus"
    }
    Text(line, color = DjColors.muted, fontSize = 12.sp, modifier = Modifier.padding(end = 8.dp))
}
