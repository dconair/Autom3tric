#pragma once
// Central parameter registry + lock-free value store.
//
// The registry is the single source of truth for every controllable value: UI sliders,
// automation lanes, MIDI mappings, the modulation matrix and preset serialization all address
// parameters by ParamId (a dense index) and discover range/default/unit/curve from here.
//
//   Registry  : built once on the control thread, then frozen (immutable) -> safe to read anywhere
//   ParamStore: one std::atomic<float> per parameter; relaxed loads/stores, no locks
//
// The audio thread only ever calls ParamStore::get().

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace autodj {

using ParamId = std::uint16_t;
inline constexpr ParamId kInvalidParam = 0xFFFF;

enum class ParamType : std::uint8_t { Float, Int, Bool, Enum };
enum class ParamScale : std::uint8_t { Linear, Log };

enum ParamFlags : std::uint32_t {
  kParamAutomatable = 1u << 0,
  kParamModulatable = 1u << 1,
  kParamHidden = 1u << 2,  // not shown in generic UIs (internal)
};

struct ParamSpec {
  std::string key;    // stable serialization id, e.g. "granular.v0.size_ms". Never rename without a migration.
  std::string label;  // UI label
  std::string group;  // UI grouping, e.g. "Granular Voice 1"
  std::string unit;   // "ms", "Hz", "st", "%", "" ...
  float min = 0.f;
  float max = 1.f;
  float def = 0.f;
  ParamType type = ParamType::Float;
  ParamScale scale = ParamScale::Linear;
  float smoothMs = 0.f;  // control-rate smoothing time constant, 0 = none (consumers decide how to apply it)
  std::uint32_t flags = kParamAutomatable | kParamModulatable;
  std::vector<std::string> enumNames;  // for ParamType::Enum
};

class ParamRegistry {
 public:
  /// Adds a parameter. Only valid before freeze(). Returns kInvalidParam on error (duplicate key, bad range).
  ParamId add(ParamSpec spec);

  /// Makes the registry immutable and builds the key lookup. Idempotent.
  void freeze();
  bool frozen() const { return frozen_; }

  std::size_t size() const { return specs_.size(); }
  const ParamSpec& spec(ParamId id) const { return specs_[id]; }
  ParamId find(std::string_view key) const;

  float clamp(ParamId id, float plain) const;
  /// plain value -> [0,1], honouring ParamScale (used by sliders, MIDI CC, XY pads)
  float toNormalized(ParamId id, float plain) const;
  float fromNormalized(ParamId id, float norm) const;

  /// Whole registry as JSON (key, label, group, unit, min, max, def, type, scale, flags, enum names).
  /// The Kotlin UI builds its generic controls from this.
  std::string specsToJson() const;

 private:
  struct Range {
    float min, max, def;
  };
  std::vector<ParamSpec> specs_;
  std::vector<Range> ranges_;  // compact copy for clamp() on hot paths
  std::unordered_map<std::string, ParamId> byKey_;
  bool frozen_ = false;
};

class ParamStore {
 public:
  explicit ParamStore(const ParamRegistry& reg);
  ParamStore(const ParamStore&) = delete;
  ParamStore& operator=(const ParamStore&) = delete;

  /// Audio-thread safe.
  float get(ParamId id) const noexcept { return values_[id].load(std::memory_order_relaxed); }
  int getInt(ParamId id) const noexcept {
    const float v = get(id);
    return static_cast<int>(v < 0.f ? v - 0.5f : v + 0.5f);
  }
  bool getBool(ParamId id) const noexcept { return get(id) >= 0.5f; }

  /// Any thread. Value is clamped to the registered range (Bool/Int/Enum are rounded).
  void set(ParamId id, float plain) noexcept;
  void setNormalized(ParamId id, float norm) noexcept;
  void resetToDefaults() noexcept;

  const ParamRegistry& registry() const { return reg_; }

 private:
  const ParamRegistry& reg_;
  std::unique_ptr<std::atomic<float>[]> values_;
};

}  // namespace autodj
