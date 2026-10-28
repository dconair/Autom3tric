// Long-running stress test: one audio thread calling Engine::process(), one control thread doing everything
// a UI could do (parameter storms, notes, source swaps, presets, graph swaps, transport, resets).
//
//   stress_test [--audio-seconds 600] [--paced-seconds 60] [--seed 1] [--quiet]
//
// Phase 1 "free"  : the audio thread renders as fast as it can with random callback sizes 1..1500.
// Phase 2 "paced" : the audio thread is scheduled like a real callback (absolute deadlines, 192 frames @ 48 kHz);
//                   a callback that takes longer than its buffer duration counts as a *virtual xrun*.
// Checks: output finite and <= 1, no audio-thread allocation/lock violation (RT guard), pool invariants,
// no growth of resident memory after warm-up, every retired buffer/graph returns to the control side.
#include <unistd.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#endif

#include <algorithm>
#include <fenv.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "autodj/core/Clock.h"
#include "autodj/core/DemoSources.h"
#include "autodj/core/RtGuard.h"
#include "autodj/engine/Engine.h"

using namespace autodj;

namespace {

long rssKb() {
#if defined(__APPLE__)
  mach_task_basic_info_data_t info;
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) return -1;
  return static_cast<long>(info.resident_size / 1024);
#else
  std::FILE* f = std::fopen("/proc/self/statm", "r");
  if (!f) return -1;
  long pages = 0, resident = 0;
  if (std::fscanf(f, "%ld %ld", &pages, &resident) != 2) resident = -1;
  std::fclose(f);
  return resident < 0 ? -1 : resident * (sysconf(_SC_PAGESIZE) / 1024);
#endif
}

struct ControlStats {
  std::atomic<std::uint64_t> paramSets{0}, notes{0}, sources{0}, presets{0}, graphs{0}, transport{0}, resets{0}, queueFull{0};
};

// One random UI action. Used by the control thread (phases free/paced) and, single-threaded and fully
// deterministic, by the fuzz phase between callbacks.
void controlStep(Engine& e, XorShift32& rng, ControlStats& st, std::size_t nParams, int iter) {
  const std::uint32_t r = rng.nextU32() % 100;
  if (r < 60) {
    // parameter storm, biased to the extremes where things break
    const ParamId id = static_cast<ParamId>(rng.nextU32() % nParams);
    const float u = rng.uniform();
    e.params().setNormalized(id, u < 0.15f ? 0.f : (u > 0.85f ? 1.f : rng.uniform()));
    ++st.paramSets;
  } else if (r < 75) {
    const int v = static_cast<int>(rng.nextU32() % 4);
    if (!(rng.uniform() < 0.7f ? e.noteOn(v, rng.bipolar() * 24.f, rng.uniform()) : e.noteOff(v))) ++st.queueFull;
    ++st.notes;
  } else if (r < 83) {
    const double len = rng.uniform() < 0.2f ? 0.02 : 0.1 + 6.0 * rng.uniform();  // 20 ms .. 6 s
    const int sr = 48000;
    auto buf = rng.uniform() < 0.5f ? demo::sine(60 + 800.0 * rng.uniform(), len, sr) : demo::noise(len, sr, 0.5f);
    if (!e.setSource(static_cast<int>(rng.nextU32() % 20), std::move(buf))) ++st.queueFull;  // slots >= 16 are rejected on purpose
    ++st.sources;
  } else if (r < 88) {
    const std::string p = e.exportPreset("stress");
    (void)e.importPreset(p);
    (void)e.importPreset("{ not json");  // must be rejected without touching anything
    ++st.presets;
  } else if (r < 91) {
    if (!e.publishGraph(e.makeGraph(rng.uniform(), rng.uniform()))) ++st.queueFull;
    ++st.graphs;
  } else if (r < 97) {
    const std::uint32_t t = rng.nextU32() % 3;
    if (t == 0) e.play(); else if (t == 1) e.stop(); else e.seekBeats(rng.uniform() * 128.0);
    ++st.transport;
  } else if (r < 98 && iter % 8 == 0) {
    e.resetEngine();
    ++st.resets;
  }
  if (iter % 16 == 0) {
    e.collectGarbage();
    EngineStatus s;
    GrainViz z;
    (void)e.readStatus(s);
    (void)e.readGrainViz(z);
  }
}

void controlLoop(Engine& e, std::atomic<bool>& stop, ControlStats& st, std::uint32_t seed) {
  XorShift32 rng(seed);
  const std::size_t nParams = e.registry().size();
  int iter = 0;
  while (!stop.load(std::memory_order_relaxed)) {
    controlStep(e, rng, st, nParams, ++iter);
    if (iter % 4 == 0) std::this_thread::sleep_for(std::chrono::microseconds(200));
  }
}

struct AudioStats {
  std::uint64_t callbacks = 0, frames = 0, nonFinite = 0, overOne = 0, virtualXruns = 0;
  double maxCallbackUs = 0, sumCallbackUs = 0, maxLateUs = 0;
  std::vector<double> samplesUs;
};

// Returns false (and describes the first offender) if the block contains NaN/Inf or exceeds full scale.
bool checkBlock(const float* x, int n, AudioStats& a, std::string* what = nullptr) {
  for (int i = 0; i < n * 2; ++i) {
    if (!std::isfinite(x[i])) {
      ++a.nonFinite;
      if (what) *what = "non-finite sample " + std::to_string(x[i]) + " at frame " + std::to_string(i / 2) + " of " + std::to_string(n);
      return false;
    }
    if (std::fabs(x[i]) > 1.0f + 1e-6f) {
      ++a.overOne;
      if (what) *what = "sample " + std::to_string(x[i]) + " above full scale at frame " + std::to_string(i / 2);
      return false;
    }
  }
  return true;
}

}  // namespace

// Phase 0: deterministic single-threaded fuzz. Control actions run between callbacks on the audio thread's own
// thread, so a failure reproduces from its seed alone. Returns false and prints the seed / iteration on failure.
bool fuzzSeed(std::uint32_t seed, int callbacks, bool verbose) {
  EngineConfig cfg;
  cfg.realtime = false;
  Engine e(cfg);
  for (int s = 0; s < 4; ++s) e.setSource(s, s % 2 ? demo::noise(2.0, 48000, 0.4f) : demo::chordPad(5.0, 48000));
  for (int v = 0; v < 4; ++v) {
    e.noteOn(v, 0.f, 1.f);
    e.params().set(e.registry().find("granular.v" + std::to_string(v) + ".density"), 400.f);
  }
  XorShift32 rng(seed);
  ControlStats cs;
  AudioStats as;
  std::vector<float> buf(2 * 2048);
  const std::size_t nParams = e.registry().size();
  for (int cb = 0; cb < callbacks; ++cb) {
    const int steps = static_cast<int>(rng.nextU32() % 4);
    // iter = cb * 4 + k so that the periodic actions in controlStep (garbage collection every 16th, engine reset every
    // 8th iteration) actually fire; with `+ 1` they never did and retired buffers piled up (a test bug, not an engine one).
    for (int k = 0; k < steps; ++k) controlStep(e, rng, cs, nParams, cb * 4 + k);
    const std::uint32_t r = rng.nextU32() % 10;
    const int n = r < 4 ? 192 : (r < 7 ? static_cast<int>(1 + rng.nextU32() % 1500) : (r < 9 ? 64 : 480));
    e.process(buf.data(), n);
    std::string what;
    if (!checkBlock(buf.data(), n, as, &what)) {
      const std::string path = "fuzz_failure_seed" + std::to_string(seed) + ".json";
      std::ofstream(path) << e.exportPreset("fuzz failure");
      std::printf("FUZZ FAILURE seed %u, callback %d (%d frames): %s\n  parameter state saved to %s\n", seed, cb, n, what.c_str(), path.c_str());
      return false;
    }
  }
  if (!e.granularNode().pool().checkInvariants()) {
    std::printf("FUZZ FAILURE seed %u: grain pool invariants broken\n", seed);
    return false;
  }
  // Ownership: after one more callback + collection, everything the audio thread retired must be back on the control side.
  e.process(buf.data(), 192);
  e.collectGarbage();
  if (e.heldBufferCount() > 16 + 2 || e.heldGraphCount() > 2) {
    std::printf("FUZZ FAILURE seed %u: %zu buffers / %zu graphs still held after collection (limit 18 / 2)\n", seed,
                e.heldBufferCount(), e.heldGraphCount());
    return false;
  }
  if (verbose) std::printf("  seed %u ok (%d callbacks)\n", seed, callbacks);
  return true;
}

int main(int argc, char** argv) {
  double audioSeconds = 600.0, pacedSeconds = 60.0, freeMinWall = 30.0;
  int fuzzSeeds = 200, fuzzCallbacks = 4000;
  std::uint32_t seed = 1;
  bool quiet = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--audio-seconds") audioSeconds = std::atof(next().c_str());
    else if (a == "--paced-seconds") pacedSeconds = std::atof(next().c_str());
    else if (a == "--free-min-wall-seconds") freeMinWall = std::atof(next().c_str());
    else if (a == "--fuzz-seeds") fuzzSeeds = std::atoi(next().c_str());
    else if (a == "--fuzz-callbacks") fuzzCallbacks = std::atoi(next().c_str());
    else if (a == "--seed") seed = static_cast<std::uint32_t>(std::atoi(next().c_str()));
    else if (a == "--quiet") quiet = true;
    else if (a == "--trap-fpe") {  // debugging aid: SIGFPE at the first bad operation (glibc only)
#if defined(__GLIBC__)
      feenableexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW);
#else
      std::fprintf(stderr, "--trap-fpe is only available with glibc\n");
#endif
    }
  }

  bool failed = false;
  auto fail = [&](const char* msg) { std::printf("FAIL: %s\n", msg); failed = true; };

  {
    int bad = 0;
    for (int i = 0; i < fuzzSeeds; ++i) {
      if (!fuzzSeed(seed + static_cast<std::uint32_t>(i), fuzzCallbacks, !quiet && i < 3)) ++bad;
    }
    std::printf("== fuzz phase: %d seeds x %d callbacks (deterministic, single thread): %d failing ==\n", fuzzSeeds, fuzzCallbacks, bad);
    if (bad) fail("fuzz phase found non-finite output or broken invariants");
  }

  for (int phase = 0; phase < 2; ++phase) {
    const bool paced = phase == 1;
    const double seconds = paced ? pacedSeconds : audioSeconds;
    if (seconds <= 0) continue;

    EngineConfig cfg;
    cfg.realtime = true;  // governor + load metering active, like on the device
    Engine e(cfg);
    for (int s = 0; s < 4; ++s) {
      e.setSource(s, s % 2 ? demo::noise(2.0, 48000, 0.4f) : demo::chordPad(5.0, 48000));
    }
    for (int v = 0; v < 4; ++v) {
      e.noteOn(v, 0.f, 1.f);
      const std::string p = "granular.v" + std::to_string(v) + ".";
      e.params().set(e.registry().find(p + "density"), 400.f);
      e.params().set(e.registry().find(p + "size_ms"), 300.f);
    }
    e.play();

    std::atomic<bool> stop{false};
    ControlStats cs;
    std::thread control(controlLoop, std::ref(e), std::ref(stop), std::ref(cs), seed + 17u * static_cast<unsigned>(phase));

    AudioStats as;
    std::vector<float> buf(2 * 2048);
    XorShift32 rng(seed ^ 0xBEEFu);
    const std::uint64_t violationsBefore = rt::violationCount();
    long rssWarm = -1, rssPeak = 0;
    const int budgetFrames = 192;
    std::uint64_t framesTarget = static_cast<std::uint64_t>(seconds * cfg.sampleRate);
    const std::uint64_t start = monotonicNowNs();
    std::uint64_t deadline = start;
    std::uint64_t nextReport = 0;

    while (as.frames < framesTarget || (!paced && static_cast<double>(monotonicNowNs() - start) / 1e9 < freeMinWall)) {
      int n = budgetFrames;
      if (!paced) {
        const std::uint32_t r = rng.nextU32() % 10;
        n = r < 5 ? 192 : (r < 8 ? static_cast<int>(1 + rng.nextU32() % 1500) : (r < 9 ? 64 : 480));
      }
      std::uint64_t late = 0;
      if (paced) {
        deadline += static_cast<std::uint64_t>(1e9 * n / cfg.sampleRate);
#if defined(__linux__)
        timespec ts{static_cast<time_t>(deadline / 1000000000ull), static_cast<long>(deadline % 1000000000ull)};
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
#else  // no absolute sleep on macOS: sleep for the remaining time on our own monotonic clock
        const std::uint64_t nowNs = monotonicNowNs();
        if (deadline > nowNs) std::this_thread::sleep_for(std::chrono::nanoseconds(deadline - nowNs));
#endif
        late = monotonicNowNs() - deadline;
      }
      const std::uint64_t t0 = monotonicNowNs();
      e.process(buf.data(), n);
      const std::uint64_t t1 = monotonicNowNs();
      const double us = static_cast<double>(t1 - t0) / 1000.0;
      ++as.callbacks;
      as.frames += static_cast<std::uint64_t>(n);
      as.sumCallbackUs += us;
      as.maxCallbackUs = std::fmax(as.maxCallbackUs, us);
      as.maxLateUs = std::fmax(as.maxLateUs, static_cast<double>(late) / 1000.0);
      if (us > 1e6 * n / cfg.sampleRate) ++as.virtualXruns;
      if (as.callbacks % 16 == 0) as.samplesUs.push_back(us);
      {
        std::string what;
        if (!checkBlock(buf.data(), n, as, &what) && as.nonFinite + as.overOne == 1) {
          std::printf("  first bad block: callback %llu (%d frames): %s\n", static_cast<unsigned long long>(as.callbacks), n, what.c_str());
          std::ofstream("stress_failure_state.json") << e.exportPreset("stress failure");
        }
      }

      const std::uint64_t elapsedSec = (t1 - start) / 1000000000ull;
      if (elapsedSec >= nextReport) {
        const long rss = rssKb();
        if (elapsedSec >= 5 && rssWarm < 0) rssWarm = rss;  // first 5 s = warm-up (allocator arenas, thread stacks)
        rssPeak = std::max(rssPeak, rss);
        if (!quiet) std::printf("  [%s] %5.0f s audio, rss %ld kB, active grains %d\n", paced ? "paced" : "free ",
                                static_cast<double>(as.frames) / cfg.sampleRate, rss, e.granularNode().stats().activeGrains);
        nextReport = elapsedSec + (paced ? 10 : 20);
      }
    }
    stop = true;
    control.join();

    // Drain: everything the audio thread still holds must come back.
    e.resetEngine();
    for (int i = 0; i < 20; ++i) { e.process(buf.data(), 192); e.collectGarbage(); }
    std::vector<float> keep(2 * 192);
    for (int i = 0; i < 40; ++i) { e.process(keep.data(), 192); e.collectGarbage(); }

    const double wall = static_cast<double>(monotonicNowNs() - start) / 1e9;
    std::sort(as.samplesUs.begin(), as.samplesUs.end());
    const double p99 = as.samplesUs.empty() ? 0.0 : as.samplesUs[static_cast<std::size_t>(0.99 * (as.samplesUs.size() - 1))];
    const long rssEnd = rssKb();
    const std::uint64_t violations = rt::violationCount() - violationsBefore;

    std::printf("\n== %s phase: %.0f s of audio in %.1f s wall (%.0fx), %llu callbacks ==\n", paced ? "paced" : "free", seconds,
                wall, seconds / wall, static_cast<unsigned long long>(as.callbacks));
    std::printf("  control thread : %llu param writes, %llu notes, %llu source swaps (%llu rejected/queue full), %llu preset round trips, "
                "%llu graph swaps, %llu transport, %llu resets\n",
                static_cast<unsigned long long>(cs.paramSets.load()), static_cast<unsigned long long>(cs.notes.load()),
                static_cast<unsigned long long>(cs.sources.load()), static_cast<unsigned long long>(cs.queueFull.load()),
                static_cast<unsigned long long>(cs.presets.load()), static_cast<unsigned long long>(cs.graphs.load()),
                static_cast<unsigned long long>(cs.transport.load()), static_cast<unsigned long long>(cs.resets.load()));
    std::printf("  callback time  : mean %.1f us, p99 (sampled) %.1f us, max %.1f us; virtual xruns %llu (callback > its buffer duration)\n",
                as.sumCallbackUs / static_cast<double>(as.callbacks), p99, as.maxCallbackUs,
                static_cast<unsigned long long>(as.virtualXruns));
    if (paced) std::printf("  scheduler wake-up lateness (OS, not engine): max %.0f us\n", as.maxLateUs);
    std::printf("  memory         : rss after warm-up %ld kB, peak %ld kB, end %ld kB\n", rssWarm, rssPeak, rssEnd);
    std::printf("  ownership      : %zu buffers / %zu graphs still held (limit 16 / 2); active grains at end %d\n",
                e.heldBufferCount(), e.heldGraphCount(), e.granularNode().pool().activeCount());
    std::printf("  rt violations  : %llu%s\n", static_cast<unsigned long long>(violations),
                AUTODJ_RT_GUARD ? "" : " (RT guard not compiled in)");

    if (as.nonFinite) fail("non-finite samples");
    if (as.overOne) fail("samples above 1.0 after the master soft clip");
    if (violations) fail("audio-thread allocation / forbidden call");
    if (!e.granularNode().pool().checkInvariants()) fail("grain pool invariants broken");
    if (rssWarm > 0 && rssEnd > rssWarm + 24 * 1024) fail("resident memory grew by more than 24 MB after warm-up");
    if (e.heldBufferCount() > 16 + 2) fail("retired sample buffers were not returned to the control side");
    if (e.heldGraphCount() > 2) fail("retired graphs were not returned to the control side");
  }
  std::printf("\n%s\n", failed ? "STRESS TEST FAILED" : "STRESS TEST PASSED");
  return failed ? 1 : 0;
}
