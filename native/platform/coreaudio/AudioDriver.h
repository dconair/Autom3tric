#pragma once
// CoreAudio output (macOS) that drives autodj::Engine - the desktop counterpart of the Oboe driver on Android.
//
// Same public surface as native/platform/oboe/AudioDriver.h (DriverOptions, DriverInfo, start/stop/info), so the JNI
// bridge compiles unchanged for both. One AudioUnit (default output device, float stereo interleaved at the engine's
// sample rate; the unit converts to the device format and follows default-device changes such as headphones being
// plugged in). The render callback only calls Engine::process() plus two relaxed atomics: no logging, no locks,
// no allocation. Everything else runs on the control thread and may lock.

#include <AudioToolbox/AudioToolbox.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

#include "autodj/engine/Engine.h"

namespace autodj::platform {

struct DriverOptions {
  bool exclusive = true;  // ignored on macOS (shared mode only); kept for API parity with Android
  int deviceId = 0;       // 0 = system default output device
  int sampleRate = 48000;
  int framesPerCallback = 0;  // 0 = whatever the device uses
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
  ~AudioDriver();
  AudioDriver(const AudioDriver&) = delete;
  AudioDriver& operator=(const AudioDriver&) = delete;

  /// 0 = OK, otherwise the failing OSStatus (the reason is in DriverInfo::lastError).
  int start(const DriverOptions& opts);
  void stop();
  DriverInfo info();  // control thread

 private:
  static OSStatus renderProc(void* self, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* ts, UInt32 bus,
                             UInt32 frames, AudioBufferList* io);
  void stopLocked();

  Engine& engine_;
  std::mutex mutex_;  // start/stop/info only
  AudioUnit unit_ = nullptr;
  bool running_ = false;
  int sampleRate_ = 0;
  std::string lastError_;
  std::atomic<std::uint32_t> lastFrames_{0};
  std::atomic<std::uint64_t> callbacks_{0};
};

}  // namespace autodj::platform
