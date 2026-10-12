#include "BenchCore.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#include <nlohmann/json.hpp>

#include "autodj/core/Clock.h"
#include "autodj/core/DemoSources.h"
#include "autodj/engine/Engine.h"

namespace autodj::bench {
namespace {

void setKey(Engine& e, const std::string& key, float v) {
  const ParamId id = e.registry().find(key);
  if (id != kInvalidParam) e.params().set(id, v);
}

double percentile(std::vector<double>& sorted, double p) {
  if (sorted.empty()) return 0.0;
  const double idx = p * static_cast<double>(sorted.size() - 1);
  const std::size_t lo = static_cast<std::size_t>(idx);
  const std::size_t hi = std::min(lo + 1, sorted.size() - 1);
  const double f = idx - static_cast<double>(lo);
  return sorted[lo] * (1.0 - f) + sorted[hi] * f;
}

Result runOnce(const Config& cfg, const std::shared_ptr<const SampleBuffer>& src, int grains, int cb, bool hermite,
               bool simd) {
  EngineConfig ec;
  ec.sampleRate = cfg.sampleRate;
  ec.realtime = false;  // no governor: the benchmark must measure the requested workload, not a throttled one
  ec.granularVoices = 4;
  Engine e(ec);
  e.setSource(0, src);
  e.granularNode().setUseSimd(simd);
  setKey(e, "transport.bpm", 120.f);
  setKey(e, "master.gain", 0.5f);
  setKey(e, "granular.chaos", cfg.chaos);
  // Four identical-load voices: N/4 grains each. density * size = overlap, size 250 ms.
  const float perVoiceDensity = static_cast<float>(grains) / 4.f / 0.25f;
  for (int v = 0; v < 4; ++v) {
    const std::string p = "granular.v" + std::to_string(v) + ".";
    setKey(e, p + "gate", 1.f);
    setKey(e, p + "source", 0.f);
    setKey(e, p + "position", 0.1f + 0.25f * static_cast<float>(v));
    setKey(e, p + "size_ms", 250.f);
    setKey(e, p + "density", perVoiceDensity);
    setKey(e, p + "spray_ms", 1500.f);
    setKey(e, p + "jitter", 0.2f);
    setKey(e, p + "pitch_rand_st", 3.f);
    setKey(e, p + "spread", 0.8f);
    setKey(e, p + "env_shape", static_cast<float>(v));
    setKey(e, p + "quality", hermite ? 1.f : 0.f);
  }

  std::vector<float> buf(static_cast<std::size_t>(cb) * 2);
  const int warm = std::max(1, static_cast<int>(cfg.warmupSeconds * cfg.sampleRate / cb));
  const int n = std::max(10, static_cast<int>(cfg.seconds * cfg.sampleRate / cb));
  for (int i = 0; i < warm; ++i) e.process(buf.data(), cb);

  std::vector<double> us(static_cast<std::size_t>(n));
  const std::uint64_t g0 = e.granularNode().stats().grainSamples;
  double activeSum = 0.0, totalUs = 0.0;
  for (int i = 0; i < n; ++i) {
    const std::uint64_t t0 = monotonicNowNs();
    e.process(buf.data(), cb);
    const std::uint64_t t1 = monotonicNowNs();
    us[static_cast<std::size_t>(i)] = static_cast<double>(t1 - t0) / 1000.0;
    totalUs += us[static_cast<std::size_t>(i)];
    activeSum += e.granularNode().stats().activeGrains;
  }
  const std::uint64_t grainFrames = e.granularNode().stats().grainSamples - g0;

  Result r;
  r.grainsTarget = grains;
  r.callbackFrames = cb;
  r.hermite = hermite;
  r.simd = simd;
  r.callbacks = n;
  r.avgActiveGrains = activeSum / n;
  r.budgetUs = 1e6 * static_cast<double>(cb) / cfg.sampleRate;
  r.meanUs = totalUs / n;
  std::sort(us.begin(), us.end());
  r.p50Us = percentile(us, 0.50);
  r.p99Us = percentile(us, 0.99);
  r.p999Us = percentile(us, 0.999);
  r.maxUs = us.back();
  r.loadMean = r.meanUs / r.budgetUs;
  r.loadP99 = r.p99Us / r.budgetUs;
  r.nsPerGrainFrame = grainFrames ? totalUs * 1000.0 / static_cast<double>(grainFrames) : 0.0;
  return r;
}

std::string cpuName() {
#if defined(__APPLE__)
  char buf[256];
  std::size_t len = sizeof buf;
  if (sysctlbyname("machdep.cpu.brand_string", buf, &len, nullptr, 0) == 0 && len > 1) return std::string(buf);
  return "Apple (unknown model)";
#endif
  std::ifstream f("/proc/cpuinfo");
  std::string line, best;
  while (std::getline(f, line)) {
    for (const char* key : {"model name", "Hardware", "Processor"}) {
      if (line.rfind(key, 0) == 0) {
        const auto c = line.find(':');
        if (c != std::string::npos && best.empty()) best = line.substr(c + 2);
      }
    }
  }
  return best.empty() ? "unknown" : best;
}

const char* kernelName(bool simd) { return simd ? granular::kernelBackendName() : "scalar"; }

}  // namespace

Environment environment() {
  Environment env;
  env.cpu = cpuName();
#if defined(__aarch64__)
  env.arch = "aarch64";
#elif defined(__x86_64__)
  env.arch = "x86_64";
#elif defined(__arm__)
  env.arch = "armv7";
#elif defined(__i386__)
  env.arch = "x86";
#else
  env.arch = "unknown";
#endif
#if defined(__clang__)
  env.compiler = "clang " __clang_version__;
#elif defined(__GNUC__)
  env.compiler = std::string("gcc ") + __VERSION__;
#else
  env.compiler = "unknown";
#endif
#ifdef NDEBUG
  env.buildType = "optimised (NDEBUG)";
#else
  env.buildType = "DEBUG - numbers are not representative";
#endif
  env.kernelBackend = granular::kernelBackendName();
  env.hardwareThreads = static_cast<int>(std::thread::hardware_concurrency());
  return env;
}

std::vector<Result> run(const Config& cfg, void (*progress)(void*, int, int), void* ctx) {
  const auto src = demo::chordPad(20.0, cfg.sampleRate);
  const bool haveSimd = std::strcmp(granular::kernelBackendName(), "scalar") != 0;

  struct Case { int grains, cb; bool hermite, simd; };
  std::vector<Case> cases;
  for (int cb : cfg.callbackFrames)
    for (int g : cfg.grains)
      for (int h = 0; h < 2; ++h) {
        if (h == 0 && !cfg.linear) continue;
        if (h == 1 && !cfg.hermite) continue;
        if (cfg.simd && haveSimd) cases.push_back({g, cb, h == 1, true});
        if (cfg.scalar || !haveSimd) cases.push_back({g, cb, h == 1, false});
      }

  std::vector<Result> out;
  int done = 0;
  for (const Case& c : cases) {
    std::vector<Result> reps;
    for (int i = 0; i < std::max(1, cfg.repeats); ++i) reps.push_back(runOnce(cfg, src, c.grains, c.cb, c.hermite, c.simd));
    std::sort(reps.begin(), reps.end(), [](const Result& a, const Result& b) { return a.meanUs < b.meanUs; });
    out.push_back(reps[reps.size() / 2]);
    if (progress) progress(ctx, ++done, static_cast<int>(cases.size()));
  }
  return out;
}

std::string toJson(const Environment& env, const Config& cfg, const std::vector<Result>& results) {
  nlohmann::json j;
  j["environment"] = {{"cpu", env.cpu},         {"arch", env.arch},   {"compiler", env.compiler},
                      {"build", env.buildType}, {"simd_backend", env.kernelBackend}, {"hardware_threads", env.hardwareThreads}};
  j["config"] = {{"seconds_per_run", cfg.seconds}, {"warmup_seconds", cfg.warmupSeconds}, {"repeats", cfg.repeats},
                 {"sample_rate", cfg.sampleRate}, {"chaos", cfg.chaos}};
  nlohmann::json rows = nlohmann::json::array();
  for (const Result& r : results) {
    rows.push_back({{"grains_target", r.grainsTarget},
                    {"grains_active_avg", r.avgActiveGrains},
                    {"callback_frames", r.callbackFrames},
                    {"interpolation", r.hermite ? "hermite" : "linear"},
                    {"kernel", kernelName(r.simd)},
                    {"budget_us", r.budgetUs},
                    {"mean_us", r.meanUs},
                    {"p50_us", r.p50Us},
                    {"p99_us", r.p99Us},
                    {"p999_us", r.p999Us},
                    {"max_us", r.maxUs},
                    {"load_mean", r.loadMean},
                    {"load_p99", r.loadP99},
                    {"ns_per_grain_frame", r.nsPerGrainFrame},
                    {"callbacks", r.callbacks}});
  }
  j["results"] = rows;
  return j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
}

std::string toMarkdown(const Environment& env, const Config& cfg, const std::vector<Result>& results) {
  std::string s;
  char line[512];
  std::snprintf(line, sizeof line,
                "CPU: %s (%s, %d threads)  \nCompiler: %s  \nBuild: %s  \nSIMD backend: %s  \n"
                "%.0f s measured per run (+%.1f s warm-up), median of %d runs, 4 voices, 48 kHz stereo float, chaos %.2f%s\n\n",
                env.cpu.c_str(), env.arch.c_str(), env.hardwareThreads, env.compiler.c_str(), env.buildType.c_str(),
                env.kernelBackend.c_str(), cfg.seconds, cfg.warmupSeconds, cfg.repeats, static_cast<double>(cfg.chaos),
                cfg.chaos > 0.f ? " (reverb/flanger macro running)" : " (bypass)");
  s += line;
  s += "| Grains (target / active) | Callback | Interp. | Kernel | mean µs | p99 µs | max µs | load mean | load p99 | ns / grain-frame |\n";
  s += "|---:|---:|---|---|---:|---:|---:|---:|---:|---:|\n";
  for (const Result& r : results) {
    std::snprintf(line, sizeof line, "| %d / %.0f | %d | %s | %s | %.1f | %.1f | %.1f | %.1f %% | %.1f %% | %.2f |\n",
                  r.grainsTarget, r.avgActiveGrains, r.callbackFrames, r.hermite ? "Hermite" : "linear",
                  kernelName(r.simd), r.meanUs, r.p99Us, r.maxUs, 100.0 * r.loadMean, 100.0 * r.loadP99,
                  r.nsPerGrainFrame);
    s += line;
  }
  // SIMD speed-up: scalar mean / simd mean for identical settings.
  bool header = false;
  for (const Result& a : results) {
    if (!a.simd) continue;
    for (const Result& b : results) {
      if (b.simd || b.grainsTarget != a.grainsTarget || b.callbackFrames != a.callbackFrames || b.hermite != a.hermite) continue;
      if (!header) {
        s += "\nSIMD speed-up (scalar mean / SIMD mean):\n\n| Grains | Callback | Interp. | speed-up |\n|---:|---:|---|---:|\n";
        header = true;
      }
      std::snprintf(line, sizeof line, "| %d | %d | %s | %.2fx |\n", a.grainsTarget, a.callbackFrames,
                    a.hermite ? "Hermite" : "linear", b.meanUs / a.meanUs);
      s += line;
    }
  }
  return s;
}

}  // namespace autodj::bench
