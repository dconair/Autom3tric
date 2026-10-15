#include "autodj/granular/GranularNode.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace autodj::granular {

namespace {
constexpr int kMinGrainSamples = 32;
constexpr float kAttackSeconds = 0.003f;  // gate attack: fast but not a click
constexpr int kVizRateHz = 60;
static_assert(kMaxVoices == kVizMaxVoices, "visualisation snapshot assumes the voice count");
}  // namespace

GranularNode::GranularNode(const GranularParamIds& ids, std::uint32_t seed) : ids_(ids), seed_(seed) {
  std::memset(bus_, 0, sizeof(bus_));
  std::memset(scratch_, 0, sizeof(scratch_));
  std::memset(mix_, 0, sizeof(mix_));
}

GranularNode::~GranularNode() = default;

void GranularNode::prepare(const ProcessSpec& spec) {
  sr_ = spec.sampleRate;
  maxBlock_ = std::min(spec.maxBlockFrames, kMaxBlockFrames);
  pool_ = std::make_unique<GrainPool>();
  env_ = std::make_unique<EnvelopeTables>();
  kernels_[0][0] = selectKernel(Interp::Linear, false);
  kernels_[0][1] = selectKernel(Interp::Hermite, false);
  kernels_[1][0] = selectKernel(Interp::Linear, true);
  kernels_[1][1] = selectKernel(Interp::Hermite, true);
  stealFadeSamples_ = std::max(16, static_cast<int>(0.004 * sr_));
  chaos_.prepare(sr_);
  coefFrames_ = -1;
  reset();
}

void GranularNode::reset() noexcept {
  if (pool_) {
    if (lastGarbage_) {
      clearGrains(*lastGarbage_);
    } else {
      pool_->clear();
    }
  }
  for (int v = 0; v < kMaxVoices; ++v) {
    Voice& vo = voices_[v];
    vo = Voice{};
    vo.rng.reseed(seed_ + static_cast<std::uint32_t>(v) * 0x9E3779B1u);
    vo.sched.reset();
  }
  quality_.reset();
  chaos_.reset();
  chaosSm_ = BlockSmoother{};
  outSm_ = BlockSmoother{};
  chaosNow_ = chaosPrev_ = 0.f;
  outDbNow_ = 0.f;
  outGainPrev_ = 1.f;
  outPrimed_ = false;
  vizAccum_ = 0;
  coefFrames_ = -1;  // smoothing coefficients were wiped with the voices
}

ParamId GranularNode::smoothedId(int v, int k) const noexcept {
  const VoiceParamIds& id = ids_.voice[v];
  switch (k) {
    case kSize: return id.sizeMs;
    case kDensity: return id.density;
    case kSpray: return id.sprayMs;
    case kPitch: return id.pitchSt;
    case kVolume: return id.volume;
    default: return id.spread;
  }
}

void GranularNode::setParams(const ParamStore& st, const BlockTime& t) noexcept {
  sr_ = t.sampleRate;
  const ParamRegistry& reg = st.registry();

  if (t.frames != coefFrames_) {  // smoothing coefficients depend on the block duration
    const float blockSec = static_cast<float>(t.frames / t.sampleRate);
    for (int v = 0; v < ids_.numVoices; ++v) {
      for (int k = 0; k < kNumSmoothed; ++k) {
        voices_[v].coef[k] = onePoleBlockCoef(reg.spec(smoothedId(v, k)).smoothMs, blockSec);
      }
    }
    chaosCoef_ = onePoleBlockCoef(reg.spec(ids_.chaos).smoothMs, blockSec);
    outCoef_ = onePoleBlockCoef(reg.spec(ids_.outputDb).smoothMs, blockSec);
    coefFrames_ = t.frames;
  }

  adaptiveOn_ = st.getBool(ids_.adaptive);
  userMaxVoices_ = std::clamp(st.getInt(ids_.maxVoices), 1, ids_.numVoices);
  userGrainCap_ = std::clamp(st.getInt(ids_.grainCap), 16, GrainPool::kCapacity);
  stealFadeSamples_ = std::max(16, static_cast<int>(st.get(ids_.stealFadeMs) * 0.001 * sr_));
  chaosNow_ = chaosSm_.next(st.get(ids_.chaos), chaosCoef_);
  outDbNow_ = outSm_.next(st.get(ids_.outputDb), outCoef_);

  for (int v = 0; v < ids_.numVoices; ++v) {
    const VoiceParamIds& id = ids_.voice[v];
    Voice& vo = voices_[v];
    VoiceParams& p = vo.p;
    p.gate = st.getBool(id.gate);
    p.source = st.getInt(id.source);
    p.position = st.get(id.position);
    p.sizeMs = vo.sm[kSize].next(st.get(id.sizeMs), vo.coef[kSize]);
    p.density = vo.sm[kDensity].next(st.get(id.density), vo.coef[kDensity]);
    p.sprayMs = vo.sm[kSpray].next(st.get(id.sprayMs), vo.coef[kSpray]);
    p.pitchSt = vo.sm[kPitch].next(st.get(id.pitchSt), vo.coef[kPitch]);
    p.volume = vo.sm[kVolume].next(st.get(id.volume), vo.coef[kVolume]);
    p.spread = vo.sm[kSpread].next(st.get(id.spread), vo.coef[kSpread]);
    p.jitter = st.get(id.jitter);
    p.pitchRandSt = st.get(id.pitchRandSt);
    p.envShape = st.getInt(id.envShape);
    p.reverseProb = st.get(id.reverseProb);
    p.ampRand = st.get(id.ampRand);
    p.scanOn = st.getBool(id.scanOn);
    p.scanSpeed = st.get(id.scanSpeed);
    p.loopStart = st.get(id.loopStart);
    p.loopEnd = st.get(id.loopEnd);
    p.freeze = st.getBool(id.freeze);
    p.syncMode = st.getInt(id.syncMode);
    p.syncDiv = st.getInt(id.syncDiv);
    p.quality = st.getInt(id.quality);
    p.releaseMs = st.get(id.releaseMs);
    p.autoGain = st.getBool(id.autoGain);
    p.cellMode = st.getInt(id.cellMode);
    p.cellCount = st.getInt(id.cellCount);
    p.cellIndex = st.getInt(id.cellIndex);
    p.stepDiv = st.getInt(id.stepDiv);
    p.seqOn = st.getBool(id.seqOn);
    // M8 hook: control-rate modulation (LFOs, envelope, matrix) is applied to `p` right here.
  }
}

bool GranularNode::handleCommand(const Command& cmd, GarbageQueue& garbage) noexcept {
  lastGarbage_ = &garbage;
  switch (cmd.type) {
    case CmdType::PublishSource:
      srcTable_.publish(cmd.slot, static_cast<const SampleBuffer*>(cmd.payload.ptr), garbage);
      return true;
    case CmdType::NoteOn: {
      if (cmd.target >= ids_.numVoices) return true;
      Voice& vo = voices_[cmd.target];
      stealVoiceGrains(cmd.target);  // retrigger: old grains fade out instead of being cut
      vo.noteGate = true;
      vo.notePitch = cmd.f0;
      vo.noteVel = cmd.f1 > 0.f ? cmd.f1 : 1.f;
      vo.sched.restart();
      return true;
    }
    case CmdType::NoteOff:
      if (cmd.target < ids_.numVoices) {
        voices_[cmd.target].noteGate = false;
        voices_[cmd.target].noteVel = 1.f;
        voices_[cmd.target].notePitch = 0.f;
      }
      return true;
    case CmdType::StepReset:
      if (cmd.target < ids_.numVoices) voices_[cmd.target].stepResetPending = true;
      return true;
    case CmdType::GranularSteps:
      if (cmd.target < ids_.numVoices) {
        voices_[cmd.target].patMask = static_cast<std::uint16_t>(static_cast<std::uint32_t>(cmd.i0) & 0xFFFFu);
        voices_[cmd.target].patCells = cmd.payload.u;
      }
      return true;
    case CmdType::ResetEngine:
      clearGrains(garbage);
      for (int v = 0; v < kMaxVoices; ++v) {
        voices_[v].noteGate = false;
        voices_[v].level = 0.f;
        voices_[v].gainPrev = 0.f;
        voices_[v].sched.reset();
        voices_[v].stepResetPending = true;  // the transport restarts at beat 0
      }
      return false;  // other nodes (and the transport) want to see it too
    default:
      return false;
  }
}

void GranularNode::clearGrains(GarbageQueue& garbage) noexcept {
  GrainPool& gp = *pool_;
  for (int ai = gp.activeCount() - 1; ai >= 0; --ai) {
    const int s = gp.activeSlot(ai);
    srcTable_.release(gp.srcEntry[s], garbage);
    gp.recycle(s);
  }
  for (auto& vo : voices_) vo.liveGrains = 0;
}

void GranularNode::stealVoiceGrains(int v) noexcept {
  GrainPool& gp = *pool_;
  const int n = gp.activeCount();
  for (int ai = 0; ai < n; ++ai) {
    const int s = gp.activeSlot(ai);
    if (gp.voice[s] != v || gp.fadeLeft[s] > 0) continue;
    gp.fadeLeft[s] = stealFadeSamples_;
    gp.fadeGain[s] = 1.f;
    gp.fadeStep[s] = 1.f / static_cast<float>(stealFadeSamples_);
  }
}

void GranularNode::process(ProcessIO& io) noexcept {
  const int frames = io.frames;
  BlockTime time = *io.time;
  // Steps and beat-synced grains sit on the beats of the music while a deck or a mix plays; otherwise on the free engine clock
  if (io.clock != nullptr && io.clock->fromMusic && time.sampleRate > 0.0) {
    time.clockBeat = io.clock->beat;
    time.beatsPerSample = io.clock->bpm / (60.0 * time.sampleRate);
  }
  GarbageQueue& garbage = *io.garbage;
  lastGarbage_ = &garbage;
  srcTable_.flushRetries(garbage);

  const AdaptiveQuality::Limits lim = AdaptiveQuality::limitsFor(adaptiveOn_ ? quality_.level() : 0);
  const int voiceLimit = std::max(1, std::min(ids_.numVoices, userMaxVoices_) - lim.voiceReduction);
  const int grainCap = std::max(8, static_cast<int>(static_cast<float>(userGrainCap_) * lim.grainCapScale));

  int nGated = 0;
  for (int v = 0; v < voiceLimit; ++v) nGated += (voices_[v].p.gate || voices_[v].noteGate) ? 1 : 0;
  nGated = std::max(1, nGated);

  for (int v = 0; v < ids_.numVoices; ++v) controlVoice(v, voiceLimit, nGated, lim, grainCap, time);
  renderGrains(frames, lim, garbage);

  // Voices -> mix_ -> Chaos (in place) -> output gain ramp -> io.out. With chaos = 0 and output = 0 dB this is bit-identical
  // to mixing straight into io.out (the stage is skipped / the gain is exactly 1).
  const std::size_t floats = static_cast<std::size_t>(frames) * 2;
  std::memset(mix_, 0, floats * sizeof(float));
  mixVoices(mix_, frames);
  chaos_.process(mix_, frames, chaosPrev_, chaosNow_);
  chaosPrev_ = chaosNow_;
  const float g1 = dbToLin(outDbNow_);
  const float g0 = outPrimed_ ? outGainPrev_ : g1;
  outPrimed_ = true;
  outGainPrev_ = g1;
  const float dg = (g1 - g0) / static_cast<float>(frames);
  for (int i = 0; i < frames; ++i) {
    const float g = g0 + dg * static_cast<float>(i);
    io.out[2 * i] += mix_[2 * i] * g;
    io.out[2 * i + 1] += mix_[2 * i + 1] * g;
  }
  publishViz(frames);
}

int GranularNode::patternStep(const Voice& vo, double beat) noexcept {
  const long long k = static_cast<long long>(std::floor(beat / vo.stepBeats + 1e-9));
  return static_cast<int>(((k % 16) + 16) % 16);
}

int GranularNode::patternCell(const Voice& vo, int step) noexcept {
  return static_cast<int>((vo.patCells >> (4 * step)) & 15u) % vo.cellN;
}

int GranularNode::stepCell(const Voice& vo, double beat) noexcept {
  const double k = std::floor((beat - vo.stepOrigin) / vo.stepBeats + 1e-9);
  const long long n = vo.cellN;
  long long c = static_cast<long long>(k) % n;
  if (c < 0) c += n;
  return static_cast<int>(c);
}

void GranularNode::controlVoice(int v, int voiceLimit, int nGated, const AdaptiveQuality::Limits& lim, int grainCap,
                                const BlockTime& time) noexcept {
  Voice& vo = voices_[v];
  const VoiceParams& p = vo.p;
  const int frames = time.frames;
  const float dt = static_cast<float>(frames / sr_);

  const bool enabled = v < voiceLimit;
  if (!enabled && vo.enabledPrev) stealVoiceGrains(v);
  vo.enabledPrev = enabled;
  const bool gate = enabled && (p.gate || vo.noteGate);

  // Gate envelope: short attack, user release. Applied as a per-block gain ramp at the mixer.
  if (gate) {
    vo.level = std::min(1.f, vo.level + dt / kAttackSeconds);
  } else {
    vo.level = std::max(0.f, vo.level - dt / (p.releaseMs * 0.001f));
    if (vo.level <= 0.f && vo.liveGrains > 0) stealVoiceGrains(v);  // inaudible already: let the tail go
  }
  const bool gateRising = gate && !vo.gatePrev;
  if (gateRising) vo.sched.restart();
  vo.gatePrev = gate;

  // Source + scan head.
  const int entry = srcTable_.entryOfSlot(p.source);
  vo.srcEntry = entry;
  const SampleBuffer* buf = entry >= 0 ? srcTable_.buffer(entry) : nullptr;
  const double srcFrames = buf ? static_cast<double>(buf->frames()) : 0.0;

  if (p.position != vo.lastPos) {  // the position control moved: the scan head follows it
    vo.head = p.position;
    vo.lastPos = p.position;
  }
  const int mode = std::clamp(p.cellMode, 0, static_cast<int>(CellMode::Count) - 1);
  const bool hold = mode == static_cast<int>(CellMode::Hold);
  const bool stepping = mode == static_cast<int>(CellMode::Step);
  vo.cellN = p.cellCount == 1 ? 16 : 8;
  vo.stepBeats = stepDivisionBeats(p.stepDiv);
  if (stepping) {
    // Step 0 is the grid step the sequence (re)started in: a new note, a switch into Step mode or the reset button.
    if (gateRising || vo.cellModePrev != mode) vo.stepResetPending = true;
    if (vo.stepResetPending) {
      vo.stepOrigin = std::floor(time.clockBeat / vo.stepBeats) * vo.stepBeats;
      vo.stepResetPending = false;
    }
  }
  vo.cellModePrev = mode;
  if (p.scanOn && !p.freeze && hold && srcFrames > 1.0) {
    vo.head += static_cast<double>(p.scanSpeed) * frames / srcFrames;
    // Loop window [ls, le] always at least kMinLoop wide and inside [0, 1] (start = end = 1.0 used to give w = 0 -> NaN).
    constexpr double kMinLoop = 1e-4;
    const double a = std::min(p.loopStart, p.loopEnd);
    const double b = std::max(p.loopStart, p.loopEnd);
    const double ls = std::clamp(a, 0.0, 1.0 - kMinLoop);
    const double le = std::clamp(b, ls + kMinLoop, 1.0);
    if (vo.head >= le || vo.head < ls) {
      const double w = le - ls;
      double x = vo.head - ls;
      x -= std::floor(x / w) * w;
      vo.head = ls + x;
    }
    if (!(vo.head >= 0.0 && vo.head <= 1.0)) vo.head = ls;  // belt and braces: never let a NaN scan head reach the grains
  }
  if (hold) {
    vo.step = -1;
    vo.rest = false;
    vo.cell = -1;
    vo.effPos = static_cast<float>(p.scanOn ? vo.head : static_cast<double>(p.position));
  } else {
    // Cells: the source is cut into cellN equal slices; `position` becomes the offset inside the slice.
    vo.step = -1;
    vo.rest = false;
    if (stepping && p.seqOn) {
      vo.step = patternStep(vo, time.clockBeat);
      vo.rest = ((vo.patMask >> vo.step) & 1u) == 0;
      vo.cell = patternCell(vo, vo.step);
    } else {
      vo.cell = stepping ? stepCell(vo, time.clockBeat) : std::clamp(p.cellIndex, 0, vo.cellN - 1);
    }
    vo.effPos = (static_cast<float>(vo.cell) + std::clamp(p.position, 0.f, 1.f)) / static_cast<float>(vo.cellN);
  }

  // Effective density: user value x adaptive scale, capped so that the voice cannot fill more than
  // its share of the grain budget (the average number of simultaneous grains is density x size).
  const float sizeSec = p.sizeMs * 0.001f;
  float density = p.density * lim.densityScale;
  const float maxDensity = (static_cast<float>(grainCap) / static_cast<float>(nGated)) / sizeSec;
  density = std::min(density, maxDensity);

  float grainsPerSec = density;
  if (p.syncMode == static_cast<int>(SyncMode::Beat) && time.beatsPerSample > 0.0) {
    const double div = divisionBeats(static_cast<SyncDivision>(std::clamp(p.syncDiv, 0, static_cast<int>(SyncDivision::Count) - 1)));
    grainsPerSec = static_cast<float>(time.beatsPerSample * sr_ / div);
  }
  float comp = 1.f;
  if (p.autoGain) comp = 1.f / std::sqrt(std::max(1.f, grainsPerSec * sizeSec));
  vo.targetGain = vo.level * p.volume * vo.noteVel * comp;

  if (!gate || buf == nullptr) return;

  auto spawnAt = [&](int off) { spawnGrain(v, vo, off, buf, entry, time, grainCap); };
  if (p.syncMode == static_cast<int>(SyncMode::Beat)) {
    const double div = divisionBeats(static_cast<SyncDivision>(std::clamp(p.syncDiv, 0, static_cast<int>(SyncDivision::Count) - 1)));
    time.forEachGridCrossing(div, spawnAt);
  } else {
    vo.sched.run(frames, static_cast<double>(density), sr_, p.jitter, vo.rng, spawnAt);
  }
}

void GranularNode::spawnGrain(int v, Voice& vo, int offset, const SampleBuffer* buf, int entry,
                              const BlockTime& time, int grainCap) noexcept {
  GrainPool& gp = *pool_;
  if (gp.activeCount() >= grainCap) {
    ++dropped_;
    return;
  }
  const VoiceParams& p = vo.p;
  const double frames = static_cast<double>(buf->frames());
  const double maxSpan = frames - 8.0;
  if (maxSpan < 16.0) return;  // source too short to granulate

  // Fixed number of random draws per grain, so automating one parameter does not reshuffle the
  // random streams of the others.
  const float rPitch = vo.rng.bipolar();
  const float rRev = vo.rng.uniform();
  const float rSpray = vo.rng.bipolar();
  const float rPan = vo.rng.bipolar();
  const float rAmp = vo.rng.uniform();

  float st = p.pitchSt + vo.notePitch + rPitch * p.pitchRandSt;
  st = std::clamp(st, -48.f, 48.f);
  const float rate = std::exp2(st * (1.0f / 12.0f));
  const bool reversed = rRev < p.reverseProb;

  int len = std::max(kMinGrainSamples, static_cast<int>(p.sizeMs * 0.001f * static_cast<float>(sr_) + 0.5f));
  double span = static_cast<double>(len) * rate;
  if (span > maxSpan) {  // grain would not fit into a short source: shorten it
    len = std::max(kMinGrainSamples, static_cast<int>(maxSpan / rate));
    span = static_cast<double>(len) * rate;
  }

  float effPos = vo.effPos;
  if (p.cellMode == static_cast<int>(CellMode::Step)) {  // the grain's own start beat decides its cell, not the block start
    const double beat = time.clockBeat + static_cast<double>(offset) * time.beatsPerSample;
    int cell;
    if (p.seqOn) {
      const int s = patternStep(vo, beat);
      if (((vo.patMask >> s) & 1u) == 0) return;  // a rest in the pattern: this grain is not played
      cell = patternCell(vo, s);
    } else {
      cell = stepCell(vo, beat);
    }
    effPos = (static_cast<float>(cell) + std::clamp(p.position, 0.f, 1.f)) / static_cast<float>(vo.cellN);
  }
  double pos = static_cast<double>(effPos) * (frames - 1.0) + static_cast<double>(rSpray) * p.sprayMs * 0.001 * sr_;
  const double lo = 2.0;
  const double hi = std::max(lo, frames - 4.0 - span);
  // NaN-safe clamp: a NaN position must land on `lo`, never reach the kernels (std::clamp would pass it through).
  pos = (pos >= lo) ? (pos <= hi ? pos : hi) : lo;

  const float pan = rPan * p.spread;  // stereo balance: -1 = left only ... +1 = right only
  const float amp = 1.f - p.ampRand * rAmp;

  const int slot = gp.allocate();
  if (slot < 0) {
    ++dropped_;
    return;
  }
  gp.pos[slot] = reversed ? pos + span : pos;
  gp.rate[slot] = reversed ? -rate : rate;
  gp.envPos[slot] = 0.f;
  gp.envStep[slot] = static_cast<float>(kEnvTableSize) / static_cast<float>(len);
  gp.gainL[slot] = amp * std::min(1.f, 1.f - pan);
  gp.gainR[slot] = amp * std::min(1.f, 1.f + pan);
  gp.remaining[slot] = len;
  gp.length[slot] = len;
  gp.startOffset[slot] = offset;
  gp.fadeLeft[slot] = 0;
  gp.fadeGain[slot] = 1.f;
  gp.fadeStep[slot] = 0.f;
  gp.voice[slot] = static_cast<std::uint8_t>(v);
  gp.shape[slot] = static_cast<std::uint8_t>(std::clamp(p.envShape, 0, static_cast<int>(EnvShape::Count) - 1));
  gp.srcEntry[slot] = static_cast<std::uint8_t>(entry);
  srcTable_.acquire(entry);
  ++vo.liveGrains;
  ++spawned_;
  if (observer_) observer_(observerCtx_, v, time.engineSample + offset, gp.pos[slot], len, gp.rate[slot], reversed);
}

void GranularNode::renderGrains(int frames, const AdaptiveQuality::Limits& lim, GarbageQueue& garbage) noexcept {
  GrainPool& gp = *pool_;
  for (int v = 0; v < ids_.numVoices; ++v) {
    Voice& vo = voices_[v];
    vo.rendered = vo.liveGrains > 0;
    if (vo.rendered) std::memset(bus_[v], 0, static_cast<std::size_t>(frames) * 2 * sizeof(float));
  }

  const int simd = useSimd_ ? 1 : 0;
  GrainKernelFn kernelFor[kMaxVoices];
  for (int v = 0; v < ids_.numVoices; ++v) {
    const bool hermite = voices_[v].p.quality == static_cast<int>(InterpQuality::Hermite) && !lim.forceLinear;
    kernelFor[v] = kernels_[simd][hermite ? 1 : 0];
  }

  // Descending: recycle() moves the last active grain into the freed position, which was already visited.
  for (int ai = gp.activeCount() - 1; ai >= 0; --ai) {
    const int s = gp.activeSlot(ai);
    const int off = gp.startOffset[s];
    const bool fading = gp.fadeLeft[s] > 0;
    int n = std::min(gp.remaining[s], frames - off);
    if (fading) n = std::min(n, gp.fadeLeft[s]);
    const int v = gp.voice[s];
    Voice& vo = voices_[v];

    if (n > 0) {
      const SampleBuffer* buf = srcTable_.buffer(gp.srcEntry[s]);
      GrainJob job;
      job.src = buf->data();
      job.pos = gp.pos[s];
      job.rate = gp.rate[s];
      job.envPos = gp.envPos[s];
      job.envStep = gp.envStep[s];
      job.env = env_->table(gp.shape[s]);
      job.gainL = gp.gainL[s];
      job.gainR = gp.gainR[s];
      job.n = n;
      float* bus = bus_[v] + 2 * off;
      if (!fading) {
        job.out = bus;
        kernelFor[v](job);
      } else {
        std::memset(scratch_, 0, static_cast<std::size_t>(n) * 2 * sizeof(float));
        job.out = scratch_;
        kernelFor[v](job);
        const float g0 = gp.fadeGain[s];
        const float step = gp.fadeStep[s];
        for (int i = 0; i < n; ++i) {
          const float g = std::max(0.f, g0 - step * static_cast<float>(i));
          bus[2 * i] += scratch_[2 * i] * g;
          bus[2 * i + 1] += scratch_[2 * i + 1] * g;
        }
        gp.fadeGain[s] = std::max(0.f, g0 - step * static_cast<float>(n));
        gp.fadeLeft[s] -= n;
      }
      vo.rendered = true;
      gp.pos[s] += static_cast<double>(n) * static_cast<double>(gp.rate[s]);
      gp.envPos[s] += static_cast<float>(n) * gp.envStep[s];
      gp.remaining[s] -= n;
      grainSamples_ += static_cast<std::uint64_t>(n);
    }
    gp.startOffset[s] = 0;

    if (gp.remaining[s] <= 0 || (fading && gp.fadeLeft[s] <= 0)) {
      srcTable_.release(gp.srcEntry[s], garbage);
      --vo.liveGrains;
      gp.recycle(s);
    }
  }
}

void GranularNode::mixVoices(float* out, int frames) noexcept {
  int active = 0;
  for (int v = 0; v < ids_.numVoices; ++v) {
    Voice& vo = voices_[v];
    const float g0 = vo.gainPrev;
    const float g1 = vo.targetGain;
    vo.gainPrev = g1;
    if (!vo.rendered) continue;
    ++active;
    const float dg = (g1 - g0) / static_cast<float>(frames);
    const float* bus = bus_[v];
    for (int i = 0; i < frames; ++i) {
      const float g = g0 + dg * static_cast<float>(i);
      out[2 * i] += bus[2 * i] * g;
      out[2 * i + 1] += bus[2 * i + 1] * g;
    }
  }
  lastActiveVoices_ = active;
}

void GranularNode::publishViz(int frames) noexcept {
  if (!viz_) return;
  vizAccum_ += frames;
  if (vizAccum_ < static_cast<int>(sr_ / kVizRateHz)) return;
  vizAccum_ = 0;

  GrainViz& g = viz_->writeSlot();
  const GrainPool& gp = *pool_;
  g.voices = ids_.numVoices;
  for (int v = 0; v < kVizMaxVoices; ++v) {
    g.playhead[v] = v < ids_.numVoices ? voices_[v].effPos : 0.f;
    g.voiceLevel[v] = v < ids_.numVoices ? voices_[v].targetGain : 0.f;
    g.cell[v] = v < ids_.numVoices ? (voices_[v].rest ? -1 : voices_[v].cell) : -1;
    g.step[v] = v < ids_.numVoices ? voices_[v].step : -1;
  }
  const int count = gp.activeCount();
  const int stride = std::max(1, (count + kVizMaxGrains - 1) / kVizMaxGrains);
  int n = 0;
  for (int ai = 0; ai < count && n < kVizMaxGrains; ai += stride) {
    const int s = gp.activeSlot(ai);
    const SampleBuffer* buf = srcTable_.buffer(gp.srcEntry[s]);
    const double frames_ = buf ? static_cast<double>(buf->frames()) : 1.0;
    GrainVizItem& it = g.grains[n++];
    it.pos = static_cast<float>(std::clamp(gp.pos[s] / frames_, 0.0, 1.0));
    it.size = static_cast<float>(gp.length[s]) / (0.5f * static_cast<float>(sr_));
    it.pan = gp.gainR[s] - gp.gainL[s];
    it.age = 1.f - static_cast<float>(gp.remaining[s]) / static_cast<float>(gp.length[s]);
    it.rate = gp.rate[s];
    it.voice = static_cast<float>(gp.voice[s]);
  }
  g.count = n;
  viz_->publish();
}

GranularStats GranularNode::stats() const noexcept {
  GranularStats s;
  s.activeGrains = pool_ ? pool_->activeCount() : 0;
  s.activeVoices = lastActiveVoices_;
  s.qualityLevel = quality_.level();
  s.droppedGrains = dropped_;
  s.spawnedGrains = spawned_;
  s.grainSamples = grainSamples_;
  return s;
}

}  // namespace autodj::granular
