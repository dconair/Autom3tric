#pragma once
// Driver without audio output, for hosts that have no driver of their own (Linux CI, tests of the JNI layer).
// Same public surface as the Oboe and CoreAudio drivers; start() succeeds but nothing pulls Engine::process().
// Tests render through the JNI `nativeRenderOffline` instead.

#include <string>

#include "autodj/engine/Engine.h"

namespace autodj::platform {

struct DriverOptions {
  bool exclusive = true;
  int deviceId = 0;
  int sampleRate = 48000;
  int framesPerCallback = 0;
};

struct DriverInfo {
  bool running = false;
  std::string api = "none";
  std::string sharing = "-";
  std::string performance = "-";
  int sampleRate = 0;
  int framesPerBurst = 0;
  int framesPerCallback = 0;
  int bufferSize = 0;
  int bufferCapacity = 0;
  double latencyMs = -1.0;
  int xruns = 0;
  int restarts = 0;
  int deviceId = 0;
  bool performanceHint = false;
  std::string lastError;
};

class AudioDriver final {
 public:
  explicit AudioDriver(Engine& engine) : engine_(engine) {}
  int start(const DriverOptions& opts) {
    running_ = true;
    sampleRate_ = opts.sampleRate;
    return 0;
  }
  void stop() { running_ = false; }
  DriverInfo info() {
    DriverInfo i;
    i.running = running_;
    i.api = "null (no audio device)";
    i.sampleRate = sampleRate_;
    return i;
  }

 private:
  Engine& engine_;
  bool running_ = false;
  int sampleRate_ = 0;
};

}  // namespace autodj::platform
