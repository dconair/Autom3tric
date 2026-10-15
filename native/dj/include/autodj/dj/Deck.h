#pragma once
// One DJ deck: plays an analysed Track through a key-preserving time stretcher, with 3-band EQ, sweep filter and gain.
//
// Position model. The stretcher is fed `nIn` source frames per `frames` output frames; the *heard* source position lags the input
// cursor by `lag(rate) = inputLatency + rate * outputLatency` source frames. Deck::process() is told, per block, either only the
// nominal rate (free running) or an absolute position the heard audio should reach by the end of the block (plan / sync); in the
// second case the cursor chases it, with the deviation from the nominal rate capped (`maxCorr`), so beat phase can never drift and a
// sync pull-in stays a gentle tempo bend instead of a jump. Starting or seeking uses the stretcher's outputSeek(), which makes the
// very first output sample correspond to the requested source position (no start latency).
//
// Audio thread only (prepare() allocates and runs on the control thread).

#include <cstdint>
#include <memory>

#include "autodj/core/Command.h"
#include "autodj/dj/DjDsp.h"
#include "autodj/engine/Tracks.h"

namespace autodj::dj {

struct DeckBlock {
  double rate = 1.0;        // nominal tempo ratio: source frames consumed per output frame
  bool hasTarget = false;   // chase an absolute position (plan / sync)
  double targetEnd = 0.0;   //   heard source position, in frames, wanted at the end of this block
  double maxCorr = 0.0;     //   largest allowed deviation from `rate` while chasing, as a fraction (0.03 = +-3 %)
  bool keylock = true;      // false: pitch follows the tempo like a record
  float gain0 = 1.f, gain1 = 1.f;
  float eq0[3] = {1.f, 1.f, 1.f}, eq1[3] = {1.f, 1.f, 1.f};  // linear gains of low / mid / high
  float filter0 = 0.f, filter1 = 0.f;                        // sweep filter knob, -1..1
};

struct DeckOutput {
  float peakL = 0.f, peakR = 0.f;  // of the post-gain signal in the last block
};

class Deck {
 public:
  Deck();
  ~Deck();
  Deck(const Deck&) = delete;
  Deck& operator=(const Deck&) = delete;

  /// Control thread: allocates the stretcher. Must be called before the deck is used.
  void prepare(double sampleRate);
  int latencyIn() const noexcept { return inLat_; }
  int latencyOut() const noexcept { return outLat_; }

  // ---- audio thread -------------------------------------------------------------------------------
  /// Raw track pointer swap: the caller owns the lifetime (see DjNode). Stops playback and parks the playhead at `startSec`.
  void setTrack(const Track* t, double startSec = 0.0) noexcept;
  const Track* track() const noexcept { return track_; }
  /// Start playing at the given heard position (seconds); restarts with a short fade if already playing.
  void play(double fromSec) noexcept;
  /// Fades out and stops, keeping the position.
  void pause() noexcept;
  /// Moves the playhead; keeps playing if it was playing.
  void seek(double sec) noexcept;
  /// Repeats [startSec, startSec + lengthSec) until clearLoop(): the stretcher is fed the loop over and over, so playback stays
  /// seamless and the heard position wraps inside the loop. Replaces a running loop. A seek outside the loop ends it.
  void setLoop(double startSec, double lengthSec) noexcept;
  void clearLoop() noexcept;
  bool looping() const noexcept { return loopLen_ > 0; }
  /// Bumped whenever a loop starts or ends (sync uses it to forget its beat offset to the other deck).
  std::uint32_t loopGeneration() const noexcept { return loopGen_; }

  bool playing() const noexcept { return streaming_ || wantStart_ >= 0.0; }
  /// Position (frames) a pending start will begin at, or -1.
  double pendingStartFrames() const noexcept { return wantStart_; }
  bool streaming() const noexcept { return streaming_; }
  bool ended() const noexcept { return ended_; }
  double heardFrames() const noexcept;
  double heardSec() const noexcept { return heardFrames() / sr_; }
  double lastRate() const noexcept { return lastRate_; }
  DeckOutput output() const noexcept { return out_; }

  /// Adds this deck's block to the interleaved stereo buffer `out`.
  void process(float* out, int frames, const DeckBlock& b) noexcept;

  static constexpr int kMaxFrames = 512;

 private:
  struct Impl;
  double lag(double rate) const noexcept { return inLat_ + rate * outLat_; }
  void startStream(double startFrames, double rate) noexcept;
  void fillInput(std::int64_t from, int count) noexcept;
  /// Source frame the (virtual, ever growing) stream position `v` reads from: wraps inside the loop once it is past its end.
  std::int64_t mapLoop(std::int64_t v) const noexcept {
    return loopLen_ > 0 && v >= loopStart_ + loopLen_ ? loopStart_ + (v - loopStart_) % loopLen_ : v;
  }

  std::unique_ptr<Impl> impl_;
  const Track* track_ = nullptr;
  double sr_ = 48000.0;
  int inLat_ = 0, outLat_ = 0;

  bool streaming_ = false;
  bool stopRequested_ = false;
  double wantStart_ = -1.0;   // pending start position in frames, < 0 = none
  bool ended_ = false;
  double pausedAt_ = 0.0;     // heard position (frames) while not streaming
  double restAt_ = -1.0;      // a seek that arrived while the pause was fading out: where the deck rests afterwards, < 0 = none
  std::int64_t cursor_ = 0;   // next source frame handed to the stretcher
  double cursorExact_ = 0.0;
  double lastRate_ = 1.0;
  std::int64_t loopStart_ = 0, loopLen_ = 0;  // loopLen_ 0 = no loop (frames)
  std::uint32_t loopGen_ = 0;
  float fadeGain_ = 1.f;
  double lastTranspose_ = 1.0;
  DeckOutput out_;

  Eq3 eq_;
  SweepFilter filter_;
  alignas(64) float outL_[kMaxFrames];
  alignas(64) float outR_[kMaxFrames];
};

}  // namespace autodj::dj
