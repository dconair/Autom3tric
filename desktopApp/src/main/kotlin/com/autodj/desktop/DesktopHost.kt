package com.autodj.desktop

import androidx.compose.runtime.State
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.autodj.app.audio.AudioEngine
import com.autodj.app.audio.DriverInfo
import com.autodj.app.audio.EngineStatus
import com.autodj.app.audio.GrainVizSnapshot
import com.autodj.app.audio.ParamSpec
import com.autodj.app.audio.SourceInfo
import com.autodj.app.ui.InstrumentHost

/**
 * The desktop counterpart of AutoDjViewModel: owns the native engine, keeps one Compose state per parameter and
 * exposes the polled status / grain cloud. [poll] is called ~30 times a second from the UI.
 */
class DesktopHost : InstrumentHost, AutoCloseable {
    val engine = AudioEngine(48000, voices = 4)
    private val specs: List<ParamSpec> = engine.specs
    private val paramStates = specs.map { mutableFloatStateOf(engine.getParam(it.id)) }

    private var vizState by mutableStateOf(GrainVizSnapshot.EMPTY)
    var status by mutableStateOf(EngineStatus())
        private set
    var driver by mutableStateOf(DriverInfo())
        private set
    var message by mutableStateOf<String?>(null)
        private set
    override val viz: GrainVizSnapshot get() = vizState
    override var selectedVoice: Int by mutableIntStateOf(0)

    private val peaksCache = HashMap<Int, FloatArray?>()
    private val infoCache = HashMap<Int, SourceInfo?>()
    private var tick = 0

    init {
        // Built-in synthetic sources until the importer arrives (milestone 3): slot = kind.
        for (kind in 0..3) engine.loadDemoSource(kind, kind)
        set("granular.v0.source", 0f)
        set("granular.v0.gate", 1f)
        set("granular.v0.density", 40f)
        set("granular.v0.size_ms", 160f)
        set("granular.v0.spray_ms", 300f)
        set("granular.v0.spread", 0.8f)
    }

    private fun set(key: String, value: Float) {
        spec(key)?.let { setParam(it, value) }
    }

    fun startAudio() {
        val r = engine.start()
        message = if (r == 0) null else "Audio output could not be opened (CoreAudio status $r): ${engine.driverInfo().lastError}"
        driver = engine.driverInfo()
    }

    fun stopAudio() {
        engine.stop()
        driver = engine.driverInfo()
    }

    fun poll() {
        engine.pollStatus()?.let { status = it }
        engine.pollViz()?.let { vizState = it }
        if (tick % 15 == 0) driver = engine.driverInfo()
        if (tick % 10 == 0) engine.collectGarbage()
        tick++
    }

    override fun spec(key: String): ParamSpec? = engine.spec(key)
    override fun paramState(spec: ParamSpec): State<Float> = paramStates[spec.id]
    override fun setParam(spec: ParamSpec, value: Float) {
        val v = value.coerceIn(spec.min, spec.max)
        paramStates[spec.id].floatValue = v
        engine.setParam(spec.id, v)
    }

    override fun noteOn(voice: Int, pitch: Float, velocity: Float) = engine.noteOn(voice, pitch, velocity)
    override fun noteOff(voice: Int) = engine.noteOff(voice)
    override fun resetSteps(voice: Int) = engine.resetSteps(voice)
    override fun peaksFor(slot: Int): FloatArray? = peaksCache.getOrPut(slot) { engine.sourcePeaks(slot, PEAK_COLUMNS) }
    override fun sourceInfoFor(slot: Int): SourceInfo? = infoCache.getOrPut(slot) { engine.sourceInfo(slot) }

    override fun close() {
        engine.stop()
        engine.close()
    }

    private companion object {
        const val PEAK_COLUMNS = 512
    }
}
