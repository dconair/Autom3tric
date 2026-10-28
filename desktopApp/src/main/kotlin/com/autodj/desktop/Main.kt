package com.autodj.desktop

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.draganddrop.dragAndDropTarget
import androidx.compose.foundation.clickable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.draw.clip
import androidx.compose.ui.semantics.Role
import com.autodj.app.ui.GuideOverlay
import com.autodj.app.ui.GuideState
import com.autodj.app.ui.GuideTarget
import com.autodj.app.ui.Guides
import com.autodj.app.ui.LocalGuide
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.draganddrop.DragAndDropEvent
import androidx.compose.ui.draganddrop.DragAndDropTarget
import androidx.compose.ui.draganddrop.DragData
import androidx.compose.ui.draganddrop.dragData
import androidx.compose.ui.unit.DpSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.FrameWindowScope
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import androidx.compose.ui.window.rememberWindowState
import com.autodj.app.ui.AutoDjTheme
import com.autodj.app.ui.InstrumentScreen
import com.autodj.app.ui.dj.DecksScreen
import com.autodj.app.ui.dj.clockInfo
import com.autodj.app.design.AutoDjSpace
import com.autodj.app.design.AutoDjType
import com.autodj.app.design.Btn
import com.autodj.app.design.DesktopTopBar
import com.autodj.app.design.Ds
import com.autodj.app.design.Segment
import com.autodj.app.design.Txt
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.height
import androidx.compose.ui.Alignment
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.KeyEvent
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.isAltPressed
import androidx.compose.ui.input.key.isCtrlPressed
import androidx.compose.ui.input.key.isMetaPressed
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.type
import com.autodj.app.ui.dj.EffectsScreen
import com.autodj.app.ui.dj.LibraryScreen
import com.autodj.app.ui.dj.MixScreen
import com.autodj.app.ui.dj.PadsScreen
import kotlinx.coroutines.delay
import java.awt.FileDialog
import java.io.File
import java.net.URI
import java.util.Locale

private val Tabs = listOf("Bibliothek", "Auto-Mix", "Decks", "Effekte", "Pads", "Granular")
private const val GRANULAR_TAB = 5

fun main() = application {
    installAutoDjFonts()
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
        window.minimumSize = java.awt.Dimension(1100, 720)
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

/** Paths of the audio files in a drop (file:// URIs from the system), without folders and files we cannot play. */
@OptIn(ExperimentalComposeUiApi::class)
private fun droppedAudioPaths(event: DragAndDropEvent): List<String> {
    val data = event.dragData() as? DragData.FilesList ?: return emptyList()
    return data.readFiles().mapNotNull { uri ->
        runCatching { File(URI(uri)) }.getOrNull()?.takeIf { it.isFile && it.extension.lowercase() in AudioExtensions }?.absolutePath
    }
}

@OptIn(ExperimentalComposeUiApi::class, ExperimentalFoundationApi::class)
@Composable
private fun FrameWindowScope.DesktopApp(host: DesktopHost) {
    val tabState = remember { mutableIntStateOf(0) }
    var tab by tabState
    val guide = remember { GuideState(GuideSeenFile.exists(), GuideSeenFile::markSeen) }
    var beatRequest by remember { mutableIntStateOf(0) }
    LaunchedEffect(host) {
        host.filePicker = { pickAudioFiles() }
        while (true) {
            delay(33)
            host.poll()
        }
    }
    val dropTarget = remember(host) {
        object : DragAndDropTarget {
            override fun onDrop(event: DragAndDropEvent): Boolean {
                val paths = droppedAudioPaths(event)
                if (paths.isEmpty()) {
                    host.report("Das war keine Audiodatei. Ziehe MP3, WAV, FLAC, AAC/M4A oder OGG ins Fenster.")
                    return false
                }
                if (tabState.intValue == GRANULAR_TAB) {
                    host.loadPaths(paths)
                } else {
                    host.dj.importSources(paths)
                    tabState.intValue = 0
                }
                return true
            }
        }
    }
    CompositionLocalProvider(LocalGuide provides guide) {
    Box(Modifier.fillMaxSize()) {
    Column(
        Modifier.fillMaxSize().background(Ds.c.ground).dragAndDropTarget(
            shouldStartDragAndDrop = { event -> event.dragData() is DragData.FilesList },
            target = dropTarget,
        ).onPreviewKeyEvent { e -> padKeyEvent(host, tab, e) },
    ) {
        val (bpm, bpmLabel) = clockInfo(host.dj)
        DesktopTopBar(
            Tabs, tab, { tab = it }, bpm, bpmLabel, audioOn = host.driver.running,
            onAudio = { if (host.driver.running) host.stopAudio() else host.startAudio() },
            modifier = Modifier.padding(AutoDjSpace.margin),
            trailing = { Btn("Anleitung", onClick = { guide.show() }, modifier = Modifier.height(AutoDjSpace.bar)) },
        )
        Box(Modifier.weight(1f)) {
            when (tab) {
                0 -> LibraryScreen(
                    host.dj,
                    dropHint = true,
                    onPickFiles = { host.dj.importSources(pickAudioFiles()) },
                    onGranular = { item ->
                        host.dj.sendToGranular(item) { slot, error ->
                            if (slot != null) {
                                host.engine.spec("granular.v0.source")?.let { host.setParam(it, slot.toFloat()) }
                                tab = GRANULAR_TAB
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
                            tab = GRANULAR_TAB
                        } else {
                            host.report(error ?: "Konnte nichts einfangen")
                        }
                    }
                })
                4 -> PadsScreen(host.dj, showKeys = true)
                else -> InstrumentScreen(host, onShowGuide = { guide.show(Guides.PLUGIN) }, beatRequest = beatRequest)
            }
        }
        StatusBar(host)
    }
    GuideOverlay(guide, onAction = { target ->
        when (target) {
            GuideTarget.LoadDemoSongs -> {
                host.dj.addDemoTracks()
                tab = 0
            }
            GuideTarget.Library -> tab = 0
            GuideTarget.AutoMix -> tab = 1
            GuideTarget.Decks -> tab = 2
            GuideTarget.Effects -> tab = 3
            GuideTarget.Pads -> tab = 4
            GuideTarget.Plugin -> tab = GRANULAR_TAB
            GuideTarget.PluginLoadFile -> {
                tab = GRANULAR_TAB
                host.filePicker?.invoke()?.let { host.loadPaths(it) }
            }
            GuideTarget.PluginBeatStart -> {
                tab = GRANULAR_TAB
                beatRequest += 1
            }
        }
    })
    }
    }
}

/** "The first-start guide was shown": an empty marker file in the user's home, so that the guide opens by itself only once. */
private object GuideSeenFile {
    private val file = File(System.getProperty("user.home") ?: ".", ".autodj/guide-seen")

    fun exists(): Boolean = runCatching { file.exists() }.getOrDefault(false)

    fun markSeen() {
        runCatching {
            file.parentFile?.mkdirs()
            file.writeText("1")
        }
    }
}

/** The bottom line: audio status and messages on the left, the look (day or night, calm) on the right. */
@Composable
private fun StatusBar(host: DesktopHost) {
    val d = host.driver
    val st = host.status
    val line = host.message ?: if (d.running) {
        String.format(Locale.US, "%s · %d Hz · %.1f ms · CPU %.0f %%", d.api, d.sampleRate, d.latencyMs, st.load * 100.0)
    } else {
        "Audio aus"
    }
    Row(Modifier.fillMaxWidth().padding(horizontal = AutoDjSpace.margin).padding(bottom = 8.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Txt(line, style = AutoDjType.small, color = if (host.message != null) Ds.c.warn.let { if (Ds.night) it else Ds.c.ink } else null, second = host.message == null, modifier = Modifier.weight(1f), maxLines = 1)
        Segment(listOf("Tag", "Nacht"), if (Ds.night) 1 else 0, { Ds.night = it == 1 }, small = true, compact = true)
        Btn("Bewegung reduzieren", onClick = { Ds.reduceMotion = !Ds.reduceMotion }, on = Ds.reduceMotion, small = true, style = AutoDjType.small)
    }
}

/** Keys 1234 / QWER / ASDF / YXCV play the 16 pads while the Pads tab is open and no text field wants the keys. */
@OptIn(ExperimentalComposeUiApi::class)
private fun padKeyEvent(host: DesktopHost, tab: Int, e: KeyEvent): Boolean {
    if (tab != 4) return false
    val index = PadKeyMap[e.key] ?: return false
    if (e.isCtrlPressed || e.isMetaPressed || e.isAltPressed) return false
    when (e.type) {
        KeyEventType.KeyDown -> if (padDown.add(index)) host.dj.perf.hit(index)
        KeyEventType.KeyUp -> padDown.remove(index)
        else -> return false
    }
    return true
}

private val padDown = mutableSetOf<Int>()
private val PadKeyMap: Map<Key, Int> = listOf(
    Key.One, Key.Two, Key.Three, Key.Four, Key.Q, Key.W, Key.E, Key.R, Key.A, Key.S, Key.D, Key.F, Key.Y, Key.X, Key.C, Key.V,
).withIndex().associate { it.value to it.index }
