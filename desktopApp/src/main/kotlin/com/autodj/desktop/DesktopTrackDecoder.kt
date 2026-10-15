package com.autodj.desktop

import com.autodj.app.dj.DecodeException
import com.autodj.app.dj.PcmSink
import com.autodj.app.dj.TrackDecoder
import java.io.File
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.channels.FileChannel
import java.nio.file.StandardOpenOption
import java.util.concurrent.TimeUnit
import javax.sound.sampled.AudioFormat
import javax.sound.sampled.AudioInputStream
import javax.sound.sampled.AudioSystem
import javax.sound.sampled.UnsupportedAudioFileException

/**
 * Desktop decoder. WAV is read directly; on macOS everything else goes through the system's `afconvert` (MP3, AAC/M4A,
 * FLAC, AIFF, CAF ...) into a temporary float WAV; the Java Sound providers on the classpath (MP3, FLAC, Ogg Vorbis)
 * are the fallback and the path on other systems; `ffmpeg` is used last when it happens to be installed.
 */
class DesktopTrackDecoder(
    private val afconvert: File? = File("/usr/bin/afconvert").takeIf { it.canExecute() },
    private val ffmpeg: String? = findOnPath("ffmpeg"),
) : TrackDecoder {

    override fun displayName(source: String): String = File(source).nameWithoutExtension.ifBlank { File(source).name }

    override fun decode(source: String, sink: PcmSink) {
        val file = File(source)
        if (!file.isFile) throw DecodeException("Datei nicht gefunden: ${file.name}")
        if (file.length() == 0L) throw DecodeException("Die Datei ist leer: ${file.name}")
        val errors = ArrayList<String>()
        val ext = file.extension.lowercase()

        if (ext == "wav" || ext == "wave") {
            try {
                WavReader.decode(file, sink)
                return
            } catch (e: DecodeException) {
                errors += e.message ?: "WAV"
            }
        }
        if (afconvert != null) {
            try {
                viaConverter(file, sink) { out -> listOf(afconvert.path, "-f", "WAVE", "-d", "LEF32", file.path, out.path) }
                return
            } catch (e: DecodeException) {
                errors += "afconvert: ${e.message}"
            }
        }
        try {
            viaJavaSound(file, sink)
            return
        } catch (e: DecodeException) {
            errors += e.message ?: "Java Sound"
        }
        if (ffmpeg != null) {
            try {
                viaConverter(file, sink) { out -> listOf(ffmpeg, "-v", "error", "-y", "-i", file.path, "-vn", "-c:a", "pcm_f32le", "-f", "wav", out.path) }
                return
            } catch (e: DecodeException) {
                errors += "ffmpeg: ${e.message}"
            }
        }
        throw DecodeException(
            "Das Format von „${file.name}“ wird nicht unterstützt" +
                (if (errors.isNotEmpty()) " (${errors.last()})" else "") + ". Unterstützt: MP3, WAV, FLAC, OGG, M4A/AAC.",
        )
    }

    // ---- external converter -> temporary WAV ------------------------------------------------------

    private fun viaConverter(file: File, sink: PcmSink, command: (File) -> List<String>) {
        val tmp = File.createTempFile("autodj-decode-", ".wav")
        try {
            val proc = try {
                ProcessBuilder(command(tmp)).redirectErrorStream(true).start()
            } catch (e: IOException) {
                throw DecodeException(e.message ?: "Programm nicht startbar")
            }
            val log = StringBuilder()
            val reader = Thread { runCatching { proc.inputStream.bufferedReader().forEachLine { if (log.length < 600) log.appendLine(it) } } }
            reader.isDaemon = true
            reader.start()
            if (!proc.waitFor(10, TimeUnit.MINUTES)) {
                proc.destroyForcibly()
                throw DecodeException("Zeitüberschreitung beim Umwandeln")
            }
            reader.join(1000)
            if (proc.exitValue() != 0 || tmp.length() < 44) throw DecodeException(log.toString().trim().ifEmpty { "Umwandlung fehlgeschlagen" })
            WavReader.decode(tmp, sink)
        } finally {
            tmp.delete()
        }
    }

    // ---- Java Sound -------------------------------------------------------------------------------

    private fun viaJavaSound(file: File, sink: PcmSink) {
        val raw: AudioInputStream = try {
            AudioSystem.getAudioInputStream(file)
        } catch (e: UnsupportedAudioFileException) {
            throw DecodeException("kein Dekodierer für das Format")
        } catch (e: IOException) {
            throw DecodeException(e.message ?: "Lesefehler")
        }
        raw.use { base ->
            val f = base.format
            val isPcm = f.encoding == AudioFormat.Encoding.PCM_SIGNED || f.encoding == AudioFormat.Encoding.PCM_UNSIGNED || f.encoding == AudioFormat.Encoding.PCM_FLOAT
            val pcm = if (isPcm) base else toPcm(base)
            try {
                // frame counts of compressed streams are not reliable (the FLAC reader reports the file size), so only PCM gets a progress bar
                streamPcm(pcm, sink, reportProgress = isPcm)
            } finally {
                if (pcm !== base) pcm.close()
            }
        }
    }

    /** Asks the installed providers for a 16-bit PCM view of [base]; falls back to whatever PCM format they offer. */
    private fun toPcm(base: AudioInputStream): AudioInputStream {
        val f = base.format
        val ch = f.channels.takeIf { it > 0 } ?: 2
        val rate = f.sampleRate
        val wanted = AudioFormat(AudioFormat.Encoding.PCM_SIGNED, rate, 16, ch, ch * 2, rate, false)
        try {
            return AudioSystem.getAudioInputStream(wanted, base)
        } catch (_: IllegalArgumentException) {
            // the provider has its own idea of the PCM format (e.g. the FLAC reader yields 24 bit)
        }
        for (c in AudioSystem.getTargetFormats(AudioFormat.Encoding.PCM_SIGNED, f)) {
            val bits = c.sampleSizeInBits.takeIf { it > 0 } ?: continue
            val cc = c.channels.takeIf { it > 0 } ?: ch
            val candidate = AudioFormat(AudioFormat.Encoding.PCM_SIGNED, rate, bits, cc, cc * ((bits + 7) / 8), rate, c.isBigEndian)
            try {
                return AudioSystem.getAudioInputStream(candidate, base)
            } catch (_: IllegalArgumentException) {
            }
        }
        throw DecodeException("Kodierung ${f.encoding} nicht dekodierbar")
    }

    private fun streamPcm(stream: AudioInputStream, sink: PcmSink, reportProgress: Boolean) {
        val f = stream.format
        val channels = f.channels
        val bits = f.sampleSizeInBits
        val bytes = bits / 8
        if (channels < 1 || bytes !in 1..4 || bits % 8 != 0 || f.sampleRate <= 0f) throw DecodeException("unbekanntes PCM-Format $f")
        val frameBytes = if (f.frameSize > 0) f.frameSize else channels * bytes
        val order = if (f.isBigEndian) ByteOrder.BIG_ENDIAN else ByteOrder.LITTLE_ENDIAN
        val isFloat = f.encoding == AudioFormat.Encoding.PCM_FLOAT
        val signed = f.encoding != AudioFormat.Encoding.PCM_UNSIGNED
        sink.onFormat(f.sampleRate.toInt(), channels)
        val total = stream.frameLength
        val chunkFrames = CHUNK_FRAMES
        val buf = ByteArray(chunkFrames * frameBytes)
        val out = FloatArray(chunkFrames * channels)
        var done = 0L
        var fill = 0
        while (true) {
            val n = stream.read(buf, fill, buf.size - fill)
            if (n < 0) break
            fill += n
            val frames = fill / frameBytes
            if (frames == 0) continue
            val bb = ByteBuffer.wrap(buf, 0, frames * frameBytes).order(order)
            for (i in 0 until frames * channels) {
                out[i] = when {
                    isFloat && bytes == 4 -> bb.float
                    isFloat -> bb.double.toFloat()
                    bytes == 1 -> if (signed) bb.get() / 128f else ((bb.get().toInt() and 0xff) - 128) / 128f
                    bytes == 2 -> bb.short / 32768f
                    bytes == 3 -> {
                        val first = bb.get().toInt()
                        val mid = bb.get().toInt() and 0xff
                        val last = bb.get().toInt()
                        val v = if (order == ByteOrder.LITTLE_ENDIAN) (last shl 16) or (mid shl 8) or (first and 0xff) else (first shl 16) or (mid shl 8) or (last and 0xff)
                        v / 8388608f
                    }
                    else -> bb.int / 2147483648f
                }
            }
            sink.onSamples(out, frames * channels)
            done += frames
            if (reportProgress && total > 0) sink.onProgress((done.toDouble() / total).toFloat().coerceIn(0f, 1f))
            val used = frames * frameBytes
            System.arraycopy(buf, used, buf, 0, fill - used)
            fill -= used
        }
    }

    private companion object {
        const val CHUNK_FRAMES = 16384

        fun findOnPath(name: String): String? {
            val path = System.getenv("PATH") ?: return null
            return path.split(File.pathSeparatorChar).map { File(it, name) }.firstOrNull { it.canExecute() }?.path
        }
    }
}

/** Streams a RIFF/WAVE file (PCM 8/16/24/32 bit, float 32/64 bit, WAVE_FORMAT_EXTENSIBLE) without loading it whole. */
internal object WavReader {
    private const val CHUNK_FRAMES = 16384

    fun decode(file: File, sink: PcmSink) {
        FileChannel.open(file.toPath(), StandardOpenOption.READ).use { ch ->
            val head = ByteBuffer.allocate(12).order(ByteOrder.LITTLE_ENDIAN)
            readFully(ch, head, 0)
            if (head.limit() < 12 || head.getInt(0) != 0x46464952 || head.getInt(8) != 0x45564157) throw DecodeException("keine WAV-Datei")  // "RIFF" ... "WAVE"
            var pos = 12L
            var tag = 0
            var channels = 0
            var rate = 0
            var bits = 0
            var blockAlign = 0
            var dataPos = -1L
            var dataLen = 0L
            val size = ch.size()
            val hdr = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN)
            while (pos + 8 <= size) {
                hdr.clear()
                if (readFully(ch, hdr, pos) < 8) break
                val id = hdr.getInt(0)
                val len = hdr.getInt(4).toLong() and 0xffffffffL
                val body = pos + 8
                if (id == 0x20746d66) {  // "fmt "
                    val n = minOf(len, 40L).toInt()
                    if (n < 16) throw DecodeException("fmt-Block zu kurz")
                    val b = ByteBuffer.allocate(n).order(ByteOrder.LITTLE_ENDIAN)
                    readFully(ch, b, body)
                    tag = b.getShort(0).toInt() and 0xffff
                    channels = b.getShort(2).toInt() and 0xffff
                    rate = b.getInt(4)
                    blockAlign = b.getShort(12).toInt() and 0xffff
                    bits = b.getShort(14).toInt() and 0xffff
                    if (tag == 0xFFFE && n >= 26) tag = b.getShort(24).toInt() and 0xffff  // sub-format GUID starts with the real tag
                } else if (id == 0x61746164) {  // "data"
                    dataPos = body
                    dataLen = if (len == 0L || body + len > size) size - body else len
                    break
                }
                pos = body + len + (len and 1L)
            }
            if (dataPos < 0 || channels == 0) throw DecodeException("WAV ohne Audiodaten")
            val isFloat = tag == 3
            if (tag != 1 && tag != 3) throw DecodeException("WAV-Kodierung $tag wird nicht unterstützt")
            if (bits !in intArrayOf(8, 16, 24, 32, 64) || (isFloat && bits != 32 && bits != 64) || (!isFloat && bits == 64)) throw DecodeException("$bits Bit nicht unterstützt")
            if (rate < 1000 || rate > 768_000) throw DecodeException("Abtastrate $rate Hz nicht unterstützt")
            val bytesPer = bits / 8
            val frameBytes = if (blockAlign >= channels * bytesPer) blockAlign else channels * bytesPer
            val totalFrames = dataLen / frameBytes
            sink.onFormat(rate, channels)
            val buf = ByteBuffer.allocate(CHUNK_FRAMES * frameBytes).order(ByteOrder.LITTLE_ENDIAN)
            val out = FloatArray(CHUNK_FRAMES * channels)
            var frameIndex = 0L
            while (frameIndex < totalFrames) {
                val want = minOf(CHUNK_FRAMES.toLong(), totalFrames - frameIndex).toInt()
                buf.clear()
                buf.limit(want * frameBytes)
                val got = readFully(ch, buf, dataPos + frameIndex * frameBytes) / frameBytes
                if (got <= 0) break
                for (f in 0 until got) {
                    val base = f * frameBytes
                    for (c in 0 until channels) {
                        val o = base + c * bytesPer
                        out[f * channels + c] = when {
                            isFloat && bits == 32 -> buf.getFloat(o)
                            isFloat -> buf.getDouble(o).toFloat()
                            bits == 8 -> ((buf.get(o).toInt() and 0xff) - 128) / 128f
                            bits == 16 -> buf.getShort(o) / 32768f
                            bits == 24 -> ((buf.get(o + 2).toInt() shl 16) or ((buf.get(o + 1).toInt() and 0xff) shl 8) or (buf.get(o).toInt() and 0xff)) / 8388608f
                            else -> buf.getInt(o) / 2147483648f
                        }
                    }
                }
                sink.onSamples(out, got * channels)
                frameIndex += got
                sink.onProgress((frameIndex.toDouble() / totalFrames).toFloat().coerceIn(0f, 1f))
                if (got < want) break
            }
        }
    }

    /** Reads until [buf] (up to its limit) is full or the file ends; leaves the position at the end of the data, returns bytes read. */
    private fun readFully(ch: FileChannel, buf: ByteBuffer, position: Long): Int {
        var total = 0
        var pos = position
        while (buf.hasRemaining()) {
            val n = ch.read(buf, pos)
            if (n < 0) break
            total += n
            pos += n
        }
        buf.flip()
        return total
    }
}
