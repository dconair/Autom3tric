package com.autodj.app.ui

import androidx.compose.runtime.State
import com.autodj.app.AutoDjViewModel
import com.autodj.app.audio.ParamSpec

/** Adapter between the ViewModel (engine, polled state) and the instrument screen. */
fun AutoDjViewModel.asInstrumentHost(onPickFiles: (() -> Unit)? = null): InstrumentHost {
    val vm = this
    return object : InstrumentHost {
        override val viz get() = vm.viz
        override var selectedVoice: Int
            get() = vm.selectedVoice
            set(value) { vm.selectedVoice = value }
        override fun spec(key: String) = vm.engine.spec(key)
        override fun paramState(spec: ParamSpec): State<Float> = vm.paramState(spec)
        override fun setParam(spec: ParamSpec, value: Float) = vm.setParam(spec, value)
        override fun noteOn(voice: Int, pitch: Float, velocity: Float) = vm.noteOn(voice, pitch, velocity)
        override fun noteOff(voice: Int) = vm.noteOff(voice)
        override fun resetSteps(voice: Int) = vm.resetSteps(voice)
        override fun peaksFor(slot: Int) = vm.peaksFor(slot, vm.peaksVersion)
        override fun sourceInfoFor(slot: Int) = vm.sourceInfoFor(slot, vm.peaksVersion)
        override val sourceNames get() = DemoSourceNames + vm.dj.granularSlots.map { it.name }
        override val pattern get() = vm.granularPattern
        override val canLoadFiles get() = onPickFiles != null
        override fun loadFiles() { onPickFiles?.invoke() }
        override val loadStatus get() = vm.dj.granularLoadStatus
        override val followsMusic get() = vm.dj.isMusicPlaying
    }
}
