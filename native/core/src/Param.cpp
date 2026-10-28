#include "autodj/core/Param.h"

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

namespace autodj {

namespace {

float roundForType(ParamType t, float v) {
  if (t == ParamType::Float) return v;
  return std::nearbyint(v);
}

}  // namespace

ParamId ParamRegistry::add(ParamSpec spec) {
  if (frozen_ || spec.key.empty() || byKey_.count(spec.key) != 0) return kInvalidParam;
  if (specs_.size() >= kInvalidParam) return kInvalidParam;
  if (!(spec.min < spec.max) && spec.type == ParamType::Float) return kInvalidParam;
  if (spec.type == ParamType::Bool) {
    spec.min = 0.f;
    spec.max = 1.f;
  }
  if (spec.type == ParamType::Enum) {
    spec.min = 0.f;
    spec.max = spec.enumNames.empty() ? 0.f : static_cast<float>(spec.enumNames.size() - 1);
  }
  if (spec.scale == ParamScale::Log && spec.min <= 0.f) return kInvalidParam;
  spec.def = std::clamp(spec.def, spec.min, spec.max);
  const auto id = static_cast<ParamId>(specs_.size());
  byKey_.emplace(spec.key, id);
  ranges_.push_back({spec.min, spec.max, spec.def});
  specs_.push_back(std::move(spec));
  return id;
}

void ParamRegistry::freeze() { frozen_ = true; }

ParamId ParamRegistry::find(std::string_view key) const {
  const auto it = byKey_.find(std::string(key));
  return it == byKey_.end() ? kInvalidParam : it->second;
}

float ParamRegistry::clamp(ParamId id, float plain) const {
  const Range& r = ranges_[id];
  if (!std::isfinite(plain)) return r.def;
  return roundForType(specs_[id].type, std::clamp(plain, r.min, r.max));
}

float ParamRegistry::toNormalized(ParamId id, float plain) const {
  const ParamSpec& s = specs_[id];
  plain = std::clamp(plain, s.min, s.max);
  if (s.max <= s.min) return 0.f;
  if (s.scale == ParamScale::Log) return std::log(plain / s.min) / std::log(s.max / s.min);
  return (plain - s.min) / (s.max - s.min);
}

float ParamRegistry::fromNormalized(ParamId id, float norm) const {
  const ParamSpec& s = specs_[id];
  if (!std::isfinite(norm)) return s.def;
  norm = std::clamp(norm, 0.f, 1.f);
  float v;
  if (s.scale == ParamScale::Log) {
    v = s.min * std::pow(s.max / s.min, norm);
  } else {
    v = s.min + norm * (s.max - s.min);
  }
  return roundForType(s.type, std::clamp(v, s.min, s.max));
}

std::string ParamRegistry::specsToJson() const {
  nlohmann::json arr = nlohmann::json::array();
  for (std::size_t i = 0; i < specs_.size(); ++i) {
    const ParamSpec& s = specs_[i];
    nlohmann::json j;
    j["id"] = i;
    j["key"] = s.key;
    j["label"] = s.label;
    j["group"] = s.group;
    j["unit"] = s.unit;
    j["min"] = s.min;
    j["max"] = s.max;
    j["def"] = s.def;
    j["type"] = static_cast<int>(s.type);
    j["scale"] = static_cast<int>(s.scale);
    j["flags"] = s.flags;
    if (!s.enumNames.empty()) j["enum"] = s.enumNames;
    arr.push_back(std::move(j));
  }
  return arr.dump();
}

ParamStore::ParamStore(const ParamRegistry& reg) : reg_(reg), values_(new std::atomic<float>[reg.size()]) {
  resetToDefaults();
}

void ParamStore::set(ParamId id, float plain) noexcept {
  if (id >= reg_.size()) return;
  values_[id].store(reg_.clamp(id, plain), std::memory_order_relaxed);
}

void ParamStore::setNormalized(ParamId id, float norm) noexcept {
  if (id >= reg_.size()) return;
  values_[id].store(reg_.fromNormalized(id, norm), std::memory_order_relaxed);
}

void ParamStore::resetToDefaults() noexcept {
  for (std::size_t i = 0; i < reg_.size(); ++i) {
    values_[i].store(reg_.spec(static_cast<ParamId>(i)).def, std::memory_order_relaxed);
  }
}

}  // namespace autodj
