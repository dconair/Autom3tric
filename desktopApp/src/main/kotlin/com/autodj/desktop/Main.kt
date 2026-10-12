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
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.DpSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import androidx.compose.ui.window.rememberWindowState
import com.autodj.app.ui.AutoDjTheme
import com.autodj.app.ui.InstrumentScreen
import com.autodj.app.ui.Palette
import kotlinx.coroutines.delay
import java.util.Locale

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
        state = rememberWindowState(size = DpSize(1100.dp, 760.dp)),
    ) {
        AutoDjTheme { DesktopApp(host) }
    }
}

@Composable
private fun DesktopApp(host: DesktopHost) {
    LaunchedEffect(host) {
        while (true) {
            delay(33)
            host.poll()
        }
    }
    Column(Modifier.fillMaxSize().background(Palette.body)) {
        Box(Modifier.weight(1f)) { InstrumentScreen(host) }
        val d = host.driver
        val s = host.status
        val line = host.message ?: if (d.running) {
            String.format(
                Locale.US, "%s · %d Hz · buffer %d frames · latency ≈ %.1f ms · CPU %.1f %% · %d grains · SIMD %s",
                d.api, d.sampleRate, d.framesPerBurst, d.latencyMs, s.load * 100.0, s.activeGrains, d.simd,
            )
        } else {
            "audio stopped"
        }
        Text(line, color = Palette.muted, fontSize = 12.sp, modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp))
    }
}
