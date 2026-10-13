#include "autodj/dj/DjNode.h"

#include <algorithm>
#include <cmath>

namespace autodj::dj {

namespace {
constexpr double kPi = 3.14159265358979323846;

double beatOf(const analysis::TrackAnalysis& a, double frames, double sr) noexcept {
  return (frames / sr - a.gridOffsetSec) / a.beatSec;
}
double framesOfBeat(const analysis::TrackAnalysis& a, double beat, double sr) noexcept {
  return (a.gridOffsetSec + beat * a.beatSec) * sr;
}
}  // namespace

void DjNode::prepare(const ProcessSpec& spec) {
  sr_ = spec.sampleRate;
  for (Deck& d : decks_) d.prepare(sr_);
}

void DjNode::setParams(const ParamStore& st, const BlockTime&) noexcept {
  for (int d = 0; d < 2; ++d) {
    const DeckParamIds& id = ids_.deck[d];
    pv_.volume[d] = st.get(id.volume);
    pv_.trimDb[d] = st.get(id.trimDb);
    pv_.autoTrim[d] = st.getBool(id.autoTrim);
    pv_.eqDb[d][0] = st.get(id.eqLow);
    pv_.eqDb[d][1] = st.get(id.eqMid);
    pv_.eqDb[d][2] = st.get(id.eqHigh);
    pv_.filter[d] = st.get(id.filter);
    pv_.tempoPct[d] = st.get(id.tempoPct);
    pv_.sync[d] = st.getBool(id.sync);
    pv_.keylock[d] = st.getBool(id.keylock);
  }
  pv_.crossfader = st.get(ids_.crossfader);
  pv_.curve = st.getInt(ids_.curve);
}

// ------------------------------------------------------------------------------------------------------
// commands
// ------------------------------------------------------------------------------------------------------

void DjNode::release(const Track* t, GarbageQueue& garbage) noexcept {
  if (t == nullptr) return;
  GarbageMsg m;
  m.kind = GarbageMsg::Kind::Track;
  m.ptr = t;
  (void)garbage.tryPush(m);  // queue full: the control side keeps the track until shutdown
}

void DjNode::clearManualDecks(GarbageQueue& garbage) noexcept {
  for (int d = 0; d < 2; ++d) {
    if (owned_[d]) release(decks_[d].track(), garbage);
    owned_[d] = false;
    decks_[d].setTrack(nullptr);
  }
}

void DjNode::finishPlan() noexcept {
  for (int d = 0; d < 2; ++d) {
    if (!owned_[d]) decks_[d].setTrack(nullptr);
    active_[d] = -1;
    waiting_[d] = -1;
  }
  finished_ = true;
}

void DjNode::stopPlan(GarbageQueue& garbage) noexcept {
  if (plan_ == nullptr) return;
  finishPlan();
  GarbageMsg m;
  m.kind = GarbageMsg::Kind::Plan;
  m.ptr = plan_;
  (void)garbage.tryPush(m);
  plan_ = nullptr;
  finished_ = false;
}

bool DjNode::handleCommand(const Command& cmd, GarbageQueue& garbage) noexcept {
  const int d = cmd.target & 1;
  switch (cmd.type) {
    case CmdType::DeckLoad: {
      const Track* t = static_cast<const Track*>(cmd.payload.ptr);
      if (plan_ != nullptr) {  // decks belong to the plan while it runs: hand the track straight back
        release(t, garbage);
        return true;
      }
      if (owned_[d]) release(decks_[d].track(), garbage);
      decks_[d].setTrack(t, cmd.f0);
      owned_[d] = t != nullptr;
      syncLocked_[d] = false;
      return true;
    }
    case CmdType::DeckPlay:
      if (plan_ != nullptr || decks_[d].track() == nullptr) return true;
      if (cmd.i0 != 0) {
        const double at = decks_[d].ended() ? 0.0 : decks_[d].heardSec();
        if (!decks_[d].playing()) decks_[d].play(at);
      } else {
        decks_[d].pause();
      }
      return true;
    case CmdType::DeckSeek:
      if (plan_ == nullptr) decks_[d].seek(cmd.payload.d);
      return true;
    case CmdType::PlanStart: {
      const MixPlan* p = static_cast<const MixPlan*>(cmd.payload.ptr);
      if (plan_ != nullptr) stopPlan(garbage);
      clearManualDecks(garbage);
      plan_ = p;
      finished_ = false;
      m_ = 0.0;
      tSec_ = 0.0;
      nextItem_ = 0;
      active_[0] = active_[1] = -1;
      waiting_[0] = waiting_[1] = -1;
      lastStarted_ = -1;
      for (DeckCtl& c : ctl_) c = DeckCtl{};
      return true;
    }
    case CmdType::PlanStop:
      stopPlan(garbage);
      return true;
    default:
      return false;
  }
}

// ------------------------------------------------------------------------------------------------------
// shared helpers
// ------------------------------------------------------------------------------------------------------

float DjNode::trimGain(int d) const noexcept {
  const Track* t = decks_[d].track();
  const float auto_ = (pv_.autoTrim[d] && t != nullptr) ? dbToLin(static_cast<float>(t->analysis.trimDb)) : 1.f;
  return auto_ * dbToLin(pv_.trimDb[d]);
}

void DjNode::crossfaderGains(float x, int curve, float (&g)[2]) noexcept {
  const float t = std::clamp((x + 1.f) * 0.5f, 0.f, 1.f);
  switch (curve) {
    case 1:  // equal power
      g[0] = std::cos(t * static_cast<float>(kPi) * 0.5f);
      g[1] = std::sin(t * static_cast<float>(kPi) * 0.5f);
      break;
    case 2:  // cut: full level almost all the way, a steep fall at the very end
      g[0] = std::clamp((1.f - t) * 6.f, 0.f, 1.f);
      g[1] = std::clamp(t * 6.f, 0.f, 1.f);
      break;
    default:  // blend: both decks at full level in the middle (typical DJ mixer)
      g[0] = std::min(1.f, 2.f * (1.f - t));
      g[1] = std::min(1.f, 2.f * t);
      break;
  }
}

DeckBlock DjNode::tone(int d, DeckCtl& ctl, float gainTarget, const float (&eqDbTarget)[3], float filterTarget, float coef) noexcept {
  (void)d;
  DeckBlock b;
  auto step = [&](Ctl& c, float target) {
    if (!c.primed) {
      c.v = target;
      c.primed = true;
    }
    const float prev = c.v;
    c.v += (target - c.v) * coef;
    return std::pair<float, float>(prev, c.v);
  };
  const auto g = step(ctl.gain, gainTarget);
  b.gain0 = g.first;
  b.gain1 = g.second;
  for (int i = 0; i < 3; ++i) {
    const auto e = step(ctl.eq[i], eqDbTarget[i]);
    b.eq0[i] = eqDbToGain(e.first);
    b.eq1[i] = eqDbToGain(e.second);
  }
  const auto f = step(ctl.filter, filterTarget);
  b.filter0 = f.first;
  b.filter1 = f.second;
  return b;
}

// ------------------------------------------------------------------------------------------------------
// live mode
// ------------------------------------------------------------------------------------------------------

void DjNode::processLive(ProcessIO& io) noexcept {
  const int n = io.frames;
  const float coef = onePoleBlockCoef(10.f, static_cast<float>(n / sr_));
  float gx[2];
  crossfaderGains(pv_.crossfader, pv_.curve, gx);

  double rNom[2];
  for (int d = 0; d < 2; ++d) rNom[d] = std::clamp(1.0 + pv_.tempoPct[d] / 100.0, 0.5, 2.0);
  const bool follow[2] = {pv_.sync[0] && !pv_.sync[1], pv_.sync[1]};

  for (int d = 0; d < 2; ++d) {
    Deck& dk = decks_[d];
    const Track* t = dk.track();
    if (t == nullptr) {
      syncLocked_[d] = false;
      continue;
    }
    const analysis::TrackAnalysis& a = t->analysis;
    DeckBlock blk;
    blk.keylock = pv_.keylock[d];
    double rate = rNom[d];

    const int o = 1 - d;
    const Deck& lead = decks_[o];
    const Track* lt = lead.track();
    const bool canSync = follow[d] && lt != nullptr && lt->analysis.hasTempo() && a.hasTempo();
    if (canSync) {
      const analysis::TrackAnalysis& la = lt->analysis;
      rate = std::clamp(la.bpm * rNom[o] / a.bpm, 0.5, 2.0);
      if (lead.streaming()) {
        if (!dk.streaming() && dk.playing()) {
          // quantised start: shift the pending start so that this deck's beats fall on the leader's beats
          const double want = dk.pendingStartFrames();
          const double bF = beatOf(a, want, sr_);
          const double bL = beatOf(la, lead.heardFrames(), sr_);
          dk.play(framesOfBeat(a, bL + std::round(bF - bL), sr_) / sr_);
        }
        if (dk.streaming()) {
          const double leadEnd = lead.heardFrames() + n * rNom[o];
          const double bLEnd = beatOf(la, leadEnd, sr_);
          if (!syncLocked_[d]) {
            syncDelta_[d] = std::round(beatOf(a, dk.heardFrames(), sr_) - beatOf(la, lead.heardFrames(), sr_));
            syncLocked_[d] = true;
          }
          blk.hasTarget = true;
          blk.targetEnd = framesOfBeat(a, bLEnd + syncDelta_[d], sr_);
          blk.maxCorr = 0.06;
        }
      } else {
        syncLocked_[d] = false;
      }
    } else {
      syncLocked_[d] = false;
    }
    blk.rate = rate;

    const float eqTarget[3] = {pv_.eqDb[d][0], pv_.eqDb[d][1], pv_.eqDb[d][2]};
    DeckBlock tn = tone(d, ctl_[d], pv_.volume[d] * trimGain(d) * gx[d], eqTarget, pv_.filter[d], coef);
    tn.rate = blk.rate;
    tn.hasTarget = blk.hasTarget;
    tn.targetEnd = blk.targetEnd;
    tn.maxCorr = blk.maxCorr;
    tn.keylock = blk.keylock;
    if (!audible(d)) tn.gain0 = tn.gain1 = 0.f;
    dk.process(io.out, n, tn);
    peak_[d][0] = std::max(peak_[d][0], dk.output().peakL);
    peak_[d][1] = std::max(peak_[d][1], dk.output().peakR);
  }
}

// ------------------------------------------------------------------------------------------------------
// plan mode
// ------------------------------------------------------------------------------------------------------

void DjNode::launchItem(int index, double beat) noexcept {
  const PlanItem& it = plan_->items[index];
  const Track* t = plan_->audioOf(index);
  Deck& dk = decks_[it.deck];
  dk.setTrack(t, 0.0);
  owned_[it.deck] = false;
  dk.play(it.secOfTrackBeat(it.trackBeatAt(beat)));
  active_[it.deck] = index;
  waiting_[it.deck] = -1;
  ctl_[it.deck] = DeckCtl{};  // first block of the song starts at its lane values, no ramp from the previous song's settings
}

void DjNode::startItem(int index) noexcept {
  const PlanItem& it = plan_->items[index];
  lastStarted_ = index;
  if (plan_->audioOf(index) == nullptr) {  // decoding is late: stay silent on this deck and join in when the audio arrives
    decks_[it.deck].setTrack(nullptr, 0.0);
    active_[it.deck] = -1;
    waiting_[it.deck] = index;
    return;
  }
  launchItem(index, it.startBeat);
}

void DjNode::fireDue() noexcept {
  const MixPlan& P = *plan_;
  constexpr double kEps = 1e-9;
  for (int d = 0; d < 2; ++d) {
    if (active_[d] >= 0 && P.items[active_[d]].endBeat <= m_ + kEps) {
      decks_[d].setTrack(nullptr);
      active_[d] = -1;
    }
  }
  for (int d = 0; d < 2; ++d) {
    const int w = waiting_[d];
    if (w < 0) continue;
    if (P.items[w].endBeat - 1.0 <= m_) {
      waiting_[d] = -1;  // too late to join, the slot is over
    } else if (P.audioOf(w) != nullptr) {
      launchItem(w, m_);
    }
  }
  const int count = static_cast<int>(P.items.size());
  while (nextItem_ < count && P.items[nextItem_].startBeat <= m_ + kEps) startItem(nextItem_++);
  if (m_ >= P.totalBeats - kEps || (nextItem_ >= count && active_[0] < 0 && active_[1] < 0 && waiting_[0] < 0 && waiting_[1] < 0)) finishPlan();
}

void DjNode::renderPlanBlock(float* out, int n, double m0, double m1) noexcept {
  const MixPlan& P = *plan_;
  for (int d = 0; d < 2; ++d) {
    if (active_[d] < 0) continue;
    const PlanItem& it = P.items[active_[d]];
    Deck& dk = decks_[d];
    const double s0 = it.secOfTrackBeat(it.trackBeatAt(m0)) * sr_;
    const double s1 = it.secOfTrackBeat(it.trackBeatAt(m1)) * sr_;

    float eq[3];
    eq[0] = P.laneValue(d, LaneTarget::EqLow, m1) + pv_.eqDb[d][0];
    eq[1] = P.laneValue(d, LaneTarget::EqMid, m1) + pv_.eqDb[d][1];
    eq[2] = P.laneValue(d, LaneTarget::EqHigh, m1) + pv_.eqDb[d][2];
    for (float& e : eq) e = std::clamp(e, kEqKillDb, 6.f);
    const float filter = std::clamp(P.laneValue(d, LaneTarget::Filter, m1) + pv_.filter[d], -1.f, 1.f);
    const float gain = P.laneValue(d, LaneTarget::Gain, m1) * pv_.volume[d] * trimGain(d);

    DeckBlock b = tone(d, ctl_[d], gain, eq, filter, 1.f);
    b.rate = (s1 - s0) / n;
    b.hasTarget = true;
    b.targetEnd = s1;
    b.maxCorr = 0.03;
    b.keylock = true;
    if (!audible(d)) b.gain0 = b.gain1 = 0.f;
    dk.process(out, n, b);
    peak_[d][0] = std::max(peak_[d][0], dk.output().peakL);
    peak_[d][1] = std::max(peak_[d][1], dk.output().peakR);
  }
}

void DjNode::processPlan(ProcessIO& io) noexcept {
  const MixPlan& P = *plan_;
  const int total = io.frames;
  float* out = io.out;
  int done = 0;
  while (done < total) {
    fireDue();
    if (finished_) break;
    double nextBeat = P.totalBeats;
    if (nextItem_ < static_cast<int>(P.items.size())) nextBeat = std::min(nextBeat, P.items[nextItem_].startBeat);
    for (int d = 0; d < 2; ++d) {
      if (active_[d] >= 0) nextBeat = std::min(nextBeat, P.items[active_[d]].endBeat);
    }
    const double framesToEvent = P.secondsBetween(m_, nextBeat) * sr_;
    int n = total - done;
    bool atEvent = false;
    if (framesToEvent <= n) {
      n = std::max(1, static_cast<int>(std::ceil(framesToEvent - 1e-6)));
      atEvent = true;
    }
    const double mEnd = atEvent ? std::max(nextBeat, m_) : P.beatAfter(m_, n / sr_);
    renderPlanBlock(out, n, m_, mEnd);
    m_ = mEnd;
    tSec_ += n / sr_;
    out += 2 * n;
    done += n;
  }
}

void DjNode::process(ProcessIO& io) noexcept {
  if (plan_ != nullptr && !finished_) processPlan(io);
  else processLive(io);
}

MasterClock DjNode::masterClock(const BlockTime& fallback) const noexcept {
  MasterClock c;
  c.bpm = fallback.bpm();
  c.beat = fallback.clockBeat;
  if (plan_ != nullptr && !finished_) {
    c.bpm = plan_->bpmAt(m_);
    c.beat = m_;
    c.fromMusic = true;
    return c;
  }
  float gx[2];
  crossfaderGains(pv_.crossfader, pv_.curve, gx);
  int best = -1;
  float bestW = -1.f;
  for (int d = 0; d < 2; ++d) {
    const Deck& dk = decks_[d];
    const Track* t = dk.track();
    if (t == nullptr || !dk.streaming() || !t->analysis.hasTempo() || t->analysis.beatSec <= 0.0) continue;
    const float w = pv_.volume[d] * gx[d];
    if (w > bestW) {
      bestW = w;
      best = d;
    }
  }
  if (best >= 0) {
    const Deck& dk = decks_[best];
    const analysis::TrackAnalysis& a = dk.track()->analysis;
    c.bpm = 60.0 / a.beatSec * dk.lastRate();
    c.beat = (dk.heardFrames() / sr_ - a.gridOffsetSec) / a.beatSec;
    c.fromMusic = true;
  }
  return c;
}

// ------------------------------------------------------------------------------------------------------
// status
// ------------------------------------------------------------------------------------------------------

void DjNode::fillStatus(DjStatus& out) noexcept {
  for (int d = 0; d < 2; ++d) {
    DeckStatus& s = out.deck[d];
    const Deck& dk = decks_[d];
    const Track* t = dk.track();
    s = DeckStatus{};
    s.loaded = t != nullptr;
    s.playing = dk.playing();
    s.ended = dk.ended();
    s.peakL = peak_[d][0];
    s.peakR = peak_[d][1];
    peak_[d][0] = peak_[d][1] = 0.f;
    if (t == nullptr) continue;
    s.trackId = t->id;
    s.posSec = dk.heardSec();
    s.durationSec = t->durationSec();
    s.rate = dk.lastRate();
    if (t->analysis.hasTempo()) {
      s.bpm = t->analysis.bpm * s.rate;
      const double b = (s.posSec - t->analysis.gridOffsetSec) / t->analysis.beatSec;
      s.beatPhase = b - std::floor(b);
    }
  }
  PlanStatus& p = out.plan;
  p = PlanStatus{};
  if (plan_ != nullptr) {
    p.active = finished_ ? 0 : 1;
    p.finished = finished_ ? 1 : 0;
    p.item = lastStarted_;
    p.waiting = waiting_[0] >= 0 ? waiting_[0] : waiting_[1];
    p.timeSec = tSec_;
    p.totalSec = plan_->totalSec;
    p.beat = m_;
    p.bpm = plan_->bpmAt(m_);
    for (std::size_t i = 0; i < plan_->transitions.size(); ++i) {
      const TransitionInfo& t = plan_->transitions[i];
      if (m_ >= t.startBeat && m_ < t.startBeat + t.lengthBeats) {
        p.transition = static_cast<std::int32_t>(i);
        break;
      }
    }
  }
}

}  // namespace autodj::dj
