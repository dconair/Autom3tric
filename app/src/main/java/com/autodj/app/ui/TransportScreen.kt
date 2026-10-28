package com.autodj.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.autodj.app.AutoDjViewModel
import java.util.Locale

@Composable
fun TransportScreen(vm: AutoDjViewModel, modifier: Modifier = Modifier) {
    val clipboard = rememberClipboardActions()
    Column(
        modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Transport", style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.primary)
                vm.engine.spec("transport.bpm")?.let { ParamControl(vm, it) }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = { vm.play() }) { Text("Play") }
                    OutlinedButton(onClick = { vm.stopTransport() }) { Text("Stop") }
                    OutlinedButton(onClick = { vm.seek(0.0) }) { Text("Rewind") }
                }
                val s = vm.status
                Text(
                    String.format(Locale.US, "clock beat %.2f   play-head %.2f   %s", s.clockBeat, s.playBeat, if (s.playing) "playing" else "stopped"),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Text(
                    "The master clock free-runs; beat-synced grains lock to it, the play-head only matters for the timeline.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Master & test tone", style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.primary)
                for (key in listOf("master.gain", "tone.enabled", "tone.freq_hz", "tone.gain")) {
                    vm.engine.spec(key)?.let { ParamControl(vm, it) }
                }
            }
        }
        Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Presets (JSON, versioned)", style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.primary)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(onClick = {
                        clipboard.copy(vm.exportPreset())
                        vm.message = "Preset copied to the clipboard"
                    }) { Text("Copy preset") }
                    OutlinedButton(onClick = {
                        clipboard.paste { text ->
                            if (text.isBlank()) vm.message = "Clipboard is empty" else vm.importPreset(text)
                        }
                    }) { Text("Paste preset") }
                }
            }
        }
    }
}
