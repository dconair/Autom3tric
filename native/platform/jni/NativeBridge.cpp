// JNI bridge. Thin on purpose: every call is a control-thread call that forwards to Engine / AudioDriver.
// The audio callback (AudioDriver::onAudioReady) never touches JNI.
//
// Conventions: handle = pointer to EngineHandle as jlong. All strings are UTF-8 JSON. Poll calls fill preallocated
// primitive arrays supplied by Kotlin, so the 30 Hz UI poll does not allocate on the Java side either.

#ifdef __ANDROID__
#include <android/log.h>
#else
#include <cstdio>
#endif
#include <jni.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "AudioDriver.h"
#include "BenchCore.h"
#include "autodj/analysis/AnalysisJson.h"
#include "autodj/core/DemoSources.h"
#include "autodj/core/RtGuard.h"
#include "autodj/engine/Engine.h"
#include "autodj/engine/MixExport.h"
#include "autodj/engine/Showcase.h"

using autodj::Engine;
using autodj::EngineConfig;
using autodj::platform::AudioDriver;
using autodj::platform::DriverOptions;

namespace {

/// One decode job: PCM chunks arrive from a Kotlin worker thread, finish() converts, analyses and registers the track.
struct IngestJob {
  autodj::TrackIngest ingest;
  std::atomic<int> progress{0};  // percent, polled by the UI
  IngestJob(int rate, int channels) : ingest(rate, channels) {}
};

struct EngineHandle {
  std::unique_ptr<Engine> engine;
  std::unique_ptr<AudioDriver> driver;
  std::mutex ingestMutex;  // guards the map only, never the (long) conversion/analysis
  std::unordered_map<int, std::shared_ptr<IngestJob>> ingests;
  int nextTicket = 1;
  std::mutex djMutex;  // guards the two maps below (plans are built on worker threads, polled from the UI thread)
  std::unordered_map<int, std::shared_ptr<const autodj::dj::MixPlan>> plans;
  std::unordered_map<int, std::unique_ptr<autodj::MixExporter>> exports;
  int nextPlan = 1;
  int nextExport = 1;
};

EngineHandle* H(jlong h) { return reinterpret_cast<EngineHandle*>(h); }

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
  auto* h = new EngineHandle();
  h->engine = std::make_unique<Engine>(cfg);
  h->driver = std::make_unique<AudioDriver>(*h->engine);
  return reinterpret_cast<jlong>(h);
}

JFN(void, nativeDestroy)(JNIEnv*, jclass, jlong h) {
  if (!h) return;
  EngineHandle* p = H(h);
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

/// The four built-in sources of the instrument (see Showcase.h): 0 pad, 1 drums, 2 pluck, 3 bass.
JFN(jboolean, nativeLoadDemoSource)(JNIEnv*, jclass, jlong h, jint slot, jint kind) {
  Engine& e = *H(h)->engine;
  const int sr = e.sampleRate();
  std::shared_ptr<const autodj::SampleBuffer> b = autodj::showcase::make(kind, sr);
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

/// out layout: [count, voices, playhead x4, level x4, cell x4, step x4, then count x (pos, size, pan, age, rate, voice)].
JFN(jboolean, nativePollViz)(JNIEnv* env, jclass, jlong h, jfloatArray out) {
  static thread_local autodj::GrainViz z;
  if (!H(h)->engine->readGrainViz(z)) return JNI_FALSE;
  constexpr int kHead = 2 + 4 * autodj::kVizMaxVoices;
  float buf[kHead + 6 * autodj::kVizMaxGrains];
  buf[0] = static_cast<float>(z.count);
  buf[1] = static_cast<float>(z.voices);
  for (int i = 0; i < autodj::kVizMaxVoices; ++i) {
    buf[2 + i] = z.playhead[i];
    buf[2 + autodj::kVizMaxVoices + i] = z.voiceLevel[i];
    buf[2 + 2 * autodj::kVizMaxVoices + i] = static_cast<float>(z.cell[i]);
    buf[2 + 3 * autodj::kVizMaxVoices + i] = static_cast<float>(z.step[i]);
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

// ---- tracks: import, analysis, waveform ---------------------------------------------------------

namespace {

nlohmann::json trackJson(const autodj::Track& t, int trackId, int overviewColumns) {
  nlohmann::json j = {{"ok", true},
                      {"trackId", trackId},
                      {"name", t.name},
                      {"durationSec", t.durationSec()},
                      {"analysis", autodj::analysis::toJson(t.analysis)}};
  if (overviewColumns > 0) {
    std::vector<int> mn, mx;
    autodj::trackOverview(t, overviewColumns, mn, mx);
    j["overview"] = {{"min", mn}, {"max", mx}};
  }
  return j;
}

/// Registers the track, returns its JSON and (if !keep) drops it again, so the library only holds analysis results.
std::string finishTrack(Engine& e, std::shared_ptr<const autodj::SampleBuffer> audio, const std::string& name,
                        const autodj::analysis::TrackAnalysis* cached, bool keep, int overviewColumns,
                        const autodj::analysis::ProgressFn& progress) {
  if (!audio) return dumpJson({{"ok", false}, {"error", "no audio data (the file could not be decoded or is empty)"}});
  const int id = e.addTrack(std::move(audio), name, cached, progress);
  if (id == 0) return dumpJson({{"ok", false}, {"error", "track could not be registered"}});
  const auto t = e.track(id);
  const std::string out = dumpJson(trackJson(*t, keep ? id : 0, overviewColumns));
  if (!keep) e.removeTrack(id);
  return out;
}

}  // namespace

extern "C" {

JFN(jint, nativeIngestBegin)(JNIEnv*, jclass, jlong h, jint srcRate, jint channels) {
  EngineHandle* p = H(h);
  if (srcRate < 8000 || srcRate > 384000 || channels < 1 || channels > 16) return 0;
  std::lock_guard<std::mutex> lk(p->ingestMutex);
  const int t = p->nextTicket++;
  p->ingests[t] = std::make_shared<IngestJob>(srcRate, channels);
  return t;
}

JFN(jboolean, nativeIngestAppend)(JNIEnv* env, jclass, jlong h, jint ticket, jfloatArray data, jint count) {
  EngineHandle* p = H(h);
  std::shared_ptr<IngestJob> job;
  {
    std::lock_guard<std::mutex> lk(p->ingestMutex);
    auto it = p->ingests.find(ticket);
    if (it == p->ingests.end()) return JNI_FALSE;
    job = it->second;
  }
  if (count <= 0 || env->GetArrayLength(data) < count) return JNI_FALSE;
  jfloat* src = env->GetFloatArrayElements(data, nullptr);
  if (!src) return JNI_FALSE;
  job->ingest.append(src, static_cast<std::size_t>(count));
  env->ReleaseFloatArrayElements(data, src, JNI_ABORT);
  return JNI_TRUE;
}

/// Converts to stereo at the engine rate, analyses (or adopts `cachedJson`) and returns
/// {"ok", "trackId", "name", "durationSec", "analysis": {...}, "overview": {"min": [...], "max": [...]}}.
/// keep=false forgets the audio again (library import: only the analysis is stored); keep=true leaves the track
/// registered under "trackId" for decks / the granular source.
JFN(jstring, nativeIngestFinish)(JNIEnv* env, jclass, jlong h, jint ticket, jstring name, jstring cachedJson, jboolean keep,
                                 jint overviewColumns) {
  EngineHandle* p = H(h);
  std::shared_ptr<IngestJob> job;
  {
    std::lock_guard<std::mutex> lk(p->ingestMutex);
    auto it = p->ingests.find(ticket);
    if (it == p->ingests.end()) return toJString(env, dumpJson({{"ok", false}, {"error", "unknown ticket"}}));
    job = it->second;
    p->ingests.erase(it);
  }
  const std::string nm = fromJString(env, name);
  const std::string cj = fromJString(env, cachedJson);
  autodj::analysis::TrackAnalysis cached;
  bool haveCached = false;
  if (!cj.empty()) {
    const auto parsed = nlohmann::json::parse(cj, nullptr, false);
    haveCached = !parsed.is_discarded() && autodj::analysis::fromJson(parsed, cached);
  }
  job->progress.store(5);
  auto audio = job->ingest.finish(p->engine->sampleRate(), nm);
  job->progress.store(15);
  IngestJob* raw = job.get();
  const auto prog = [raw](float f) { raw->progress.store(15 + static_cast<int>(85.f * f)); };
  const std::string out = finishTrack(*p->engine, std::move(audio), nm, haveCached ? &cached : nullptr, keep == JNI_TRUE,
                                      overviewColumns, prog);
  job->progress.store(100);
  return toJString(env, out);
}

JFN(void, nativeIngestCancel)(JNIEnv*, jclass, jlong h, jint ticket) {
  EngineHandle* p = H(h);
  std::lock_guard<std::mutex> lk(p->ingestMutex);
  p->ingests.erase(ticket);
}

/// Percent of the current analysis (only meaningful while nativeIngestFinish runs on another thread).
JFN(jint, nativeIngestProgress)(JNIEnv*, jclass, jlong h, jint ticket) {
  EngineHandle* p = H(h);
  std::lock_guard<std::mutex> lk(p->ingestMutex);
  auto it = p->ingests.find(ticket);
  return it == p->ingests.end() ? 100 : it->second->progress.load();
}

JFN(jstring, nativeTrackJson)(JNIEnv* env, jclass, jlong h, jint trackId, jint overviewColumns) {
  const auto t = H(h)->engine->track(trackId);
  if (!t) return toJString(env, dumpJson({{"ok", false}, {"error", "unknown track"}}));
  return toJString(env, dumpJson(trackJson(*t, trackId, overviewColumns)));
}

/// Min/max waveform of [startSec, endSec) in `columns` columns (2 floats each) from the peak pyramid.
JFN(jboolean, nativeTrackPeaks)(JNIEnv* env, jclass, jlong h, jint trackId, jdouble startSec, jdouble endSec, jint columns,
                                jfloatArray out) {
  if (columns <= 0 || columns > 8192 || env->GetArrayLength(out) < 2 * columns) return JNI_FALSE;
  const auto t = H(h)->engine->track(trackId);
  if (!t || !t->audio) return JNI_FALSE;
  const double sr = t->audio->sampleRate();
  std::vector<float> buf(static_cast<std::size_t>(columns) * 2);
  t->peaks.query(*t->audio, static_cast<std::int64_t>(startSec * sr), static_cast<std::int64_t>(endSec * sr), columns, buf.data());
  env->SetFloatArrayRegion(out, 0, 2 * columns, buf.data());
  return JNI_TRUE;
}

JFN(void, nativeTrackRelease)(JNIEnv*, jclass, jlong h, jint trackId) { H(h)->engine->removeTrack(trackId); }

/// Uses a registered track as the granular instrument's source in `slot` (0..15).
JFN(jboolean, nativeTrackToSource)(JNIEnv*, jclass, jlong h, jint trackId, jint slot) {
  const auto t = H(h)->engine->track(trackId);
  if (!t || !t->audio) return JNI_FALSE;
  return H(h)->engine->setSource(slot, t->audio) ? JNI_TRUE : JNI_FALSE;
}

/// Built-in synthetic songs (no file needed): kinds 0..3 differ in tempo and key. Same JSON as nativeIngestFinish.
JFN(jstring, nativeAddDemoTrack)(JNIEnv* env, jclass, jlong h, jint kind, jboolean keep, jint overviewColumns) {
  struct Demo { const char* name; double bpm; int root; bool minor; unsigned seed; };
  static const Demo demos[4] = {
      {"Demo 1 - Warm-up (A minor)", 118.0, 57, true, 1}, {"Demo 2 - Groove (E minor)", 122.0, 52, true, 2},
      {"Demo 3 - Peak (C major)", 126.0, 60, false, 3},   {"Demo 4 - Late (D minor)", 124.0, 50, true, 4}};
  const Demo& d = demos[(kind % 4 + 4) % 4];
  autodj::demo::MusicSpec spec;
  spec.bpm = d.bpm;
  spec.rootMidi = d.root;
  spec.minor = d.minor;
  spec.seed = d.seed;
  spec.sampleRate = H(h)->engine->sampleRate();
  spec.name = d.name;
  return toJString(env, finishTrack(*H(h)->engine, autodj::demo::musicTrack(spec), d.name, nullptr, keep == JNI_TRUE,
                                    overviewColumns, {}));
}

/// Renders `frames` stereo frames offline into `out` (tests, export). Only while no audio driver is running.
JFN(jint, nativeRenderOffline)(JNIEnv* env, jclass, jlong h, jint frames, jfloatArray out) {
  if (frames <= 0 || env->GetArrayLength(out) < 2 * frames) return 0;
  std::vector<float> buf(static_cast<std::size_t>(frames) * 2);
  H(h)->engine->process(buf.data(), frames);
  env->SetFloatArrayRegion(out, 0, 2 * frames, buf.data());
  return frames;
}

// ---- DJ: decks, automatic mix, offline export -------------------------------------------------------------------

/// A library entry without audio: only enough to plan a mix with. Returns the track id, 0 if the analysis JSON is unusable.
JFN(jint, nativeTrackStub)(JNIEnv* env, jclass, jlong h, jstring name, jstring analysisJson) {
  autodj::analysis::TrackAnalysis a;
  const auto parsed = nlohmann::json::parse(fromJString(env, analysisJson), nullptr, false);
  if (parsed.is_discarded() || !autodj::analysis::fromJson(parsed, a)) return 0;
  return H(h)->engine->addTrackStub(fromJString(env, name), a);
}

JFN(jboolean, nativeDeckLoad)(JNIEnv*, jclass, jlong h, jint deck, jint trackId, jdouble startSec) {
  if (trackId <= 0) return H(h)->engine->deckLoad(deck, nullptr, 0.0) ? JNI_TRUE : JNI_FALSE;
  auto t = H(h)->engine->track(trackId);
  if (!t || !t->audio) return JNI_FALSE;
  return H(h)->engine->deckLoad(deck, std::move(t), startSec) ? JNI_TRUE : JNI_FALSE;
}
JFN(jboolean, nativeDeckPlay)(JNIEnv*, jclass, jlong h, jint deck, jboolean play) {
  return H(h)->engine->deckPlay(deck, play == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
}
JFN(jboolean, nativeDeckSeek)(JNIEnv*, jclass, jlong h, jint deck, jdouble sec) {
  return H(h)->engine->deckSeek(deck, sec) ? JNI_TRUE : JNI_FALSE;
}

// ---- drum pads (the effects are plain parameters "fx.*" / "pads.*") ----
JFN(jboolean, nativePadTrigger)(JNIEnv*, jclass, jlong h, jint pad, jfloat velocity) {
  return H(h)->engine->padTrigger(pad, velocity) ? JNI_TRUE : JNI_FALSE;
}
JFN(jboolean, nativePadRow)(JNIEnv*, jclass, jlong h, jint pad, jint mask) {
  return H(h)->engine->padSetRow(pad, static_cast<unsigned>(mask)) ? JNI_TRUE : JNI_FALSE;
}

JFN(jboolean, nativeGranularSteps)(JNIEnv*, jclass, jlong h, jint voice, jint mask, jlong cells) {
  return H(h)->engine->granularSetSteps(voice, static_cast<unsigned>(mask), static_cast<std::uint64_t>(cells)) ? JNI_TRUE : JNI_FALSE;
}

/// The last `seconds` of the output become granular source `slot` ("freeze"). False while nothing has been rendered yet.
JFN(jboolean, nativeCaptureToSource)(JNIEnv* env, jclass, jlong h, jint slot, jdouble seconds, jstring name) {
  auto buf = H(h)->engine->captureOutput(seconds, fromJString(env, name));
  if (!buf) return JNI_FALSE;
  return H(h)->engine->setSource(slot, std::move(buf)) ? JNI_TRUE : JNI_FALSE;
}

/// Suggested order: indices into `ids`.
JFN(jintArray, nativeOrderTracks)(JNIEnv* env, jclass, jlong h, jintArray ids, jint mode) {
  const jsize n = env->GetArrayLength(ids);
  std::vector<int> v(static_cast<std::size_t>(n));
  if (n > 0) env->GetIntArrayRegion(ids, 0, n, v.data());
  const auto order = H(h)->engine->orderTracks(v, static_cast<autodj::dj::OrderMode>(std::clamp<int>(mode, 0, static_cast<int>(autodj::dj::OrderMode::kCount) - 1)));
  jintArray out = env->NewIntArray(static_cast<jsize>(order.size()));
  if (!order.empty()) env->SetIntArrayRegion(out, 0, static_cast<jsize>(order.size()), order.data());
  return out;
}

/// Plans a mix in the given order. type: 0 auto, 1 EQ blend, 2 crossfade, 3 filter sweep, 4 cut.
JFN(jstring, nativePlanCreate)(JNIEnv* env, jclass, jlong h, jintArray ids, jint type, jint lengthBeats, jint seed) {
  const jsize n = env->GetArrayLength(ids);
  std::vector<int> v(static_cast<std::size_t>(n));
  if (n > 0) env->GetIntArrayRegion(ids, 0, n, v.data());
  autodj::dj::PlanOptions o;
  o.type = static_cast<autodj::dj::TransitionType>(std::clamp<int>(type, 0, 4));
  o.lengthBeats = lengthBeats;
  o.seed = static_cast<std::uint32_t>(seed);
  auto plan = H(h)->engine->planMix(v, o);
  if (!plan || plan->items.empty()) return toJString(env, dumpJson({{"ok", false}, {"error", "no usable tracks"}}));
  EngineHandle* p = H(h);
  int id;
  {
    std::lock_guard<std::mutex> lk(p->djMutex);
    id = p->nextPlan++;
    p->plans[id] = plan;
  }
  nlohmann::json j = {{"ok", true}, {"planId", id}};
  j["plan"] = nlohmann::json::parse(autodj::dj::planToJson(*plan), nullptr, false);
  return toJString(env, dumpJson(j));
}

std::shared_ptr<const autodj::dj::MixPlan> findPlan(jlong h, int planId) {
  EngineHandle* p = H(h);
  std::lock_guard<std::mutex> lk(p->djMutex);
  auto it = p->plans.find(planId);
  return it == p->plans.end() ? nullptr : it->second;
}

/// Binds the decoded audio of a registered track to a plan item (the plan keeps it alive from then on).
JFN(jboolean, nativePlanBind)(JNIEnv*, jclass, jlong h, jint planId, jint item, jint trackId) {
  auto plan = findPlan(h, planId);
  auto t = H(h)->engine->track(trackId);
  return plan && t && plan->bindAudio(item, std::move(t)) ? JNI_TRUE : JNI_FALSE;
}
JFN(jboolean, nativePlanBound)(JNIEnv*, jclass, jlong h, jint planId, jint item) {
  auto plan = findPlan(h, planId);
  return plan && plan->audioBound(item) ? JNI_TRUE : JNI_FALSE;
}
JFN(void, nativePlanUnbind)(JNIEnv*, jclass, jlong h, jint planId, jint item) {
  if (auto plan = findPlan(h, planId)) plan->unbindAudio(item);
}
JFN(jboolean, nativePlanStart)(JNIEnv*, jclass, jlong h, jint planId) {
  auto plan = findPlan(h, planId);
  return plan && H(h)->engine->planStart(std::move(plan)) ? JNI_TRUE : JNI_FALSE;
}
JFN(void, nativePlanStop)(JNIEnv*, jclass, jlong h) { H(h)->engine->planStop(); }
JFN(void, nativePlanRelease)(JNIEnv*, jclass, jlong h, jint planId) {
  EngineHandle* p = H(h);
  std::lock_guard<std::mutex> lk(p->djMutex);
  p->plans.erase(planId);
}

/// out: per deck [loaded, playing, ended, trackId, posSec, durationSec, rate, bpm, beatPhase, peakL, peakR] x 2, then the plan
/// [active, finished, item, transition, waiting, timeSec, totalSec, beat, bpm], then the step of the pad sequencer (-1 = off)  (32 values).
JFN(jboolean, nativePollDj)(JNIEnv* env, jclass, jlong h, jdoubleArray out) {
  autodj::dj::DjStatus s;
  if (!H(h)->engine->readDjStatus(s) || env->GetArrayLength(out) < 32) return JNI_FALSE;
  double v[32];
  int k = 0;
  for (const auto& d : s.deck) {
    v[k++] = d.loaded; v[k++] = d.playing; v[k++] = d.ended; v[k++] = d.trackId; v[k++] = d.posSec; v[k++] = d.durationSec;
    v[k++] = d.rate; v[k++] = d.bpm; v[k++] = d.beatPhase; v[k++] = d.peakL; v[k++] = d.peakR;
  }
  const auto& p = s.plan;
  v[k++] = p.active; v[k++] = p.finished; v[k++] = p.item; v[k++] = p.transition; v[k++] = p.waiting;
  v[k++] = p.timeSec; v[k++] = p.totalSec; v[k++] = p.beat; v[k++] = p.bpm;
  v[k++] = s.padStep;
  env->SetDoubleArrayRegion(out, 0, 32, v);
  return JNI_TRUE;
}

/// Starts an offline export of a plan (its own copy: create it with nativePlanCreate, bind items as nativeExportStep reports them).
JFN(jstring, nativeExportBegin)(JNIEnv* env, jclass, jlong h, jint planId, jstring path, jint bits) {
  auto plan = findPlan(h, planId);
  if (!plan) return toJString(env, dumpJson({{"ok", false}, {"error", "unknown plan"}}));
  auto ex = std::make_unique<autodj::MixExporter>(plan, H(h)->engine->sampleRate(), bits);
  if (!ex->open(fromJString(env, path))) return toJString(env, dumpJson({{"ok", false}, {"error", ex->error()}}));
  EngineHandle* p = H(h);
  std::lock_guard<std::mutex> lk(p->djMutex);
  const int id = p->nextExport++;
  p->exports[id] = std::move(ex);
  return toJString(env, dumpJson({{"ok", true}, {"exportId", id}, {"frames", p->exports[id]->framesExpected()}}));
}

/// Renders up to `frames` more frames. out: [written, expected, lastStartedItem, waitingItem, peak]. False when finished or failed.
JFN(jboolean, nativeExportStep)(JNIEnv* env, jclass, jlong h, jint exportId, jint frames, jdoubleArray out) {
  EngineHandle* p = H(h);
  autodj::MixExporter* ex = nullptr;
  {
    std::lock_guard<std::mutex> lk(p->djMutex);
    auto it = p->exports.find(exportId);
    if (it != p->exports.end()) ex = it->second.get();
  }
  if (ex == nullptr) return JNI_FALSE;
  const bool more = ex->step(frames);  // an export is driven by one thread; the map entry stays until nativeExportEnd
  const double v[5] = {double(ex->framesWritten()), double(ex->framesExpected()), double(ex->lastStartedItem()), double(ex->waitingItem()), double(ex->peak())};
  if (env->GetArrayLength(out) >= 5) env->SetDoubleArrayRegion(out, 0, 5, v);
  return more ? JNI_TRUE : JNI_FALSE;
}

JFN(jstring, nativeExportEnd)(JNIEnv* env, jclass, jlong h, jint exportId, jboolean discard) {
  EngineHandle* p = H(h);
  std::unique_ptr<autodj::MixExporter> ex;
  {
    std::lock_guard<std::mutex> lk(p->djMutex);
    auto it = p->exports.find(exportId);
    if (it == p->exports.end()) return toJString(env, dumpJson({{"ok", false}, {"error", "unknown export"}}));
    ex = std::move(it->second);
    p->exports.erase(it);
  }
  const std::string err = ex->error();
  const bool closed = ex->close(discard == JNI_TRUE);
  return toJString(env, dumpJson({{"ok", closed && err.empty()}, {"error", err}, {"frames", ex->framesWritten()}, {"peak", ex->peak()}}));
}

}  // extern "C"
