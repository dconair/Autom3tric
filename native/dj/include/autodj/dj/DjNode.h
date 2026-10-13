#pragma once
// The DJ section of the audio graph: two decks, a crossfader, and the player of an automatic mix plan.
//
//   live mode  decks are driven by the parameters (volume, EQ, filter, tempo, sync) and by commands (load, play, seek)
//   plan mode  a MixPlan drives both decks: master beat clock, deck start/stop, tempo ramps, automation lanes. The
//              parameter faders stay active as a multiplier / offset on top of the plan, the crossfader is ignored.
//
// Lifetime of what the audio thread points at: a Track loaded by DeckLoad stays alive until the audio thread hands the
// pointer back through the garbage queue (Kind::Track); the tracks of a plan live inside the plan, which is handed back
// (Kind::Plan) only after the decks that use its tracks have been cleared.

#include <array>
#include <atomic>

#include "autodj/core/Node.h"
#include "autodj/dj/Deck.h"
#include "autodj/dj/DjParams.h"
#include "autodj/dj/DjStatus.h"
#include "autodj/dj/MixPlan.h"

namespace autodj::dj {

class DjNode final : public Node {
 public:
  explicit DjNode(const DjParamIds& ids) : ids_(ids) {}

  void prepare(const ProcessSpec& spec) override;
  void reset() noexcept override {}
  void setParams(const ParamStore& st, const BlockTime& t) noexcept override;
  void process(ProcessIO& io) noexcept override;
  bool handleCommand(const Command& cmd, GarbageQueue& garbage) noexcept override;

  /// Audio thread, before process(): the beat grid of what is playing, for tempo-synced effects and the pad sequencer. Plan
  /// running: the plan's master clock. Otherwise the louder playing deck's own beat grid. `fallback` (the free-running
  /// engine clock) when no tempo is known.
  MasterClock masterClock(const BlockTime& fallback) const noexcept;

  /// Audio thread, after process(): the state for the UI.
  void fillStatus(DjStatus& out) noexcept;

  /// Test hook: bit d set = deck d is audible, the others still run but are muted (-1 = all audible).
  void setDebugAudibleMask(int mask) noexcept { audibleMask_.store(mask, std::memory_order_relaxed); }

 private:
  struct Ctl {  // one smoothed control (gain, EQ band, filter)
    float v = 0.f;
    bool primed = false;
  };
  struct DeckCtl {
    Ctl gain, eq[3], filter;
  };
  struct Snapshot {
    float volume[2] = {1.f, 1.f}, trimDb[2] = {0.f, 0.f};
    bool autoTrim[2] = {true, true}, sync[2] = {false, false}, keylock[2] = {true, true};
    float eqDb[2][3] = {{0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}};
    float filter[2] = {0.f, 0.f};
    float tempoPct[2] = {0.f, 0.f};
    float crossfader = 0.f;
    int curve = 0;
  };

  void processLive(ProcessIO& io) noexcept;
  void processPlan(ProcessIO& io) noexcept;
  void renderPlanBlock(float* out, int n, double m0, double m1) noexcept;
  void fireDue() noexcept;
  void startItem(int index) noexcept;
  void launchItem(int index, double beat) noexcept;
  void finishPlan() noexcept;
  void stopPlan(GarbageQueue& garbage) noexcept;
  void clearManualDecks(GarbageQueue& garbage) noexcept;
  void release(const Track* t, GarbageQueue& garbage) noexcept;
  bool audible(int d) const noexcept {
    const int m = audibleMask_.load(std::memory_order_relaxed);
    return m < 0 || (m >> d & 1);
  }
  DeckBlock tone(int d, DeckCtl& ctl, float gainTarget, const float (&eqDbTarget)[3], float filterTarget, float coef) noexcept;
  static void crossfaderGains(float x, int curve, float (&g)[2]) noexcept;
  float trimGain(int d) const noexcept;

  DjParamIds ids_;
  double sr_ = 48000.0;
  Snapshot pv_;
  Deck decks_[2];
  DeckCtl ctl_[2];
  bool owned_[2] = {false, false};  // deck's track came through DeckLoad (the control side expects it back)
  bool syncLocked_[2] = {false, false};
  double syncDelta_[2] = {0.0, 0.0};
  float peak_[2][2] = {{0.f, 0.f}, {0.f, 0.f}};
  std::atomic<int> audibleMask_{-1};

  const MixPlan* plan_ = nullptr;
  bool finished_ = false;
  double m_ = 0.0;       // master beat
  double tSec_ = 0.0;    // seconds since the plan started
  int nextItem_ = 0;
  int active_[2] = {-1, -1};
  int waiting_[2] = {-1, -1};  // item due on this deck whose audio is not bound yet: it joins in as soon as it is
  int lastStarted_ = -1;
  GarbageQueue* garbage_ = nullptr;
};

}  // namespace autodj::dj
