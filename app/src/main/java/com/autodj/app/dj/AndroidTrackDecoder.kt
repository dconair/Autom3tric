package com.autodj.app.dj

import android.content.Context
import android.media.AudioFormat
import android.media.MediaCodec
import android.media.MediaExtractor
import android.media.MediaFormat
import android.net.Uri
import android.provider.OpenableColumns
import java.io.IOException
import java.nio.ByteOrder

/**
 * Android decoder: MediaExtractor + MediaCodec, so everything the phone can play is importable (MP3, AAC/M4A, FLAC,
 * OGG, WAV, Opus ...). [source] is a content:// URI from the document picker, or a file path.
 */
class AndroidTrackDecoder(private val context: Context) : TrackDecoder {

    override fun displayName(source: String): String {
        val uri = Uri.parse(source)
        var name: String? = null
        if (uri.scheme == "content") {
            try {
                context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
                    if (c.moveToFirst()) name = c.getString(0)
                }
            } catch (_: Exception) {
                // fall through to the last path segment
            }
        }
        val raw = name ?: uri.lastPathSegment ?: source
        val base = raw.substringAfterLast('/')
        return base.substringBeforeLast('.', base).ifBlank { base }
    }

    override fun decode(source: String, sink: PcmSink) {
        val extractor = MediaExtractor()
        var codec: MediaCodec? = null
        try {
            val uri = Uri.parse(source)
            try {
                if (uri.scheme == null || uri.scheme == "file") extractor.setDataSource(uri.path ?: source)
                else extractor.setDataSource(context, uri, null)
            } catch (e: IOException) {
                throw DecodeException("Die Datei lässt sich nicht öffnen: ${e.message ?: ""}".trim(), e)
            } catch (e: SecurityException) {
                throw DecodeException("Kein Zugriff mehr auf die Datei – bitte noch einmal hinzufügen", e)
            } catch (e: RuntimeException) {
                throw DecodeException("Dieses Format wird nicht unterstützt", e)
            }
            var trackIndex = -1
            var inFormat: MediaFormat? = null
            for (i in 0 until extractor.trackCount) {
                val f = extractor.getTrackFormat(i)
                if (f.getString(MediaFormat.KEY_MIME)?.startsWith("audio/") == true) {
                    trackIndex = i
                    inFormat = f
                    break
                }
            }
            if (trackIndex < 0 || inFormat == null) throw DecodeException("Die Datei enthält keine Audiospur")
            extractor.selectTrack(trackIndex)
            val mime = inFormat.getString(MediaFormat.KEY_MIME)!!
            val durationUs = if (inFormat.containsKey(MediaFormat.KEY_DURATION)) inFormat.getLong(MediaFormat.KEY_DURATION) else 0L

            codec = try {
                MediaCodec.createDecoderByType(mime).also { it.configure(inFormat, null, null, 0); it.start() }
            } catch (e: Exception) {
                throw DecodeException("Kein Dekodierer für $mime auf diesem Gerät", e)
            }
            val info = MediaCodec.BufferInfo()
            var sawInputEnd = false
            var sawOutputEnd = false
            var started = false
            var channels = 0
            var pcmEncoding = AudioFormat.ENCODING_PCM_16BIT
            var out = FloatArray(0)
            var lastProgress = -1f

            while (!sawOutputEnd) {
                if (!sawInputEnd) {
                    val inIx = codec.dequeueInputBuffer(TIMEOUT_US)
                    if (inIx >= 0) {
                        val buf = codec.getInputBuffer(inIx)!!
                        val n = extractor.readSampleData(buf, 0)
                        if (n < 0) {
                            codec.queueInputBuffer(inIx, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM)
                            sawInputEnd = true
                        } else {
                            codec.queueInputBuffer(inIx, 0, n, extractor.sampleTime, 0)
                            extractor.advance()
                        }
                    }
                }
                val outIx = codec.dequeueOutputBuffer(info, TIMEOUT_US)
                when {
                    outIx == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> {
                        val f = codec.outputFormat
                        val rate = f.getInteger(MediaFormat.KEY_SAMPLE_RATE)
                        val ch = f.getInteger(MediaFormat.KEY_CHANNEL_COUNT)
                        pcmEncoding = if (f.containsKey(MediaFormat.KEY_PCM_ENCODING)) f.getInteger(MediaFormat.KEY_PCM_ENCODING) else AudioFormat.ENCODING_PCM_16BIT
                        if (started && ch != channels) throw DecodeException("Der Kanalwechsel mitten im Song wird nicht unterstützt")
                        channels = ch
                        if (!started) {
                            sink.onFormat(rate, ch)
                            started = true
                        }
                    }
                    outIx >= 0 -> {
                        if (!started) {  // some codecs deliver data before announcing the format
                            val f = codec.outputFormat
                            channels = f.getInteger(MediaFormat.KEY_CHANNEL_COUNT)
                            pcmEncoding = if (f.containsKey(MediaFormat.KEY_PCM_ENCODING)) f.getInteger(MediaFormat.KEY_PCM_ENCODING) else AudioFormat.ENCODING_PCM_16BIT
                            sink.onFormat(f.getInteger(MediaFormat.KEY_SAMPLE_RATE), channels)
                            started = true
                        }
                        val buf = codec.getOutputBuffer(outIx)!!
                        if (info.size > 0 && channels > 0) {
                            buf.position(info.offset)
                            buf.limit(info.offset + info.size)
                            buf.order(ByteOrder.nativeOrder())
                            val count = if (pcmEncoding == AudioFormat.ENCODING_PCM_FLOAT) info.size / 4 else info.size / 2
                            if (out.size < count) out = FloatArray(count)
                            if (pcmEncoding == AudioFormat.ENCODING_PCM_FLOAT) {
                                buf.asFloatBuffer().get(out, 0, count)
                            } else {
                                val s = buf.asShortBuffer()
                                for (i in 0 until count) out[i] = s.get(i) / 32768f
                            }
                            val whole = count - count % channels
                            if (whole > 0) sink.onSamples(out, whole)
                            if (durationUs > 0 && info.presentationTimeUs >= 0) {
                                val p = (info.presentationTimeUs.toDouble() / durationUs).toFloat().coerceIn(0f, 1f)
                                if (p - lastProgress >= 0.01f) {
                                    lastProgress = p
                                    sink.onProgress(p)
                                }
                            }
                        }
                        if (info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) sawOutputEnd = true
                        codec.releaseOutputBuffer(outIx, false)
                    }
                    else -> Unit  // INFO_TRY_AGAIN_LATER / buffers changed
                }
            }
            if (!started) throw DecodeException("Die Datei enthält keine Audiodaten")
        } catch (e: DecodeException) {
            throw e
        } catch (e: Exception) {
            throw DecodeException("Dekodierfehler: ${e.message ?: e.javaClass.simpleName}", e)
        } finally {
            try {
                codec?.stop()
            } catch (_: Exception) {
            }
            codec?.release()
            extractor.release()
        }
    }

    private companion object {
        const val TIMEOUT_US = 10_000L
    }
}
