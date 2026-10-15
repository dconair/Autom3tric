#include "AudioDriver.h"

#include <CoreAudio/CoreAudio.h>

#include <cstring>

namespace autodj::platform {
namespace {

// Element 0 is kAudioObjectPropertyElementMain (named ...Master before macOS 12); the literal avoids the deprecation.
constexpr AudioObjectPropertyElement kMainElement = 0;

AudioDeviceID defaultOutputDevice() {
  AudioObjectPropertyAddress a{kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kMainElement};
  AudioDeviceID dev = kAudioObjectUnknown;
  UInt32 size = sizeof dev;
  if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, &size, &dev) != noErr) return kAudioObjectUnknown;
  return dev;
}

template <typename T>
bool deviceProperty(AudioDeviceID dev, AudioObjectPropertySelector sel, AudioObjectPropertyScope scope, T* out) {
  AudioObjectPropertyAddress a{sel, scope, kMainElement};
  UInt32 size = sizeof(T);
  return AudioObjectGetPropertyData(dev, &a, 0, nullptr, &size, out) == noErr;
}

std::string statusText(const char* what, OSStatus s) { return std::string(what) + " failed (OSStatus " + std::to_string(static_cast<int>(s)) + ")"; }

}  // namespace

AudioDriver::~AudioDriver() { stop(); }

OSStatus AudioDriver::renderProc(void* self, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32 frames,
                                 AudioBufferList* io) {
  auto* d = static_cast<AudioDriver*>(self);
  if (io == nullptr || io->mNumberBuffers < 1) return noErr;
  AudioBuffer& b = io->mBuffers[0];
  if (b.mData == nullptr) return noErr;
  const UInt32 need = frames * 2u * static_cast<UInt32>(sizeof(float));
  if (b.mNumberChannels != 2 || b.mDataByteSize < need) {  // not the format we asked for: stay silent rather than scribble
    std::memset(b.mData, 0, b.mDataByteSize);
    return noErr;
  }
  d->engine_.process(static_cast<float*>(b.mData), static_cast<int>(frames));
  d->lastFrames_.store(frames, std::memory_order_relaxed);
  d->callbacks_.fetch_add(1, std::memory_order_relaxed);
  return noErr;
}

int AudioDriver::start(const DriverOptions& opts) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (running_) return 0;
  lastError_.clear();

  AudioComponentDescription desc{};
  desc.componentType = kAudioUnitType_Output;
  desc.componentSubType = kAudioUnitSubType_DefaultOutput;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
  if (comp == nullptr) {
    lastError_ = "no default output AudioComponent";
    return -1;
  }
  OSStatus s = AudioComponentInstanceNew(comp, &unit_);
  if (s != noErr) {
    lastError_ = statusText("AudioComponentInstanceNew", s);
    unit_ = nullptr;
    return static_cast<int>(s);
  }

  AudioStreamBasicDescription fmt{};
  fmt.mSampleRate = static_cast<Float64>(opts.sampleRate > 0 ? opts.sampleRate : 48000);
  fmt.mFormatID = kAudioFormatLinearPCM;
  fmt.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;  // interleaved
  fmt.mChannelsPerFrame = 2;
  fmt.mBitsPerChannel = 32;
  fmt.mBytesPerFrame = 2 * sizeof(float);
  fmt.mFramesPerPacket = 1;
  fmt.mBytesPerPacket = fmt.mBytesPerFrame;
  AURenderCallbackStruct cb{&AudioDriver::renderProc, this};

  s = AudioUnitSetProperty(unit_, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &fmt, sizeof fmt);
  if (s != noErr) lastError_ = statusText("set stream format", s);
  if (s == noErr) {
    s = AudioUnitSetProperty(unit_, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof cb);
    if (s != noErr) lastError_ = statusText("set render callback", s);
  }
  if (s == noErr) {
    s = AudioUnitInitialize(unit_);
    if (s != noErr) lastError_ = statusText("AudioUnitInitialize", s);
  }
  if (s == noErr) {
    s = AudioOutputUnitStart(unit_);
    if (s != noErr) {
      lastError_ = statusText("AudioOutputUnitStart", s);
      AudioUnitUninitialize(unit_);
    }
  }
  if (s != noErr) {
    AudioComponentInstanceDispose(unit_);
    unit_ = nullptr;
    return static_cast<int>(s);
  }
  sampleRate_ = static_cast<int>(fmt.mSampleRate);
  running_ = true;
  return 0;
}

void AudioDriver::stopLocked() {
  if (unit_ == nullptr) return;
  AudioOutputUnitStop(unit_);
  AudioUnitUninitialize(unit_);
  AudioComponentInstanceDispose(unit_);
  unit_ = nullptr;
  running_ = false;
}

void AudioDriver::stop() {
  std::lock_guard<std::mutex> lock(mutex_);
  stopLocked();
}

DriverInfo AudioDriver::info() {
  std::lock_guard<std::mutex> lock(mutex_);
  DriverInfo i;
  i.running = running_;
  i.api = "CoreAudio";
  i.sharing = "shared";
  i.performance = "default";
  i.sampleRate = sampleRate_;
  i.framesPerCallback = static_cast<int>(lastFrames_.load(std::memory_order_relaxed));
  i.lastError = lastError_;

  const AudioDeviceID dev = defaultOutputDevice();
  i.deviceId = static_cast<int>(dev);
  if (dev != kAudioObjectUnknown) {
    UInt32 bufFrames = 0, latency = 0, safety = 0;
    Float64 rate = 0;
    const auto out = kAudioObjectPropertyScopeOutput;
    deviceProperty(dev, kAudioDevicePropertyBufferFrameSize, out, &bufFrames);
    deviceProperty(dev, kAudioDevicePropertyLatency, out, &latency);
    deviceProperty(dev, kAudioDevicePropertySafetyOffset, out, &safety);
    deviceProperty(dev, kAudioDevicePropertyNominalSampleRate, out, &rate);
    i.framesPerBurst = static_cast<int>(bufFrames);
    i.bufferSize = static_cast<int>(bufFrames);
    i.bufferCapacity = static_cast<int>(bufFrames);
    if (i.framesPerCallback == 0) i.framesPerCallback = static_cast<int>(bufFrames);
    // Estimate: device buffer + device latency + safety offset. Not a measured round trip.
    if (rate > 0.0 && bufFrames > 0) i.latencyMs = 1000.0 * static_cast<double>(bufFrames + latency + safety) / rate;
  }
  return i;
}

}  // namespace autodj::platform
