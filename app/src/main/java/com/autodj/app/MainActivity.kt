package com.autodj.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import com.autodj.app.ui.AutoDjApp
import com.autodj.app.ui.AutoDjTheme

class MainActivity : ComponentActivity() {
    private val vm: AutoDjViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            AutoDjTheme {
                AutoDjApp(vm)
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
