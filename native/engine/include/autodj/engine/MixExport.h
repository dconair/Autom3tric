#pragma once
// Offline rendering of a mix plan into a WAV file: a private Engine without audio hardware plays the plan as fast as the CPU
// allows. The caller drives it block by block (step), so that it can decode and bind the audio of the upcoming items in between,
// the same way live playback does (see dj::PlanAudio), and can show progress or cancel.

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "autodj/dj/MixPlan.h"

namespace autodj {

class Engine;

class MixExporter {
 public:
  /// bitDepth: 16 (TPDF dither) or 24.
  MixExporter(std::shared_ptr<const dj::MixPlan> plan, int sampleRate, int bitDepth = 16);
  ~MixExporter();
  MixExporter(const MixExporter&) = delete;
  MixExporter& operator=(const MixExporter&) = delete;

  bool open(const std::string& path);
  /// Renders up to `frames` frames into the file. False once the mix is complete or after an error (see error()).
  bool step(int frames);
  /// Patches the WAV header and closes the file; `discard` removes the file instead (cancelled export).
  bool close(bool discard = false);

  bool done() const { return done_; }
  const std::string& error() const { return error_; }
  std::int64_t framesWritten() const { return written_; }
  std::int64_t framesExpected() const { return expected_; }
  int lastStartedItem() const { return item_; }
  int waitingItem() const { return waiting_; }
  float peak() const { return peak_; }

 private:
  void writeHeader();

  std::shared_ptr<const dj::MixPlan> plan_;
  int rate_;
  int bits_;
  std::unique_ptr<Engine> engine_;
  std::FILE* file_ = nullptr;
  std::string path_;
  std::string error_;
  std::vector<float> block_;
  std::vector<std::uint8_t> bytes_;
  std::int64_t written_ = 0, expected_ = 0;
  std::uint32_t rng_ = 0x2545F491u;
  float peak_ = 0.f;
  int item_ = -1, waiting_ = -1;
  bool started_ = false, done_ = false;
};

}  // namespace autodj
