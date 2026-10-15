#pragma once
// Granular engine micro-benchmark. Shared by the host CLI (native/bench/bench_granular.cpp) and the
// Android app (JNI -> "Benchmark" button), so both report the same numbers computed the same way.
//
// What is measured: wall-clock time of Engine::process() per callback while N grains are active,
// the same call the Oboe callback makes (4 voices, stereo float, 48 kHz, random positions in a 20 s
// source, random pitch, stereo spread). Numbers are only comparable between runs on the same machine.

#include <string>
#include <vector>

namespace autodj::bench {

struct Config {
  std::vector<int> grains{64, 128, 256, 512};
  std::vector<int> callbackFrames{128, 192};
  bool linear = true;
  bool hermite = true;
  bool scalar = true;   // also run the scalar reference kernels for the SIMD speed-up column
  bool simd = true;
  double seconds = 8.0;   // measured audio per run
  double warmupSeconds = 1.5;
  int repeats = 3;        // the run with the median mean is reported
  int sampleRate = 48000;
  float chaos = 0.f;      // granular.chaos for all runs; 0 = bypass (the default workload), > 0 adds the reverb/flanger macro
};

struct Result {
  int grainsTarget = 0;
  int callbackFrames = 0;
  bool hermite = false;
  bool simd = false;
  double avgActiveGrains = 0;
  double budgetUs = 0;
  double meanUs = 0, p50Us = 0, p99Us = 0, p999Us = 0, maxUs = 0;
  double loadMean = 0;        // meanUs / budgetUs
  double loadP99 = 0;         // p99Us / budgetUs
  double nsPerGrainFrame = 0; // wall time / (grain-frames rendered); includes scheduling, mixing and master
  int callbacks = 0;
};

struct Environment {
  std::string cpu, arch, compiler, buildType, kernelBackend;
  int hardwareThreads = 0;
};

Environment environment();

/// progress(done, total) is called between runs (may be null).
std::vector<Result> run(const Config& cfg, void (*progress)(void* ctx, int done, int total) = nullptr,
                        void* ctx = nullptr);

std::string toJson(const Environment& env, const Config& cfg, const std::vector<Result>& results);
std::string toMarkdown(const Environment& env, const Config& cfg, const std::vector<Result>& results);

}  // namespace autodj::bench
