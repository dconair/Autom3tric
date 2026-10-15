#include "autodj/engine/MixExport.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "autodj/engine/Engine.h"

namespace autodj {

namespace {
void put32(std::uint8_t* p, std::uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
void put16(std::uint8_t* p, std::uint16_t v) {
  p[0] = static_cast<std::uint8_t>(v);
  p[1] = static_cast<std::uint8_t>(v >> 8);
}
}  // namespace

MixExporter::MixExporter(std::shared_ptr<const dj::MixPlan> plan, int sampleRate, int bitDepth)
    : plan_(std::move(plan)), rate_(sampleRate), bits_(bitDepth == 24 ? 24 : 16) {
  EngineConfig cfg;
  cfg.sampleRate = rate_;
  cfg.realtime = false;
  engine_ = std::make_unique<Engine>(cfg);
  expected_ = plan_ ? static_cast<std::int64_t>(std::ceil(plan_->totalSec * rate_)) : 0;
}

MixExporter::~MixExporter() {
  if (file_ != nullptr) close(true);
}

void MixExporter::writeHeader() {
  const std::uint32_t bytesPerFrame = static_cast<std::uint32_t>(bits_ / 8) * 2;
  const std::uint32_t dataBytes = static_cast<std::uint32_t>(std::min<std::int64_t>(written_ * bytesPerFrame, 0xFFFFFF00LL));
  std::uint8_t h[44];
  std::memcpy(h, "RIFF", 4);
  put32(h + 4, 36 + dataBytes);
  std::memcpy(h + 8, "WAVEfmt ", 8);
  put32(h + 16, 16);
  put16(h + 20, 1);
  put16(h + 22, 2);
  put32(h + 24, static_cast<std::uint32_t>(rate_));
  put32(h + 28, static_cast<std::uint32_t>(rate_) * bytesPerFrame);
  put16(h + 32, static_cast<std::uint16_t>(bytesPerFrame));
  put16(h + 34, static_cast<std::uint16_t>(bits_));
  std::memcpy(h + 36, "data", 4);
  put32(h + 40, dataBytes);
  std::fseek(file_, 0, SEEK_SET);
  std::fwrite(h, 1, sizeof h, file_);
  std::fseek(file_, 0, SEEK_END);
}

bool MixExporter::open(const std::string& path) {
  if (!plan_ || plan_->items.empty()) {
    error_ = "Der Mix ist leer";
    return false;
  }
  path_ = path;
  file_ = std::fopen(path.c_str(), "wb");
  if (file_ == nullptr) {
    error_ = "Die Datei kann nicht geschrieben werden: " + path;
    return false;
  }
  writeHeader();
  return true;
}

bool MixExporter::step(int frames) {
  if (done_ || file_ == nullptr) return false;
  if (!started_) {
    if (!engine_->planStart(plan_)) {
      error_ = "Der Mix konnte nicht gestartet werden";
      done_ = true;
      return false;
    }
    started_ = true;
  }
  frames = std::clamp(frames, 1, 1 << 16);
  const std::int64_t left = expected_ - written_;
  if (left <= 0) {
    done_ = true;
    return false;
  }
  const int n = static_cast<int>(std::min<std::int64_t>(frames, left));
  block_.resize(static_cast<std::size_t>(n) * 2);
  engine_->process(block_.data(), n);
  const int bytesPer = bits_ / 8;
  bytes_.resize(static_cast<std::size_t>(n) * 2 * static_cast<std::size_t>(bytesPer));
  std::uint8_t* o = bytes_.data();
  auto next = [this]() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_ >> 8) * (1.0f / 16777216.0f);
  };
  for (std::size_t i = 0; i < block_.size(); ++i) {
    float v = std::clamp(block_[i], -1.f, 1.f);
    peak_ = std::max(peak_, std::fabs(v));
    if (bits_ == 16) {
      const float d = next() - next();  // TPDF, +-1 LSB
      const int q = static_cast<int>(std::lrintf(std::clamp(v * 32767.f + d, -32768.f, 32767.f)));
      put16(o, static_cast<std::uint16_t>(static_cast<std::int16_t>(q)));
      o += 2;
    } else {
      const std::int32_t q = static_cast<std::int32_t>(std::lrintf(v * 8388607.f));
      o[0] = static_cast<std::uint8_t>(q);
      o[1] = static_cast<std::uint8_t>(q >> 8);
      o[2] = static_cast<std::uint8_t>(q >> 16);
      o += 3;
    }
  }
  if (std::fwrite(bytes_.data(), 1, bytes_.size(), file_) != bytes_.size()) {
    error_ = "Schreibfehler (Datenträger voll?)";
    done_ = true;
    return false;
  }
  written_ += n;
  engine_->collectGarbage();
  dj::DjStatus st;
  if (engine_->readDjStatus(st)) {
    item_ = st.plan.item;
    waiting_ = st.plan.waiting;
    if (st.plan.finished) {
      // the plan can end a few samples before the rounded-up length: fill with the silence it would have ended with
      done_ = true;
    }
  }
  if (written_ >= expected_) done_ = true;
  return !done_;
}

bool MixExporter::close(bool discard) {
  if (file_ == nullptr) return false;
  if (!discard) writeHeader();
  const bool ok = std::fclose(file_) == 0;
  file_ = nullptr;
  if (discard) std::remove(path_.c_str());
  return ok && !discard && error_.empty();
}

}  // namespace autodj
