package com.autodj.app.ui

import android.content.ClipData
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.platform.ClipEntry
import androidx.compose.ui.platform.Clipboard
import androidx.compose.ui.platform.LocalClipboard
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch

/** Plain-text clipboard access from non-suspend UI callbacks. */
class ClipboardActions(private val clipboard: Clipboard, private val scope: CoroutineScope) {
    fun copy(text: String) {
        scope.launch { clipboard.setClipEntry(ClipEntry(ClipData.newPlainText("AutoDJ", text))) }
    }

    fun paste(onText: (String) -> Unit) {
        scope.launch {
            val text = clipboard.getClipEntry()?.clipData?.takeIf { it.itemCount > 0 }?.getItemAt(0)?.text?.toString().orEmpty()
            onText(text)
        }
    }
}

@Composable
fun rememberClipboardActions(): ClipboardActions {
    val clipboard = LocalClipboard.current
    val scope = rememberCoroutineScope()
    return remember(clipboard, scope) { ClipboardActions(clipboard, scope) }
}
