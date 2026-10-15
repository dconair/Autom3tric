package com.autodj.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.waitForUpOrCancellation
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import com.autodj.app.AutoDjViewModel
import com.autodj.app.audio.ParamSpec
import kotlin.math.roundToInt

/** Sections of the voice parameters; any key that is not listed lands in "More". */
private val Sections = listOf(
    "Source & position" to listOf("source", "position", "scan_on", "scan_speed", "loop_start", "loop_end", "freeze"),
    "Grains" to listOf("size_ms", "density", "spray_ms", "jitter", "env_shape", "quality"),
    "Pitch & stereo" to listOf("pitch_st", "pitch_rand_st", "reverse_prob", "spread", "amp_rand"),
    "Trigger" to listOf("sync_mode", "sync_div"),
    "Output" to listOf("gate", "volume", "release_ms", "auto_gain"),
)

private val DemoSources = listOf("Pad", "Drums", "Noise", "Sine")

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun GranularScreen(vm: AutoDjViewModel, modifier: Modifier = Modifier) {
    val voice = vm.selectedVoice
    val voiceSpecs = vm.specs.filter { it.key.startsWith("granular.v$voice.") }
    val byShort = voiceSpecs.associateBy { it.shortKey }
    val used = Sections.flatMap { it.second }.toSet()
    val more = voiceSpecs.filter { it.shortKey !in used }

    LazyColumn(
        modifier.fillMaxSize().padding(horizontal = 16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.padding(top = 8.dp)) {
                for (v in 0 until 4) {
                    FilterChip(
                        selected = v == voice,
                        onClick = { vm.selectedVoice = v },
                        label = { Text("Voice ${v + 1}") },
                    )
                }
            }
        }
        item { GrainCloud(vm.viz, voice) }
        item {
            Text("Demo sources → slot", style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                DemoSources.forEachIndexed { slot, name ->
                    OutlinedButton(onClick = {
                        vm.loadDemoSource(slot, slot)
                        byShort["source"]?.let { vm.setParam(it, slot.toFloat()) }
                    }) { Text(name) }
                }
            }
        }
        item { TriggerPad(vm, voice) }
        for ((title, keys) in Sections) {
            val specs = keys.mapNotNull { byShort[it] }
            if (specs.isNotEmpty()) item(key = "$voice-$title") { Section(vm, title, specs) }
        }
        if (more.isNotEmpty()) item(key = "$voice-more") { Section(vm, "More", more) }
        item { Box(Modifier.height(24.dp)) }
    }
}

@Composable
private fun Section(vm: AutoDjViewModel, title: String, specs: List<ParamSpec>) {
    Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(title, style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.primary)
            for (s in specs) ParamControl(vm, s)
        }
    }
}

/** Hold to play the voice: x selects the pitch (-12..+12 st), y the velocity. */
@Composable
private fun TriggerPad(vm: AutoDjViewModel, voice: Int) {
    var down by remember { mutableStateOf(false) }
    Box(
        Modifier
            .fillMaxWidth()
            .height(110.dp)
            .clip(RoundedCornerShape(16.dp))
            .background(if (down) VoiceColors[voice].copy(alpha = 0.45f) else MaterialTheme.colorScheme.surfaceVariant)
            .pointerInput(voice) {
                awaitEachGesture {
                    val d = awaitFirstDown()
                    val pitch = ((d.position.x / size.width) * 24f - 12f).roundToInt().toFloat().coerceIn(-12f, 12f)
                    val vel = (1f - 0.7f * (d.position.y / size.height)).coerceIn(0.2f, 1f)
                    down = true
                    vm.noteOn(voice, pitch, vel)
                    waitForUpOrCancellation()
                    vm.noteOff(voice)
                    down = false
                }
            },
        contentAlignment = Alignment.Center,
    ) {
        Text("Hold: play voice ${voice + 1}  (x = pitch, y = velocity)", style = MaterialTheme.typography.bodyMedium)
    }
}
