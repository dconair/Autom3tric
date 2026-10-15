package com.autodj.desktop

import com.autodj.app.dj.DecodeException
import com.autodj.app.dj.PcmSink
import org.junit.Assume.assumeTrue
import java.io.ByteArrayOutputStream
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.sqrt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/** Decoding of real files into PCM: our own WAV reader for every PCM flavour, and the compressed formats via the classpath providers / the system. */
class DesktopTrackDecoderTest {
    private class Collect : PcmSink {
        var rate = 0
        var channels = 0
        var data = FloatArray(0)
        var size = 0
        var lastProgress = 0f
        override fun onFormat(sampleRate: Int, channels: Int) {
            rate = sampleRate
            this.channels = channels
        }

        override fun onSamples(data: FloatArray, count: Int) {
            if (size + count > this.data.size) this.data = this.data.copyOf(maxOf(size + count, this.data.size * 2))
            System.arraycopy(data, 0, this.data, size, count)
            size += count
        }

        override fun onProgress(fraction: Float) {
            lastProgress = fraction
        }

        val frames: Int get() = if (channels == 0) 0 else size / channels

        /** Amplitude of the sine component at [freq] in [channel] (Goertzel on the whole signal, skipping [skip] frames at both ends). */
        fun amplitude(channel: Int, freq: Double, skip: Int = 0): Double {
            val n = frames - 2 * skip
            val w = 2.0 * PI * freq / rate
            var re = 0.0
            var im = 0.0
            for (i in 0 until n) {
                val v = data[(skip + i) * channels + channel].toDouble()
                re += v * cos(w * i)
                im -= v * sin(w * i)
            }
            return 2.0 * sqrt(re * re + im * im) / n
        }
    }

    private fun fixture(name: String): File {
        val dir = System.getProperty("autodj.fixtureDir") ?: "src/test/fixtures"
        return File(dir, name).also { assertTrue(it.isFile, "fixture $name missing in $dir") }
    }

    private fun decode(file: File, decoder: DesktopTrackDecoder = DesktopTrackDecoder()): Collect =
        Collect().also { decoder.decode(file.path, it) }

    // ---- WAV ------------------------------------------------------------------------------------

    private fun wav(bits: Int, float: Boolean, channels: Int, rate: Int, extensible: Boolean, samples: (frame: Int, ch: Int) -> Double, frames: Int): ByteArray {
        val bytesPer = bits / 8
        val data = ByteBuffer.allocate(frames * channels * bytesPer).order(ByteOrder.LITTLE_ENDIAN)
        for (f in 0 until frames) for (c in 0 until channels) {
            val v = samples(f, c)
            when {
                float && bits == 32 -> data.putFloat(v.toFloat())
                float -> data.putDouble(v)
                bits == 8 -> data.put((v * 127.0 + 128.0).toInt().toByte())
                bits == 16 -> data.putShort((v * 32767.0).toInt().toShort())
                bits == 24 -> {
                    val x = (v * 8388607.0).toInt()
                    data.put(x.toByte()).put((x shr 8).toByte()).put((x shr 16).toByte())
                }
                else -> data.putInt((v * 2147483647.0).toLong().toInt())
            }
        }
        val fmt = ByteBuffer.allocate(if (extensible) 40 else 16).order(ByteOrder.LITTLE_ENDIAN)
        fmt.putShort(if (extensible) 0xFFFE.toShort() else (if (float) 3 else 1).toShort())
        fmt.putShort(channels.toShort()).putInt(rate).putInt(rate * channels * bytesPer).putShort((channels * bytesPer).toShort()).putShort(bits.toShort())
        if (extensible) {
            fmt.putShort(22).putShort(bits.toShort()).putInt(3)
            fmt.putShort((if (float) 3 else 1).toShort())
            fmt.put(byteArrayOf(0, 0, 0, 0, 0x10, 0, 0x80.toByte(), 0, 0, 0xAA.toByte(), 0, 0x38, 0x9B.toByte(), 0x71))
        }
        val out = ByteArrayOutputStream()
        fun le32(v: Int) = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array()
        out.write("RIFF".toByteArray()); out.write(le32(4 + 8 + fmt.capacity() + 8 + data.capacity())); out.write("WAVE".toByteArray())
        out.write("fmt ".toByteArray()); out.write(le32(fmt.capacity())); out.write(fmt.array())
        out.write("LIST".toByteArray()); out.write(le32(4)); out.write("INFO".toByteArray())  // an unrelated chunk in between must be skipped
        out.write("data".toByteArray()); out.write(le32(data.capacity())); out.write(data.array())
        return out.toByteArray()
    }

    @Test fun wavInEveryFlavourDecodesToTheSameSignal() {
        val dir = File(System.getProperty("java.io.tmpdir"), "autodj-wav-test").apply { mkdirs() }
        val frames = 30000  // more than one read chunk
        val sine = { f: Int, c: Int -> (if (c == 0) 0.6 else 0.3) * sin(2.0 * PI * 1000.0 * f / 48000.0) }
        for ((bits, float) in listOf(8 to false, 16 to false, 24 to false, 32 to false, 32 to true, 64 to true)) {
            for (ext in listOf(false, true)) {
                for (ch in listOf(1, 2)) {
                    val f = File(dir, "t_${bits}_${float}_${ext}_$ch.wav")
                    f.writeBytes(wav(bits, float, ch, 48000, ext, sine, frames))
                    val got = decode(f)
                    val label = "bits=$bits float=$float extensible=$ext channels=$ch"
                    assertEquals(48000, got.rate, label)
                    assertEquals(ch, got.channels, label)
                    assertEquals(frames, got.frames, label)
                    val tol = if (bits == 8) 0.02 else 0.002
                    assertEquals(0.6, got.amplitude(0, 1000.0), tol, "$label left amplitude")
                    if (ch == 2) assertEquals(0.3, got.amplitude(1, 1000.0), tol, "$label right amplitude")
                    f.delete()
                }
            }
        }
        dir.delete()
    }

    @Test fun truncatedAndGarbageFilesFailWithAReadableMessage() {
        val dir = File(System.getProperty("java.io.tmpdir"), "autodj-bad-test").apply { mkdirs() }
        val txt = File(dir, "notes.mp3").apply { writeText("this is not audio at all") }
        val e = assertFailsWith<DecodeException> { decode(txt, DesktopTrackDecoder(afconvert = null, ffmpeg = null)) }
        assertTrue(e.message!!.contains("nicht unterstützt"), e.message)
        val empty = File(dir, "empty.wav").apply { writeBytes(ByteArray(0)) }
        assertFailsWith<DecodeException> { decode(empty) }
        assertFailsWith<DecodeException> { decode(File(dir, "missing.flac")) }
        // a WAV cut in the middle of the data still yields the part that is there
        val full = wav(16, false, 2, 44100, false, { f, _ -> 0.5 * sin(2.0 * PI * 440.0 * f / 44100.0) }, 40000)
        val cut = File(dir, "cut.wav").apply { writeBytes(full.copyOf(full.size - 20000)) }
        val got = decode(cut)
        assertTrue(got.frames in 34000..36000, "frames=${got.frames}")
        dir.listFiles()?.forEach { it.delete() }
        dir.delete()
    }

    // ---- compressed formats ---------------------------------------------------------------------

    private fun checkTone(got: Collect, label: String, expectedFrames: Int, frameSlack: Int) {
        assertEquals(44100, got.rate, label)
        assertEquals(2, got.channels, label)
        assertTrue(kotlin.math.abs(got.frames - expectedFrames) <= frameSlack, "$label: ${got.frames} frames, expected about $expectedFrames")
        val skip = 4000  // encoder delay / priming
        assertEquals(0.5, got.amplitude(0, 440.0, skip), 0.05, "$label left")
        assertEquals(0.25, got.amplitude(1, 440.0, skip), 0.03, "$label right")
        assertTrue(got.lastProgress > 0.9f || got.lastProgress == 0f, "$label progress ${got.lastProgress}")
    }

    @Test fun mp3ThroughJavaSound() = checkTone(decode(fixture("tone.mp3"), DesktopTrackDecoder(afconvert = null, ffmpeg = null)), "mp3", 88200, 3000)

    @Test fun flacThroughJavaSound() = checkTone(decode(fixture("tone.flac"), DesktopTrackDecoder(afconvert = null, ffmpeg = null)), "flac", 88200, 5)

    @Test fun oggThroughJavaSound() = checkTone(decode(fixture("tone.ogg"), DesktopTrackDecoder(afconvert = null, ffmpeg = null)), "ogg", 88200, 3000)

    @Test fun defaultDecoderHandlesAllFixtures() {
        val d = DesktopTrackDecoder()
        for (name in listOf("tone.mp3", "tone.flac", "tone.ogg")) checkTone(decode(fixture(name), d), name, 88200, 3000)
    }

    @Test fun m4aThroughTheSystemConverter() {
        val d = DesktopTrackDecoder()
        val hasConverter = File("/usr/bin/afconvert").canExecute() || System.getenv("PATH").orEmpty().split(File.pathSeparator).any { File(it, "ffmpeg").canExecute() }
        assumeTrue("neither afconvert nor ffmpeg available", hasConverter)
        checkTone(decode(fixture("tone.m4a"), d), "m4a", 88200, 3000)
    }

    @Test fun displayNameDropsDirectoryAndExtension() {
        val d = DesktopTrackDecoder(afconvert = null, ffmpeg = null)
        assertEquals("My Song - Remix", d.displayName("/music/dir.with.dots/My Song - Remix.flac"))
        assertEquals("noext", d.displayName("/tmp/noext"))
    }
}
