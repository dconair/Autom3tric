package com.autodj.app.dj

/** Receives decoded PCM in chunks. Samples are interleaved floats in -1..1. */
interface PcmSink {
    /** Called once before the first chunk. */
    fun onFormat(sampleRate: Int, channels: Int)

    /** [count] valid samples (frames * channels) at the start of [data]; the decoder reuses the array afterwards. */
    fun onSamples(data: FloatArray, count: Int)

    /** Optional: fraction 0..1 of the file decoded so far, when the decoder knows the length. */
    fun onProgress(fraction: Float) {}
}

class DecodeException(message: String, cause: Throwable? = null) : Exception(message, cause)

/**
 * Turns a file (path or content URI, whatever [source] is on this platform) into PCM. Implementations block the calling
 * thread, never touch the audio engine and must be safe to call from a worker thread. Platform code lives in
 * the app (Android MediaCodec) and desktopApp (Java Sound, afconvert on macOS).
 */
interface TrackDecoder {
    /** Decodes the whole file; throws [DecodeException] with a message fit for the UI when it cannot. */
    fun decode(source: String, sink: PcmSink)

    /** A readable title for the library (file name without extension). */
    fun displayName(source: String): String
}
