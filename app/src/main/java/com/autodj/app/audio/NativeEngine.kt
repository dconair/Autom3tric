package com.autodj.app.audio

/**
 * Raw JNI surface of libautodj_native (native/platform/jni/NativeBridge.cpp). Everything here is a
 * control-thread call; the audio callback itself never touches the JVM. Use [AudioEngine] instead of this object.
 */
internal object NativeEngine {
    init {
        NativeLibrary.load()
    }

    @JvmStatic external fun nativeCreate(sampleRate: Int, voices: Int): Long
    @JvmStatic external fun nativeDestroy(handle: Long)
    @JvmStatic external fun nativeStart(handle: Long, exclusive: Boolean, deviceId: Int): Int
    @JvmStatic external fun nativeStop(handle: Long)
    @JvmStatic external fun nativeDriverInfo(handle: Long): String

    @JvmStatic external fun nativeParamSpecs(handle: Long): String
    @JvmStatic external fun nativeSetParam(handle: Long, id: Int, value: Float)
    @JvmStatic external fun nativeSetParamNormalized(handle: Long, id: Int, normalized: Float)
    @JvmStatic external fun nativeGetParam(handle: Long, id: Int): Float
    @JvmStatic external fun nativeExportPreset(handle: Long, name: String): String
    @JvmStatic external fun nativeImportPreset(handle: Long, json: String): String

    @JvmStatic external fun nativePlay(handle: Long)
    @JvmStatic external fun nativeStopTransport(handle: Long)
    @JvmStatic external fun nativeSeek(handle: Long, beats: Double)
    @JvmStatic external fun nativeNoteOn(handle: Long, voice: Int, pitchSemitones: Float, velocity: Float)
    @JvmStatic external fun nativeNoteOff(handle: Long, voice: Int)
    @JvmStatic external fun nativeResetEngine(handle: Long)
    @JvmStatic external fun nativeResetSteps(handle: Long, voice: Int)
    @JvmStatic external fun nativeCollectGarbage(handle: Long): Int
    @JvmStatic external fun nativeLoadDemoSource(handle: Long, slot: Int, kind: Int): Boolean
    @JvmStatic external fun nativeSourcePeaks(handle: Long, slot: Int, columns: Int, out: FloatArray): Boolean
    @JvmStatic external fun nativeSourceInfo(handle: Long, slot: Int): String

    @JvmStatic external fun nativePollStatus(handle: Long, out: DoubleArray): Boolean
    @JvmStatic external fun nativePollViz(handle: Long, out: FloatArray): Boolean

    @JvmStatic external fun nativeRunBenchmark(quick: Boolean): String
    @JvmStatic external fun nativeBenchmarkProgress(): Int
}
