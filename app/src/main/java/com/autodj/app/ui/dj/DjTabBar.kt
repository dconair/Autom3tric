package com.autodj.app.ui.dj

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autodj.app.dj.DjColors

/** Top navigation of the desktop app (the Android app uses a bottom bar): one text tab per screen, [trailing] on the right. */
@Composable
fun DjTabBar(
    labels: List<String>,
    selected: Int,
    onSelect: (Int) -> Unit,
    modifier: Modifier = Modifier,
    trailing: @Composable () -> Unit = {},
) {
    Row(
        modifier.fillMaxWidth().background(DjColors.panel).padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(2.dp),
    ) {
        labels.forEachIndexed { i, label ->
            val on = i == selected
            Box(
                Modifier.height(44.dp).clip(RoundedCornerShape(8.dp)).clickable(role = Role.Tab) { onSelect(i) }.padding(horizontal = 16.dp),
                contentAlignment = Alignment.Center,
            ) {
                Text(label, color = if (on) DjColors.text else DjColors.muted, fontSize = 15.sp)
                if (on) {
                    Box(Modifier.align(Alignment.BottomCenter).width(24.dp).height(2.dp).background(DjColors.deckA))
                }
            }
        }
        Box(Modifier.weight(1f))
        trailing()
    }
}
