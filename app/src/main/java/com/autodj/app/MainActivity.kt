package com.autodj.app

import android.net.Uri
import android.os.Bundle
import androidx.activity.ComponentActivity
import android.content.Intent
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import com.autodj.app.ui.AutoDjApp
import com.autodj.app.ui.AutoDjTheme
import java.io.File

class MainActivity : ComponentActivity() {
    private val vm: AutoDjViewModel by viewModels()
    private val prefs by lazy { getSharedPreferences("autodj", MODE_PRIVATE) }

    /** True when the chooser was opened from the granular plugin: the files then go straight into the instrument. */
    private var pickForGranular = false

    private val pickAudio = registerForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { uris ->
        val forGranular = pickForGranular
        pickForGranular = false
        if (uris.isEmpty()) return@registerForActivityResult
        for (uri in uris) {
            try {
                // keep read access across restarts, so the library can decode the song again later
                contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            } catch (_: SecurityException) {
                // the provider does not offer persistable grants; the song stays usable for this session
            }
        }
        if (forGranular) vm.loadFilesIntoGranular(uris.map { it.toString() }) else vm.dj.importSources(uris.map { it.toString() })
    }

    private var exportBits = 16
    private var exportTarget: Uri? = null
    private val saveMix = registerForActivityResult(ActivityResultContracts.CreateDocument("audio/wav")) { uri ->
        if (uri == null) return@registerForActivityResult
        exportTarget = uri
        // the native renderer writes a real file; the finished file is copied to the place the user picked
        vm.dj.mix.export(File(cacheDir, "mix-export.wav").absolutePath, exportBits)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        vm.dj.mix.onExported = { path ->
            val target = exportTarget
            exportTarget = null
            if (target != null) {
                try {
                    contentResolver.openOutputStream(target)?.use { out -> File(path).inputStream().use { it.copyTo(out) } }
                } catch (e: Exception) {
                    vm.message = "Export konnte nicht gespeichert werden: ${e.message}"
                } finally {
                    File(path).delete()
                }
            }
        }
        setContent {
            AutoDjTheme {
                AutoDjApp(
                    vm,
                    onPickFiles = {
                        pickForGranular = false
                        pickAudio.launch(arrayOf("audio/*", "application/ogg"))
                    },
                    onPickGranularFiles = {
                        pickForGranular = true
                        pickAudio.launch(arrayOf("audio/*", "application/ogg"))
                    },
                    onExport = { bits ->
                        exportBits = bits
                        saveMix.launch("AutoDJ-Mix.wav")
                    },
                    guideSeen = prefs.getBoolean(GUIDE_SEEN, false),
                    onGuideSeen = { prefs.edit().putBoolean(GUIDE_SEEN, true).apply() },
                )
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

private const val GUIDE_SEEN = "guide_seen"
