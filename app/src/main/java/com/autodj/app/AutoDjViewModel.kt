package com.autodj.app

import android.app.Application
import android.content.Context
import android.media.AudioManager
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.autodj.app.audio.AudioEngine
import com.autodj.app.audio.BenchmarkResult
import com.autodj.app.audio.DriverInfo
import com.autodj.app.audio.EngineStatus
import com.autodj.app.audio.GrainVizSnapshot
import com.autodj.app.audio.ParamSpec
import com.autodj.app.audio.SourceInfo
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/**
 * Owns the native engine for the lifetime of the app process' UI. Parameter values live in one Compose state
 * per parameter, so moving a slider recomposes only that slider; engine status and the grain cloud are polled
 * from the lock-free triple buffers at ~30 Hz.
 */
class AutoDjViewModel(app: Application) : AndroidViewModel(app) {
    val engine = AudioEngine(deviceSampleRate(app), voices = 4)
    val specs: List<ParamSpec> = engine.specs

    private val paramStates = specs.map { mutableFloatStateOf(engine.getParam(it.id)) }
    fun paramState(spec: ParamSpec) = paramStates[spec.id]

    var status by mutableStateOf(EngineStatus())
        private set
    var viz by mutableStateOf(GrainVizSnapshot.EMPTY)
        private set
    var driver by mutableStateOf(DriverInfo())
        private set
    var running by mutableStateOf(false)
        private set
    var message by mutableStateOf<String?>(null)
    var exclusive by mutableStateOf(true)
    var selectedVoice by mutableIntStateOf(0)

    /** Bumped whenever the content of a source slot changes, so the waveform screen re-reads its overview. */
    var peaksVersion by mutableIntStateOf(0)
        private set
    private val peaksCache = HashMap<Int, FloatArray?>()
    private val infoCache = HashMap<Int, SourceInfo?>()

    /** Waveform overview (PEAK_COLUMNS min/max pairs) of a slot; pass [peaksVersion] so that composition re-runs on change. */
    @Suppress("UNUSED_PARAMETER")
    fun peaksFor(slot: Int, version: Int): FloatArray? = peaksCache.getOrPut(slot) { engine.sourcePeaks(slot, PEAK_COLUMNS) }

    @Suppress("UNUSED_PARAMETER")
    fun sourceInfoFor(slot: Int, version: Int): SourceInfo? = infoCache.getOrPut(slot) { engine.sourceInfo(slot) }

    var benchRunning by mutableStateOf(false)
        private set
    var benchProgress by mutableIntStateOf(0)
        private set
    var benchResult by mutableStateOf<BenchmarkResult?>(null)
        private set

    private var uiVisible = true
    private var resumeAfterBackground = false

    init {
        // Built-in synthetic sources (a real decoder arrives with milestone 3): slot = kind.
        for (kind in 0..3) engine.loadDemoSource(kind, kind)
        applyDemoSetup()
        viewModelScope.launch {
            var tick = 0
            while (isActive) {
                delay(if (uiVisible) 33L else 500L)
                if (!uiVisible) continue
                engine.pollStatus()?.let { status = it }
                engine.pollViz()?.let { viz = it }
                if (tick % 15 == 0) driver = engine.driverInfo()
                if (tick % 10 == 0) engine.collectGarbage()
                tick++
            }
        }
    }

    /** Voice 1 plays the chord pad as soon as the audio is started, so there is something to hear. */
    private fun applyDemoSetup() {
        set("granular.v0.source", 0f)
        set("granular.v0.gate", 1f)
        set("granular.v0.density", 40f)
        set("granular.v0.size_ms", 160f)
        set("granular.v0.spray_ms", 300f)
        set("granular.v0.spread", 0.8f)
    }

    private fun set(key: String, value: Float) {
        engine.spec(key)?.let { setParam(it, value) }
    }

    fun setParam(spec: ParamSpec, value: Float) {
        val v = value.coerceIn(spec.min, spec.max)
        paramStates[spec.id].floatValue = v
        engine.setParam(spec.id, v)
    }

    /** Re-reads every value from the engine (after a preset import). */
    private fun syncParams() {
        for (s in specs) paramStates[s.id].floatValue = engine.getParam(s.id)
    }

    fun toggleAudio() = if (running) stopAudio() else startAudio()

    fun startAudio() {
        val r = engine.start(exclusive)
        running = r == 0
        message = if (r == 0) null else "Audio stream could not be opened (Oboe result $r): ${engine.driverInfo().lastError}"
        driver = engine.driverInfo()
    }

    fun stopAudio() {
        engine.stop()
        running = false
        driver = engine.driverInfo()
    }

    fun onUiVisible(visible: Boolean) {
        uiVisible = visible
        if (!visible) {
            // The stream is paused while the app is in the background (a foreground service for background
            // playback comes with the player in a later milestone).
            resumeAfterBackground = running
            if (running) stopAudio()
        } else if (resumeAfterBackground) {
            resumeAfterBackground = false
            startAudio()
        }
    }

    fun noteOn(voice: Int, pitch: Float, velocity: Float) = engine.noteOn(voice, pitch, velocity)
    fun noteOff(voice: Int) = engine.noteOff(voice)
    fun loadDemoSource(slot: Int, kind: Int): Boolean {
        val ok = engine.loadDemoSource(slot, kind)
        peaksCache.remove(slot)
        infoCache.remove(slot)
        peaksVersion++
        return ok
    }
    fun resetSteps(voice: Int) = engine.resetSteps(voice)
    fun play() = engine.play()
    fun stopTransport() = engine.stopTransport()
    fun seek(beats: Double) = engine.seek(beats)

    fun exportPreset(): String = engine.exportPreset("AutoDJ preset")

    fun importPreset(json: String) {
        message = engine.importPreset(json)
        syncParams()
    }

    fun runBenchmark(quick: Boolean) {
        if (benchRunning) return
        if (running) stopAudio()  // measure an otherwise idle device
        benchRunning = true
        benchResult = null
        benchProgress = 0
        viewModelScope.launch {
            val poll = launch {
                while (isActive) {
                    benchProgress = AudioEngine.benchmarkProgress()
                    delay(200)
                }
            }
            benchResult = withContext(Dispatchers.Default) { AudioEngine.runBenchmark(quick) }
            poll.cancel()
            benchProgress = 100
            benchRunning = false
        }
    }

    override fun onCleared() {
        engine.close()
    }

    private companion object {
        const val PEAK_COLUMNS = 512

        fun deviceSampleRate(app: Application): Int {
            val am = app.getSystemService(Context.AUDIO_SERVICE) as AudioManager
            return am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toIntOrNull() ?: 48000
        }
    }
}
