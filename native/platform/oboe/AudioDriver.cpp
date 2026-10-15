#include "AudioDriver.h"

#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "AutoDJ", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AutoDJ", __VA_ARGS__)

namespace autodj::platform {

AudioDriver::~AudioDriver() { stop(); }

oboe::Result AudioDriver::start(const DriverOptions& opts) {
  std::lock_guard<std::mutex> lk(mutex_);
  opts_ = opts;
  wantRunning_ = true;
  return openAndStartLocked();
}

void AudioDriver::stop() {
  std::shared_ptr<oboe::AudioStream> s;
  {
    std::lock_guard<std::mutex> lk(mutex_);
    wantRunning_ = false;
    s = std::move(stream_);
    tuner_.reset();
  }
  if (s) {
    s->requestStop();
    s->close();
  }
}

oboe::Result AudioDriver::openAndStartLocked() {
  oboe::AudioStreamBuilder b;
  b.setDirection(oboe::Direction::Output)
      ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
      ->setSharingMode(opts_.exclusive ? oboe::SharingMode::Exclusive : oboe::SharingMode::Shared)
      ->setFormat(oboe::AudioFormat::Float)
      ->setChannelCount(oboe::ChannelCount::Stereo)
      ->setSampleRate(opts_.sampleRate)
      ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium)
      ->setUsage(oboe::Usage::Media)
      ->setContentType(oboe::ContentType::Music)
      ->setDeviceId(opts_.deviceId)
      ->setFramesPerDataCallback(opts_.framesPerCallback)
      ->setDataCallback(this)
      ->setErrorCallback(this);

  std::shared_ptr<oboe::AudioStream> s;
  oboe::Result r = b.openStream(s);
  if (r != oboe::Result::OK) {
    lastError_ = std::string("openStream: ") + oboe::convertToText(r);
    LOGE("%s", lastError_.c_str());
    return r;
  }
  if (s->getSampleRate() != engine_.sampleRate()) {
    // The engine was built for opts_.sampleRate; Oboe resamples to it when the device runs at another rate.
    LOGI("stream rate %d differs from engine rate %d", s->getSampleRate(), engine_.sampleRate());
  }
  // Start with two bursts (low latency, some headroom); the tuner grows it after an underrun.
  s->setBufferSizeInFrames(2 * s->getFramesPerBurst());
  // ADPF: Oboe feeds the measured callback duration to the system's PerformanceHint session (where the device
  // implements it), so the scheduler can raise CPU frequency before an underrun instead of after.
  s->setPerformanceHintEnabled(true);
  tuner_ = std::make_unique<oboe::LatencyTuner>(*s);
  stream_ = s;
  r = s->requestStart();
  if (r != oboe::Result::OK) {
    lastError_ = std::string("requestStart: ") + oboe::convertToText(r);
    LOGE("%s", lastError_.c_str());
    s->close();
    stream_.reset();
    tuner_.reset();
    return r;
  }
  lastError_.clear();
  LOGI("stream started: api=%s sharing=%s burst=%d buffer=%d rate=%d", oboe::convertToText(s->getAudioApi()),
       oboe::convertToText(s->getSharingMode()), s->getFramesPerBurst(), s->getBufferSizeInFrames(), s->getSampleRate());
  return oboe::Result::OK;
}

oboe::DataCallbackResult AudioDriver::onAudioReady(oboe::AudioStream* stream, void* audioData, int32_t numFrames) {
  engine_.process(static_cast<float*>(audioData), numFrames);
  // The tuner and the driver's underrun counter are cheap, but there is no reason to poll them every callback.
  if ((callbackCount_.fetch_add(1, std::memory_order_relaxed) & 15u) == 0u) {
    if (tuner_) tuner_->tune();
    auto xr = stream->getXRunCount();
    if (xr) engine_.reportXruns(static_cast<std::uint32_t>(xr.value()));
  }
  return oboe::DataCallbackResult::Continue;
}

void AudioDriver::onErrorAfterClose(oboe::AudioStream*, oboe::Result error) {
  // E.g. headset unplugged / audio server restart: open a new stream on the new default route.
  std::lock_guard<std::mutex> lk(mutex_);
  lastError_ = std::string("stream closed: ") + oboe::convertToText(error);
  if (!wantRunning_) return;
  stream_.reset();
  tuner_.reset();
  ++restarts_;
  const oboe::Result r = openAndStartLocked();
  LOGI("stream restart #%d after %s -> %s", restarts_, oboe::convertToText(error), oboe::convertToText(r));
}

DriverInfo AudioDriver::info() {
  std::lock_guard<std::mutex> lk(mutex_);
  DriverInfo i;
  i.restarts = restarts_;
  i.lastError = lastError_;
  if (!stream_) return i;
  i.running = stream_->getState() == oboe::StreamState::Started;
  i.api = oboe::convertToText(stream_->getAudioApi());
  i.sharing = oboe::convertToText(stream_->getSharingMode());
  i.performance = oboe::convertToText(stream_->getPerformanceMode());
  i.sampleRate = stream_->getSampleRate();
  i.framesPerBurst = stream_->getFramesPerBurst();
  i.framesPerCallback = stream_->getFramesPerDataCallback();
  i.bufferSize = stream_->getBufferSizeInFrames();
  i.bufferCapacity = stream_->getBufferCapacityInFrames();
  i.deviceId = stream_->getDeviceId();
  i.performanceHint = stream_->isPerformanceHintEnabled();
  if (i.running) {
    auto lat = stream_->calculateLatencyMillis();
    if (lat) i.latencyMs = lat.value();
  }
  auto xr = stream_->getXRunCount();
  if (xr) i.xruns = xr.value();
  return i;
}

}  // namespace autodj::platform
