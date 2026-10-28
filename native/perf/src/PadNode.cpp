#include "autodj/perf/PadNode.h"

#include <algorithm>
#include <cmath>

#include "autodj/core/Dsp.h"

namespace autodj::perf {

namespace {
constexpr double kStepBeats = 0.25;  // a 16th note
inline int stepOf(long long k) noexcept { return static_cast<int>(((k % kSteps) + kSteps) % kSteps); }
}  // namespace

void PadNode::prepare(const ProcessSpec& spec) {
  sr_ = spec.sampleRate > 8000.0 ? spec.sampleRate : 48000.0;
  kit_.render(sr_);
  releaseStep_ = static_cast<float>(1.0 / (0.004 * sr_));
  for (int p = 0; p < kPads; ++p) rows_[p] = defaultPatternRow(p);
  reset();
}

void PadNode::reset() noexcept {
  for (Voice& v : voices_) v = Voice{};
  volSmooth_ = BlockSmoother{};
  volPrev_ = 0.f;
  seqWasOn_ = false;
  stepShown_ = -1;
}

int PadNode::activeVoices() const noexcept {
  int n = 0;
  for (const Voice& v : voices_) n += v.pad >= 0 ? 1 : 0;
  return n;
}

void PadNode::setParams(const ParamStore& st, const BlockTime& t) noexcept {
  volume_ = st.get(ids_.padVolume);
  seqOn_ = st.getBool(ids_.padSeqOn);
  swing_ = std::clamp(st.get(ids_.padSwing), 0.f, 0.6f);
  const float coef = onePoleBlockCoef(12.f, static_cast<float>(t.frames / t.sampleRate));
  volPrev_ = volSmooth_.primed ? volSmooth_.value : volume_;
  volSmooth_.next(volume_, coef);
}

bool PadNode::handleCommand(const Command& cmd, GarbageQueue&) noexcept {
  switch (cmd.type) {
    case CmdType::PadTrigger:
      trigger(cmd.target & 15, cmd.f0 > 0.f ? std::min(cmd.f0, 1.5f) : 1.f, 0);
      return true;
    case CmdType::PadRow:
      rows_[cmd.target & 15] = static_cast<unsigned>(cmd.i0) & 0xFFFFu;
      return true;
    case CmdType::ResetEngine:
      reset();
      return false;  // the other nodes reset as well
    default:
      return false;
  }
}

void PadNode::trigger(int pad, float vel, int delay) noexcept {
  const PadSound& snd = kit_.sound(pad);
  if (snd.data.empty()) return;
  if (snd.chokeGroup != 0) {
    for (Voice& v : voices_) {
      if (v.pad >= 0 && v.pad != pad && kit_.sound(v.pad).chokeGroup == snd.chokeGroup) v.releasing = true;
    }
  }
  int slot = -1;
  for (int i = 0; i < kVoices; ++i) {
    if (voices_[i].pad < 0) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {  // all busy: take the oldest voice that is already sounding
    std::uint64_t oldest = ~0ull;
    for (int i = 0; i < kVoices; ++i) {
      if (voices_[i].delay == 0 && voices_[i].age < oldest) {
        oldest = voices_[i].age;
        slot = i;
      }
    }
    if (slot < 0) slot = 0;
  }
  Voice& v = voices_[slot];
  v.pad = pad;
  v.pos = 0;
  v.delay = std::max(0, delay);
  v.vel = vel;
  v.fade = 1.f;
  v.releasing = false;
  v.age = ++ageCounter_;
}

void PadNode::runSequencer(const MasterClock& clk, int frames) noexcept {
  const double bpm = std::clamp(clk.bpm, 40.0, 240.0);
  const double bps = bpm / (60.0 * sr_);  // beats per sample
  const double b0 = clk.beat;
  const double b1 = b0 + frames * bps;
  // a jump of the clock (a seek, a deck taking over, the plan starting) restarts the grid walk at the new position
  if (!seqWasOn_ || std::fabs(b0 - expectBeat_) > 0.15) {
    nextK_ = static_cast<long long>(std::ceil((b0 - 0.01) / kStepBeats));
  }
  seqWasOn_ = true;
  expectBeat_ = b1;
  const double swingBeats = swing_ * 0.5 * kStepBeats;
  for (int guard = 0; guard < 64; ++guard) {
    const double eb = static_cast<double>(nextK_) * kStepBeats + ((nextK_ & 1) ? swingBeats : 0.0);
    if (eb >= b1) break;
    if (eb >= b0 - 0.01) {
      const int step = stepOf(nextK_);
      const int off = std::clamp(static_cast<int>((eb - b0) / bps + 0.5), 0, frames - 1);
      // a gentle accent profile on the groove instruments: downbeats full, 8ths a little less, 16ths softer
      const float prof = (step & 3) == 0 ? 1.f : (step & 1) == 0 ? 0.85f : 0.7f;
      for (int p = 0; p < kPads; ++p) {
        if ((rows_[p] >> step) & 1u) trigger(p, kit_.sound(p).accentProfile ? prof : 1.f, off);
      }
    }
    ++nextK_;
  }
  stepShown_ = stepOf(static_cast<long long>(std::floor(b1 / kStepBeats)));
}

void PadNode::process(ProcessIO& io) noexcept {
  const int frames = io.frames;
  if (seqOn_ && io.clock != nullptr) {
    runSequencer(*io.clock, frames);
  } else {
    seqWasOn_ = false;
    stepShown_ = -1;
  }

  const float dg = (volSmooth_.value - volPrev_) / static_cast<float>(frames);
  float* out = io.out;
  for (Voice& v : voices_) {
    if (v.pad < 0) continue;
    const PadSound& snd = kit_.sound(v.pad);
    const std::size_t len = snd.data.size();
    const float* src = snd.data.data();
    const float gl = snd.gainL * v.vel * 1.2f, gr = snd.gainR * v.vel * 1.2f;
    int i = 0;
    if (v.delay > 0) {
      i = std::min(v.delay, frames);
      v.delay -= i;
    }
    for (; i < frames; ++i) {
      if (v.pos >= len) {
        v.pad = -1;
        break;
      }
      if (v.releasing) {
        v.fade -= releaseStep_;
        if (v.fade <= 0.f) {
          v.pad = -1;
          break;
        }
      }
      const float vol = volPrev_ + dg * static_cast<float>(i);
      const float s = src[v.pos++] * v.fade * vol;
      out[2 * i] += s * gl;
      out[2 * i + 1] += s * gr;
    }
  }
}

}  // namespace autodj::perf
