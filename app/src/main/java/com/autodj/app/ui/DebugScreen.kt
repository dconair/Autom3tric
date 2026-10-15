package com.autodj.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autodj.app.AutoDjViewModel
import java.util.Locale

@Composable
fun DebugScreen(vm: AutoDjViewModel, modifier: Modifier = Modifier) {
    val s = vm.status
    val d = vm.driver
    val clipboard = rememberClipboardActions()
    Column(
        modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text("Audio stream", style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.primary)
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Request exclusive mode (applies on next start)")
                    Switch(checked = vm.exclusive, onCheckedChange = { vm.exclusive = it })
                }
                Mono("API / sharing / mode : ${d.api} / ${d.sharing} / ${d.performance}")
                Mono("sample rate          : ${d.sampleRate} Hz (engine ${s.sampleRate} Hz)")
                Mono("frames per burst     : ${d.framesPerBurst}")
                Mono("frames per callback  : ${d.framesPerCallback} (engine saw ${s.callbackFrames})")
                Mono("buffer size/capacity : ${d.bufferSize} / ${d.bufferCapacity} frames")
                Mono("latency (estimated)  : ${if (d.latencyMs >= 0) String.format(Locale.US, "%.1f ms", d.latencyMs) else "n/a"}")
                Mono("driver xruns         : ${d.xruns}   restarts: ${d.restarts}")
                Mono("ADPF performance hint: ${if (d.performanceHint) "requested via Oboe (active only if the device implements it)" else "off"}")
                if (d.lastError.isNotEmpty()) Mono("last error          : ${d.lastError}")
            }
        }
        Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text("Engine", style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.primary)
                Mono(String.format(Locale.US, "callback load        : %.1f %% (peak %.1f %%, last %.1f %%)", 100 * s.load, 100 * s.loadPeak, 100 * s.lastBlockLoad))
                Mono("deadline misses      : ${s.deadlineMisses}")
                Mono("active grains/voices : ${s.activeGrains} / ${s.activeVoices}   dropped: ${s.droppedGrains}")
                Mono("adaptive quality     : level ${s.qualityLevel} (0 = full)")
                Mono("grain kernels        : ${d.simd}")
                Mono(String.format(Locale.US, "output peak          : L %.2f  R %.2f", s.peakL, s.peakR))
                Mono("callbacks            : ${s.callbacks}")
                Mono("RT guard (debug)     : ${if (d.rtGuard) "on, violations: ${d.rtViolations}" else "off (release build)"}")
            }
        }
        Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Granular benchmark on this device", style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.primary)
                Text(
                    "64/128/256/512 grains × linear/Hermite × scalar/SIMD × 128/192 frames. Audio is stopped while it runs. " +
                        "Keep the screen on and the device cool for comparable numbers.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(enabled = !vm.benchRunning, onClick = { vm.runBenchmark(quick = true) }) { Text("Quick (~1 min)") }
                    OutlinedButton(enabled = !vm.benchRunning, onClick = { vm.runBenchmark(quick = false) }) { Text("Full (~10 min)") }
                }
                if (vm.benchRunning) {
                    LinearProgressIndicator(progress = { vm.benchProgress / 100f }, modifier = Modifier.fillMaxWidth())
                }
                vm.benchResult?.let { r ->
                    OutlinedButton(onClick = {
                        clipboard.copy(r.markdown + "\n\n```json\n" + r.json + "\n```")
                        vm.message = "Benchmark copied to the clipboard"
                    }) { Text("Copy results") }
                    SelectionContainer {
                        Text(
                            r.markdown,
                            fontFamily = FontFamily.Monospace,
                            fontSize = 10.sp,
                            lineHeight = 13.sp,
                            modifier = Modifier.fillMaxWidth(),
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun Mono(text: String) {
    Text(text, fontFamily = FontFamily.Monospace, fontSize = 12.sp, lineHeight = 16.sp)
}
