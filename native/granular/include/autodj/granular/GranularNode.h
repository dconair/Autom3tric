#pragma once
// The granular engine: 1-4 voices sharing one preallocated grain pool.
//
// Per block (control rate, everything below runs on the audio thread, nothing allocates):
//   setParams : read atomics, per-block one-pole smoothing, (hook for the modulation matrix, M8)
//   process   : 1. per voice: gate envelope, scan head, trigger grains (phase accumulator or beat grid)
//               2. render all active grains into per-voice stereo buses
//               3. mix the buses with a per-block gain ramp into the output
//               4. publish a throttled visualisation snapshot
//
// Voices are independent "granular instruments": own source slot, own parameter group, own PRNG.
// Stealing (retrigger, voice limit lowered by the adaptive governor) fades the grains of that voice
// out over granular.steal_fade_ms instead of cutting them.

#include <cstdint>
#include <memory>

#include "autodj/core/Dsp.h"
#include "autodj/core/Node.h"
#include "autodj/core/SourceTable.h"
#include "autodj/core/Status.h"
#include "autodj/core/TripleBuffer.h"
#include "autodj/fx/ChaosFx.h"
#include "autodj/granular/AdaptiveQuality.h"
#include "autodj/granular/EnvelopeTables.h"
#include "autodj/granular/GrainKernels.h"
#include "autodj/granular/GrainPool.h"
#include "autodj/granular/GrainScheduler.h"
#include "autodj/granular/GranularParams.h"

namespace autodj::granular {

struct GranularStats {
  int activeGrains = 0;
  int activeVoices = 0;
  int qualityLevel = 0;
  std::uint32_t droppedGrains = 0;
  std::uint64_t spawnedGrains = 0;
  std::uint64_t grainSamples = 0;  // cumulative number of grain-frames rendered (benchmark / profiling)
};

class GranularNode final : public Node {
 public:
  /// Called for every spawned grain (tests/diagnostics only; null in production).
  using SpawnObserver = void (*)(void* ctx, int voice, std::int64_t engineSample, double srcPos, int length,
                                 float rate, bool reversed);

  explicit GranularNode(const GranularParamIds& ids, std::uint32_t seed = 0xA5F00D01u);
  ~GranularNode() override;

  // ---- Node ------------------------------------------------------------------------------
  void prepare(const ProcessSpec& spec) override;
  void reset() noexcept override;
  void setParams(const ParamStore& params, const BlockTime& time) noexcept override;
  void process(ProcessIO& io) noexcept override;
  bool handleCommand(const Command& cmd, GarbageQueue& garbage) noexcept override;

  // ---- engine hooks (audio thread) -----------------------------------------------------
  /// Feed the callback load (busy time / buffer duration) once per audio callback.
  void updateLoad(float load, float dtSeconds) noexcept { quality_.update(load, dtSeconds); }
  void setAdaptiveConfig(const AdaptiveQuality::Config& c) noexcept { quality_ = AdaptiveQuality(c); }
  void attachViz(TripleBuffer<GrainViz>* sink) noexcept { viz_ = sink; }

  // ---- diagnostics -----------------------------------------------------------------------
  GranularStats stats() const noexcept;
  void setSpawnObserver(SpawnObserver fn, void* ctx) noexcept {
    observer_ = fn;
    observerCtx_ = ctx;
  }
  /// false = use the scalar reference kernels (tests / A-B benchmarks).
  void setUseSimd(bool simd) noexcept { useSimd_ = simd; }
  const GrainPool& pool() const noexcept { return *pool_; }
  const AdaptiveQuality& adaptive() const noexcept { return quality_; }
  int numVoices() const noexcept { return ids_.numVoices; }

 private:
  enum Smoothed { kSize, kDensity, kSpray, kPitch, kVolume, kSpread, kNumSmoothed };

  struct VoiceParams {
    bool gate = false;
    int source = 0;
    float position = 0.5f;
    float sizeMs = 80.f, density = 20.f, sprayMs = 0.f, jitter = 0.f;
    float pitchSt = 0.f, pitchRandSt = 0.f;
    int envShape = 0;
    float reverseProb = 0.f, spread = 0.f, ampRand = 0.f;
    bool scanOn = false;
    float scanSpeed = 1.f, loopStart = 0.f, loopEnd = 1.f;
    bool freeze = false;
    int syncMode = 0, syncDiv = 2, quality = 0;
    float volume = 0.8f, releaseMs = 120.f;
    bool autoGain = true;
    int cellMode = 0, cellCount = 0, cellIndex = 0, stepDiv = 3;
  };

  struct Voice {
    XorShift32 rng;
    GrainScheduler sched;
    VoiceParams p;
    BlockSmoother sm[kNumSmoothed];
    float coef[kNumSmoothed] = {};
    bool noteGate = false;
    float notePitch = 0.f, noteVel = 1.f;
    bool gatePrev = false, enabledPrev = true;
    double head = 0.5;      // scan head, normalised
    float lastPos = -1.f;
    float effPos = 0.5f;    // normalised position new grains start from
    float level = 0.f;      // gate envelope 0..1
    float targetGain = 0.f; // mixer gain for this block
    float gainPrev = 0.f;
    int liveGrains = 0;
    int srcEntry = -1;
    bool rendered = false;  // bus holds data this block
    // Cells (Step / Keys). The step index is a pure function of the master clock beat, so it is sample-accurate and
    // independent of the block size; only the origin ("cell 0") is state.
    int cellN = 8;
    int cell = -1;                  // cell read from at the start of this block, -1 in Hold mode
    double stepBeats = 1.0;
    double stepOrigin = 0.0;        // beat of the grid step that counts as step 0
    bool stepResetPending = true;
    int cellModePrev = 0;
  };

  ParamId smoothedId(int voice, int k) const noexcept;
  void controlVoice(int v, int voiceLimit, int nGated, const AdaptiveQuality::Limits& lim, int grainCap,
                    const BlockTime& time) noexcept;
  void spawnGrain(int v, Voice& vo, int offset, const SampleBuffer* buf, int entry, const BlockTime& time,
                  int grainCap) noexcept;
  void renderGrains(int frames, const AdaptiveQuality::Limits& lim, GarbageQueue& garbage) noexcept;
  void mixVoices(float* out, int frames) noexcept;
  static int stepCell(const Voice& vo, double beat) noexcept;
  void publishViz(int frames) noexcept;
  void stealVoiceGrains(int v) noexcept;
  void clearGrains(GarbageQueue& garbage) noexcept;

  GranularParamIds ids_;
  std::uint32_t seed_;
  double sr_ = 48000.0;
  int maxBlock_ = 192;
  int coefFrames_ = -1;

  // global control state (snapshotted in setParams)
  bool adaptiveOn_ = true;
  int userMaxVoices_ = kMaxVoices;
  int userGrainCap_ = GrainPool::kCapacity;
  int stealFadeSamples_ = 192;

  Voice voices_[kMaxVoices];
  std::unique_ptr<GrainPool> pool_;
  std::unique_ptr<EnvelopeTables> env_;
  SourceTable srcTable_;
  AdaptiveQuality quality_;
  GrainKernelFn kernels_[2][2] = {};  // [simd][interp]
  bool useSimd_ = true;

  alignas(kCacheLine) float bus_[kMaxVoices][kMaxBlockFrames * 2];
  alignas(kCacheLine) float scratch_[kMaxBlockFrames * 2];
  alignas(kCacheLine) float mix_[kMaxBlockFrames * 2];  // voices summed, then Chaos, then output gain, then added to io.out

  // instrument section: Chaos macro + output level (block-rate smoothed, ramped across the block)
  fx::ChaosFx chaos_;
  BlockSmoother chaosSm_, outSm_;
  float chaosCoef_ = 1.f, outCoef_ = 1.f;
  float chaosNow_ = 0.f, chaosPrev_ = 0.f;
  float outDbNow_ = 0.f, outGainPrev_ = 1.f;
  bool outPrimed_ = false;

  GarbageQueue* lastGarbage_ = nullptr;
  TripleBuffer<GrainViz>* viz_ = nullptr;
  int vizAccum_ = 0;
  SpawnObserver observer_ = nullptr;
  void* observerCtx_ = nullptr;

  std::uint32_t dropped_ = 0;
  std::uint64_t spawned_ = 0;
  std::uint64_t grainSamples_ = 0;
  int lastActiveVoices_ = 0;
};

}  // namespace autodj::granular
