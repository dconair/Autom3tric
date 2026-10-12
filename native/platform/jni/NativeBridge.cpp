// JNI bridge. Thin on purpose: every call is a control-thread call that forwards to Engine / AudioDriver.
// The audio callback (AudioDriver::onAudioReady) never touches JNI.
//
// Conventions: handle = pointer to Handle as jlong. All strings are UTF-8 JSON. Poll calls fill preallocated
// primitive arrays supplied by Kotlin, so the 30 Hz UI poll does not allocate on the Java side either.

#ifdef __ANDROID__
#include <android/log.h>
#else
#include <cstdio>
#endif
#include <jni.h>

#include <atomic>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "AudioDriver.h"
#include "BenchCore.h"
#include "autodj/core/DemoSources.h"
#include "autodj/core/RtGuard.h"
#include "autodj/engine/Engine.h"

using autodj::Engine;
using autodj::EngineConfig;
using autodj::platform::AudioDriver;
using autodj::platform::DriverOptions;

namespace {

struct Handle {
  std::unique_ptr<Engine> engine;
  std::unique_ptr<AudioDriver> driver;
};

Handle* H(jlong h) { return reinterpret_cast<Handle*>(h); }

jstring toJString(JNIEnv* env, const std::string& s) { return env->NewStringUTF(s.c_str()); }

std::string fromJString(JNIEnv* env, jstring s) {
  if (!s) return {};
  const char* c = env->GetStringUTFChars(s, nullptr);
  std::string out = c ? c : "";
  if (c) env->ReleaseStringUTFChars(s, c);
  return out;
}

std::atomic<int> g_benchProgress{0};

// Debug builds (AUTODJ_RT_GUARD=1): a violation is logged and counted, shown in the debug overlay, and the app keeps running.
#if AUTODJ_RT_GUARD
void violationHandler(const char* what) noexcept {
#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_ERROR, "AutoDJ", "RT VIOLATION on audio thread: %s", what);
#else
  std::fprintf(stderr, "AutoDJ RT VIOLATION on audio thread: %s\n", what);
#endif
}
#endif

std::string dumpJson(const nlohmann::json& j) { return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace); }

void benchProgress(void*, int done, int total) {
  g_benchProgress.store(total > 0 ? 100 * done / total : 0, std::memory_order_relaxed);
}

}  // namespace

extern "C" {

#define JFN(ret, name) JNIEXPORT ret JNICALL Java_com_autodj_app_audio_NativeEngine_##name

JFN(jlong, nativeCreate)(JNIEnv*, jclass, jint sampleRate, jint voices) {
  EngineConfig cfg;
  cfg.sampleRate = sampleRate;
  cfg.granularVoices = voices;
  cfg.realtime = true;
#if AUTODJ_RT_GUARD
  autodj::rt::setViolationHandler(&violationHandler);
#endif
  auto* h = new Handle();
  h->engine = std::make_unique<Engine>(cfg);
  h->driver = std::make_unique<AudioDriver>(*h->engine);
  return reinterpret_cast<jlong>(h);
}

JFN(void, nativeDestroy)(JNIEnv*, jclass, jlong h) {
  if (!h) return;
  Handle* p = H(h);
  p->driver.reset();  // stops the stream before the engine goes away
  p->engine.reset();
  delete p;
}

JFN(jint, nativeStart)(JNIEnv*, jclass, jlong h, jboolean exclusive, jint deviceId) {
  DriverOptions o;
  o.exclusive = exclusive;
  o.deviceId = deviceId;
  o.sampleRate = H(h)->engine->sampleRate();
  return static_cast<jint>(H(h)->driver->start(o));
}

JFN(void, nativeStop)(JNIEnv*, jclass, jlong h) { H(h)->driver->stop(); }

JFN(jstring, nativeDriverInfo)(JNIEnv* env, jclass, jlong h) {
  const auto i = H(h)->driver->info();
  nlohmann::json j = {{"running", i.running},       {"api", i.api},
                      {"sharing", i.sharing},       {"performance", i.performance},
                      {"sampleRate", i.sampleRate}, {"framesPerBurst", i.framesPerBurst},
                      {"framesPerCallback", i.framesPerCallback},
                      {"bufferSize", i.bufferSize}, {"bufferCapacity", i.bufferCapacity},
                      {"latencyMs", i.latencyMs},   {"xruns", i.xruns},
                      {"restarts", i.restarts},     {"deviceId", i.deviceId},
                      {"performanceHint", i.performanceHint}, {"lastError", i.lastError},
                      {"simd", autodj::granular::kernelBackendName()},
                      {"rtGuard", AUTODJ_RT_GUARD != 0},
                      {"rtViolations", autodj::rt::violationCount()}};
  return toJString(env, dumpJson(j));
}

// ---- parameters -------------------------------------------------------------------------------

JFN(jstring, nativeParamSpecs)(JNIEnv* env, jclass, jlong h) {
  return toJString(env, H(h)->engine->registry().specsToJson());
}

JFN(void, nativeSetParam)(JNIEnv*, jclass, jlong h, jint id, jfloat v) {
  H(h)->engine->params().set(static_cast<autodj::ParamId>(id), v);
}

JFN(void, nativeSetParamNormalized)(JNIEnv*, jclass, jlong h, jint id, jfloat n) {
  H(h)->engine->params().setNormalized(static_cast<autodj::ParamId>(id), n);
}

JFN(jfloat, nativeGetParam)(JNIEnv*, jclass, jlong h, jint id) {
  return H(h)->engine->params().get(static_cast<autodj::ParamId>(id));
}

JFN(jstring, nativeExportPreset)(JNIEnv* env, jclass, jlong h, jstring name) {
  return toJString(env, H(h)->engine->exportPreset(fromJString(env, name)));
}

JFN(jstring, nativeImportPreset)(JNIEnv* env, jclass, jlong h, jstring json) {
  const auto r = H(h)->engine->importPreset(fromJString(env, json));
  nlohmann::json j = {{"ok", r.ok},           {"error", r.error},
                      {"fromVersion", r.fromVersion}, {"applied", r.applied},
                      {"unknownKeys", r.unknownKeys}, {"clampedKeys", r.clampedKeys}};
  return toJString(env, dumpJson(j));
}

// ---- transport / notes / sources --------------------------------------------------------------

JFN(void, nativePlay)(JNIEnv*, jclass, jlong h) { H(h)->engine->play(); }
JFN(void, nativeStopTransport)(JNIEnv*, jclass, jlong h) { H(h)->engine->stop(); }
JFN(void, nativeSeek)(JNIEnv*, jclass, jlong h, jdouble beats) { H(h)->engine->seekBeats(beats); }
JFN(void, nativeNoteOn)(JNIEnv*, jclass, jlong h, jint voice, jfloat pitch, jfloat vel) {
  H(h)->engine->noteOn(voice, pitch, vel);
}
JFN(void, nativeNoteOff)(JNIEnv*, jclass, jlong h, jint voice) { H(h)->engine->noteOff(voice); }
JFN(void, nativeResetEngine)(JNIEnv*, jclass, jlong h) { H(h)->engine->resetEngine(); }
JFN(void, nativeResetSteps)(JNIEnv*, jclass, jlong h, jint voice) { H(h)->engine->resetSteps(voice); }
JFN(jint, nativeCollectGarbage)(JNIEnv*, jclass, jlong h) { return static_cast<jint>(H(h)->engine->collectGarbage()); }

/// Built-in synthetic sources until the decoder arrives in milestone 3: 0 chord pad, 1 drum loop, 2 noise, 3 sine.
JFN(jboolean, nativeLoadDemoSource)(JNIEnv*, jclass, jlong h, jint slot, jint kind) {
  Engine& e = *H(h)->engine;
  const int sr = e.sampleRate();
  std::shared_ptr<const autodj::SampleBuffer> b;
  switch (kind) {
    case 0: b = autodj::demo::chordPad(12.0, sr); break;
    case 1: b = autodj::demo::drumLoop(120.0, 4, sr); break;
    case 2: b = autodj::demo::noise(4.0, sr, 0.5f); break;
    default: b = autodj::demo::sine(220.0, 6.0, sr, 0.5f, 220.7); break;
  }
  return e.setSource(slot, std::move(b)) ? JNI_TRUE : JNI_FALSE;
}

/// Waveform overview of a source slot: out receives 2 * columns floats (min, max per column). false = slot empty.
JFN(jboolean, nativeSourcePeaks)(JNIEnv* env, jclass, jlong h, jint slot, jint columns, jfloatArray out) {
  if (columns <= 0 || columns > 4096 || env->GetArrayLength(out) < 2 * columns) return JNI_FALSE;
  static thread_local float buf[2 * 4096];
  if (!H(h)->engine->sourcePeaks(slot, columns, buf)) return JNI_FALSE;
  env->SetFloatArrayRegion(out, 0, 2 * columns, buf);
  return JNI_TRUE;
}

/// {"name": ..., "seconds": ..., "sampleRate": ...} of a source slot, or "" when it is empty.
JFN(jstring, nativeSourceInfo)(JNIEnv* env, jclass, jlong h, jint slot) {
  double seconds = 0.0;
  int sr = 0;
  std::string name;
  if (!H(h)->engine->sourceInfo(slot, &seconds, &sr, &name)) return toJString(env, "");
  nlohmann::json j = {{"name", name}, {"seconds", seconds}, {"sampleRate", sr}};
  return toJString(env, dumpJson(j));
}

// ---- polling ----------------------------------------------------------------------------------

/// out layout (18 doubles) is mirrored by EngineStatus.fromArray() in Kotlin.
JFN(jboolean, nativePollStatus)(JNIEnv* env, jclass, jlong h, jdoubleArray out) {
  autodj::EngineStatus s;
  if (!H(h)->engine->readStatus(s)) return JNI_FALSE;
  const double v[18] = {s.load,        s.loadPeak,       s.lastBlockLoad, s.peakL,
                        s.peakR,       double(s.deadlineMisses), double(s.xruns), double(s.droppedGrains),
                        double(s.callbacks), s.clockBeat,  s.playBeat,      s.bpm,
                        double(s.playing), double(s.activeGrains), double(s.activeVoices), double(s.qualityLevel),
                        double(s.callbackFrames), double(s.sampleRate)};
  env->SetDoubleArrayRegion(out, 0, 18, v);
  return JNI_TRUE;
}

/// out layout: [count, voices, playhead x4, level x4, cell x4, then count x (pos, size, pan, age, rate, voice)].
JFN(jboolean, nativePollViz)(JNIEnv* env, jclass, jlong h, jfloatArray out) {
  static thread_local autodj::GrainViz z;
  if (!H(h)->engine->readGrainViz(z)) return JNI_FALSE;
  constexpr int kHead = 2 + 3 * autodj::kVizMaxVoices;
  float buf[kHead + 6 * autodj::kVizMaxGrains];
  buf[0] = static_cast<float>(z.count);
  buf[1] = static_cast<float>(z.voices);
  for (int i = 0; i < autodj::kVizMaxVoices; ++i) {
    buf[2 + i] = z.playhead[i];
    buf[2 + autodj::kVizMaxVoices + i] = z.voiceLevel[i];
    buf[2 + 2 * autodj::kVizMaxVoices + i] = static_cast<float>(z.cell[i]);
  }
  for (int i = 0; i < z.count; ++i) {
    const auto& g = z.grains[i];
    float* d = buf + kHead + 6 * i;
    d[0] = g.pos; d[1] = g.size; d[2] = g.pan; d[3] = g.age; d[4] = g.rate; d[5] = g.voice;
  }
  env->SetFloatArrayRegion(out, 0, kHead + 6 * z.count, buf);
  return JNI_TRUE;
}

// ---- on-device benchmark ----------------------------------------------------------------------

/// Blocks the calling (Kotlin background) thread; returns {"markdown": ..., "json": {...}}.
JFN(jstring, nativeRunBenchmark)(JNIEnv* env, jclass, jboolean quick) {
  autodj::bench::Config cfg;
  if (quick) {
    cfg.seconds = 2.0;
    cfg.warmupSeconds = 0.5;
    cfg.repeats = 1;
  }
  g_benchProgress.store(0);
  const auto envInfo = autodj::bench::environment();
  const auto results = autodj::bench::run(cfg, &benchProgress, nullptr);
  g_benchProgress.store(100);
  nlohmann::json out;
  out["markdown"] = autodj::bench::toMarkdown(envInfo, cfg, results);
  out["json"] = nlohmann::json::parse(autodj::bench::toJson(envInfo, cfg, results));
  return toJString(env, dumpJson(out));
}

JFN(jint, nativeBenchmarkProgress)(JNIEnv*, jclass) { return g_benchProgress.load(std::memory_order_relaxed); }

}  // extern "C"
