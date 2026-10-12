package com.autodj.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import android.content.Intent
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import com.autodj.app.ui.AutoDjApp
import com.autodj.app.ui.AutoDjTheme

class MainActivity : ComponentActivity() {
    private val vm: AutoDjViewModel by viewModels()

    private val pickAudio = registerForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { uris ->
        if (uris.isEmpty()) return@registerForActivityResult
        for (uri in uris) {
            try {
                // keep read access across restarts, so the library can decode the song again later
                contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            } catch (_: SecurityException) {
                // the provider does not offer persistable grants; the song stays usable for this session
            }
        }
        vm.dj.importSources(uris.map { it.toString() })
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            AutoDjTheme {
                AutoDjApp(vm, onPickFiles = { pickAudio.launch(arrayOf("audio/*", "application/ogg")) })
            }
        }
    }

    override fun onStart() {
        super.onStart()
        vm.onUiVisible(true)
    }

    override fun onStop() {
        vm.onUiVisible(false)
        super.onStop()
    }
}
