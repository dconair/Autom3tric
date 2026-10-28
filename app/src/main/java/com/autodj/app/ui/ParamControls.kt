package com.autodj.app.ui

import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import com.autodj.app.AutoDjViewModel
import com.autodj.app.audio.ParamSpec
import com.autodj.app.audio.ParamType

/** One control for one registry parameter. The widget kind follows the parameter type; double tap resets to default. */
@Composable
fun ParamControl(vm: AutoDjViewModel, spec: ParamSpec, modifier: Modifier = Modifier) {
    val value = vm.paramState(spec).floatValue
    val onChange: (Float) -> Unit = { vm.setParam(spec, it) }
    when (spec.type) {
        ParamType.Bool -> Row(
            modifier.fillMaxWidth().padding(vertical = 2.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Text(spec.label, style = MaterialTheme.typography.bodyMedium)
            Switch(checked = value >= 0.5f, onCheckedChange = { onChange(if (it) 1f else 0f) })
        }

        ParamType.Enum -> Column(modifier.fillMaxWidth().padding(vertical = 4.dp)) {
            Text(spec.label, style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                spec.enumNames.forEachIndexed { i, name ->
                    FilterChip(selected = value.toInt() == i, onClick = { onChange(i.toFloat()) }, label = { Text(name) })
                }
            }
        }

        else -> Column(modifier.fillMaxWidth()) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text(spec.label, style = MaterialTheme.typography.bodyMedium)
                Text(spec.format(value), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.primary)
            }
            val steps = if (spec.type == ParamType.Int && spec.max - spec.min <= 32f) (spec.max - spec.min).toInt() - 1 else 0
            Slider(
                value = spec.toNormalized(value),
                onValueChange = { onChange(spec.fromNormalized(it)) },
                steps = steps.coerceAtLeast(0),
                modifier = Modifier.pointerInput(spec.id) { detectTapGestures(onDoubleTap = { onChange(spec.def) }) },
            )
        }
    }
}
