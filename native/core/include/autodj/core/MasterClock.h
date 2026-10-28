#pragma once
// The musical clock of the current block: where the beat grid is and how fast it runs. Tempo-synced effects and the drum pad
// sequencer follow it. It comes from the running mix plan or the playing deck (so that effects sit on the beats of the music),
// otherwise from the free-running engine clock.

namespace autodj {

struct MasterClock {
  double bpm = 120.0;
  double beat = 0.0;     // position in beats at the first frame of the block; integer beats are beats, multiples of 4 are bar lines
  bool fromMusic = false;  // true when derived from a plan or a deck, false when it is the free-running engine clock
};

}  // namespace autodj
