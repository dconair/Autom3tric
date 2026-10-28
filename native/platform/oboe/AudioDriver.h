#pragma once
// Oboe output stream that drives autodj::Engine.
//
// Stream setup: AAudio where available, PerformanceMode::LowLatency, SharingMode::Exclusive (falls back to
// shared on its own), float stereo, device sample rate and burst size, Oboe's LatencyTuner (grows the
// buffer by one burst after an underrun), ADPF performance hints through Oboe. The data callback does nothing
// but Engine::process() plus two cheap atomics - no JNI, no logging, no locks, no allocation.
//
// Everything except onAudioReady() runs on control/Oboe-owned threads and may lock.

#include <oboe/Oboe.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "autodj/engine/Engine.h"

namespace autodj::platform {

struct DriverOptions {
  bool exclusive = true;
  int deviceId = oboe::kUnspecified;
  int sampleRate = 48000;
  int framesPerCallback = oboe::kUnspecified;  // unspecified = whatever the device bursts at
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

class AudioDriver final : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
 public:
  explicit AudioDriver(Engine& engine) : engine_(engine) {}
  ~AudioDriver() override;

  oboe::Result start(const DriverOptions& opts);
  void stop();
  DriverInfo info();  // control thread

  // oboe::AudioStreamDataCallback - AUDIO THREAD
  oboe::DataCallbackResult onAudioReady(oboe::AudioStream* stream, void* audioData, int32_t numFrames) override;
  // oboe::AudioStreamErrorCallback - runs on a thread Oboe creates for this purpose, not the audio thread
  void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) override;

 private:
  oboe::Result openAndStartLocked();

  Engine& engine_;
  std::mutex mutex_;  // start/stop/restart/info only
  DriverOptions opts_;
  std::shared_ptr<oboe::AudioStream> stream_;
  std::unique_ptr<oboe::LatencyTuner> tuner_;
  bool wantRunning_ = false;
  int restarts_ = 0;
  std::string lastError_;
  std::atomic<std::uint32_t> callbackCount_{0};
};

}  // namespace autodj::platform
