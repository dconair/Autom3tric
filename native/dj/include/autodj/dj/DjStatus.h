#pragma once
// Plain-data snapshot of the DJ section the audio thread publishes once per callback (through a TripleBuffer).

#include <cstdint>

namespace autodj::dj {

struct DeckStatus {
  std::int32_t loaded = 0;
  std::int32_t playing = 0;
  std::int32_t ended = 0;
  std::int32_t trackId = 0;
  double posSec = 0.0;       // heard position in the track
  double durationSec = 0.0;
  double rate = 1.0;         // tempo ratio currently applied
  double bpm = 0.0;          // effective tempo = native bpm * rate (0 = unknown)
  double beatPhase = 0.0;    // 0..1 within the current beat of the deck's own grid
  float peakL = 0.f, peakR = 0.f;
};

struct PlanStatus {
  std::int32_t active = 0;
  std::int32_t finished = 0;
  std::int32_t item = -1;        // item that started last (the one playing "on top")
  std::int32_t transition = -1;  // transition currently running, -1 = none
  std::int32_t waiting = -1;     // item that is due but whose audio is not bound yet (-1 = none)
  double timeSec = 0.0;
  double totalSec = 0.0;
  double beat = 0.0;
  double bpm = 0.0;
};

struct DjStatus {
  DeckStatus deck[2];
  PlanStatus plan;
  std::int32_t padStep = -1;  // step the drum pad sequencer is on (0..15), -1 = off; filled in by the engine
};

}  // namespace autodj::dj
