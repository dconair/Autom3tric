#include <gtest/gtest.h>

#include <atomic>
#include <set>
#include <thread>

#include "autodj/core/Param.h"
#include "autodj/core/ParamSerializer.h"
#include "autodj/engine/Engine.h"
#include "test_util.h"

using namespace autodj;

namespace {
ParamSpec floatSpec(const char* key, float mn, float mx, float def, ParamScale sc = ParamScale::Linear) {
  ParamSpec s;
  s.key = key;
  s.label = key;
  s.min = mn;
  s.max = mx;
  s.def = def;
  s.scale = sc;
  return s;
}
}  // namespace

TEST(ParamRegistry, RejectsDuplicatesBadRangesAndLateAdds) {
  ParamRegistry reg;
  EXPECT_NE(reg.add(floatSpec("a", 0, 1, 0.5f)), kInvalidParam);
  EXPECT_EQ(reg.add(floatSpec("a", 0, 1, 0.5f)), kInvalidParam) << "duplicate key";
  EXPECT_EQ(reg.add(floatSpec("b", 1, 1, 1)), kInvalidParam) << "empty range";
  EXPECT_EQ(reg.add(floatSpec("c", 0, 10, 1, ParamScale::Log)), kInvalidParam) << "log scale needs min > 0";
  EXPECT_EQ(reg.add(floatSpec("", 0, 1, 0)), kInvalidParam) << "empty key";
  reg.freeze();
  EXPECT_EQ(reg.add(floatSpec("d", 0, 1, 0)), kInvalidParam) << "frozen";
  EXPECT_EQ(reg.size(), 1u);
}

TEST(ParamRegistry, ClampNormalizeRoundTrip) {
  ParamRegistry reg;
  const ParamId lin = reg.add(floatSpec("lin", -24, 24, 0));
  const ParamId log = reg.add(floatSpec("log", 5, 500, 80, ParamScale::Log));
  ParamSpec en;
  en.key = "enum";
  en.type = ParamType::Enum;
  en.enumNames = {"A", "B", "C"};
  const ParamId e = reg.add(en);
  ParamSpec bl;
  bl.key = "bool";
  bl.type = ParamType::Bool;
  const ParamId b = reg.add(bl);
  reg.freeze();

  EXPECT_FLOAT_EQ(reg.clamp(lin, 100.f), 24.f);
  EXPECT_FLOAT_EQ(reg.clamp(lin, -100.f), -24.f);
  EXPECT_FLOAT_EQ(reg.clamp(lin, std::nanf("")), 0.f) << "NaN falls back to the default";
  EXPECT_FLOAT_EQ(reg.clamp(e, 1.6f), 2.f) << "enums are rounded";
  EXPECT_FLOAT_EQ(reg.clamp(e, 7.f), 2.f);
  EXPECT_FLOAT_EQ(reg.clamp(b, 0.4f), 0.f);

  for (float n : {0.f, 0.1f, 0.5f, 0.77f, 1.f}) {
    EXPECT_NEAR(reg.toNormalized(lin, reg.fromNormalized(lin, n)), n, 1e-5);
    EXPECT_NEAR(reg.toNormalized(log, reg.fromNormalized(log, n)), n, 1e-5);
  }
  EXPECT_NEAR(reg.fromNormalized(log, 0.5f), std::sqrt(5.f * 500.f), 1e-3) << "log scale: midpoint is the geometric mean";
  EXPECT_FLOAT_EQ(reg.fromNormalized(log, 0.f), 5.f);
  EXPECT_FLOAT_EQ(reg.fromNormalized(log, 1.f), 500.f);
}

TEST(ParamStore, SetClampsAndDefaultsAreApplied) {
  ParamRegistry reg;
  const ParamId a = reg.add(floatSpec("a", 0, 10, 3));
  reg.freeze();
  ParamStore st(reg);
  EXPECT_FLOAT_EQ(st.get(a), 3.f);
  st.set(a, 99.f);
  EXPECT_FLOAT_EQ(st.get(a), 10.f);
  st.set(a, 4.f);
  st.resetToDefaults();
  EXPECT_FLOAT_EQ(st.get(a), 3.f);
  st.set(kInvalidParam, 1.f);  // must not crash
}

TEST(ParamStore, ConcurrentSetAndGetAreRaceFree) {
  ParamRegistry reg;
  const ParamId a = reg.add(floatSpec("a", 0, 1000000, 0));
  reg.freeze();
  ParamStore st(reg);
  std::atomic<bool> stop{false};
  std::thread writer([&] {
    for (int i = 1; i <= 200000; ++i) st.set(a, static_cast<float>(i));
    stop = true;
  });
  float last = 0.f;
  bool monotonic = true;
  while (!stop) {
    const float v = st.get(a);
    monotonic &= v >= last;
    last = v;
  }
  writer.join();
  EXPECT_TRUE(monotonic);
  EXPECT_FLOAT_EQ(st.get(a), 200000.f);
}

// ---- the real registry -----------------------------------------------------------------------

TEST(EngineRegistry, EveryParameterIsWellFormed) {
  Engine e;
  const ParamRegistry& reg = e.registry();
  ASSERT_GT(reg.size(), 80u);
  std::set<std::string> keys;
  for (std::size_t i = 0; i < reg.size(); ++i) {
    const ParamSpec& s = reg.spec(static_cast<ParamId>(i));
    EXPECT_TRUE(keys.insert(s.key).second) << "duplicate key " << s.key;
    EXPECT_FALSE(s.label.empty()) << s.key;
    EXPECT_LE(s.min, s.def) << s.key;
    EXPECT_LE(s.def, s.max) << s.key;
    EXPECT_LT(s.min, s.max + (s.type == ParamType::Float ? 0.f : 1.f)) << s.key;
    if (s.scale == ParamScale::Log) {
      EXPECT_GT(s.min, 0.f) << s.key;
    }
    if (s.type == ParamType::Enum) {
      EXPECT_EQ(static_cast<std::size_t>(s.max) + 1, s.enumNames.size()) << s.key;
    }
    EXPECT_EQ(reg.find(s.key), static_cast<ParamId>(i)) << s.key;
  }
}

TEST(EngineRegistry, SpecsJsonIsValidAndComplete) {
  Engine e;
  const auto j = nlohmann::json::parse(e.registry().specsToJson(), nullptr, false);
  ASSERT_TRUE(j.is_array());
  EXPECT_EQ(j.size(), e.registry().size());
  EXPECT_EQ(j[0]["key"], "transport.bpm");
  bool sawEnum = false;
  for (const auto& p : j) sawEnum |= p.contains("enum");
  EXPECT_TRUE(sawEnum);
}

TEST(EngineRegistry, VoiceGroupsAreSymmetric) {
  Engine e;
  const auto& ids = e.ids().granular;
  ASSERT_EQ(ids.numVoices, 4);
  const std::string k0 = e.registry().spec(ids.voice[0].sizeMs).key;
  const std::string k3 = e.registry().spec(ids.voice[3].sizeMs).key;
  EXPECT_EQ(k0, "granular.v0.size_ms");
  EXPECT_EQ(k3, "granular.v3.size_ms");
  EXPECT_EQ(ids.voice[1].gate - ids.voice[0].gate, ids.voice[2].gate - ids.voice[1].gate);
}

// ---- serialization ------------------------------------------------------------------------------

TEST(Preset, RoundTripRestoresEveryValue) {
  Engine a;
  XorShift32 rng(99);
  for (std::size_t i = 0; i < a.registry().size(); ++i) {
    a.params().setNormalized(static_cast<ParamId>(i), rng.uniform());
  }
  const std::string json = a.exportPreset("round trip");

  Engine b;
  const LoadReport rep = b.importPreset(json);
  ASSERT_TRUE(rep.ok) << rep.error;
  EXPECT_EQ(rep.applied, static_cast<int>(a.registry().size()));
  EXPECT_TRUE(rep.unknownKeys.empty());
  EXPECT_TRUE(rep.clampedKeys.empty());
  for (std::size_t i = 0; i < a.registry().size(); ++i) {
    const auto id = static_cast<ParamId>(i);
    EXPECT_FLOAT_EQ(a.params().get(id), b.params().get(id)) << a.registry().spec(id).key;
  }
}

TEST(Preset, UnknownKeysClampingAndMissingKeys) {
  Engine e;
  const ParamId size = e.registry().find("granular.v0.size_ms");
  const ParamId dens = e.registry().find("granular.v0.density");
  const std::string doc = R"({"format":"autodj-preset","version":1,"name":"x",
      "params":{"granular.v0.size_ms":9999,"does.not.exist":1,"granular.v0.gate":true}})";
  const LoadReport rep = e.importPreset(doc);
  ASSERT_TRUE(rep.ok) << rep.error;
  EXPECT_FLOAT_EQ(e.params().get(size), 500.f) << "out of range value is clamped";
  ASSERT_EQ(rep.clampedKeys.size(), 1u);
  ASSERT_EQ(rep.unknownKeys.size(), 1u);
  EXPECT_EQ(rep.unknownKeys[0], "does.not.exist");
  EXPECT_FLOAT_EQ(e.params().get(dens), 20.f) << "keys missing from the file keep their value";
  EXPECT_TRUE(e.params().getBool(e.registry().find("granular.v0.gate")));
}

TEST(Preset, RejectsGarbageAndFutureVersions) {
  Engine e;
  EXPECT_FALSE(e.importPreset("not json at all").ok);
  EXPECT_FALSE(e.importPreset("[1,2,3]").ok);
  EXPECT_FALSE(e.importPreset(R"({"format":"something-else","version":1,"params":{}})").ok);
  EXPECT_FALSE(e.importPreset(R"({"format":"autodj-preset","params":{}})").ok) << "missing version";
  const LoadReport future = e.importPreset(R"({"format":"autodj-preset","version":999,"params":{}})");
  EXPECT_FALSE(future.ok);
  EXPECT_NE(future.error.find("newer"), std::string::npos);
}

TEST(Preset, MigrationChainUpgradesOldDocuments) {
  // Simulates a future format change: version 0 used "grain_ms" for what is now "granular.v0.size_ms".
  // The real migration table is empty while version 1 is current, so the chain is injected here
  // together with a pretend current version via a version-0 document.
  Engine e;
  std::vector<PresetMigration> chain;
  chain.push_back({0, [](nlohmann::json& doc) {
                     auto& p = doc["params"];
                     if (p.contains("grain_ms")) {
                       p["granular.v0.size_ms"] = p["grain_ms"];
                       p.erase("grain_ms");
                     }
                   }});
  const std::string old = R"({"format":"autodj-preset","version":0,"name":"old","params":{"grain_ms":42}})";
  const LoadReport rep = presetFromJson(e.registry(), e.params(), old, chain);
  ASSERT_TRUE(rep.ok) << rep.error;
  EXPECT_EQ(rep.fromVersion, 0);
  EXPECT_FLOAT_EQ(e.params().get(e.registry().find("granular.v0.size_ms")), 42.f);
  EXPECT_TRUE(rep.unknownKeys.empty());

  const LoadReport noPath = presetFromJson(e.registry(), e.params(), old, {});
  EXPECT_FALSE(noPath.ok) << "no migration registered for version 0";
}

TEST(Preset, PrefixFilterExportsOnlyOneVoice) {
  Engine e;
  const auto doc = nlohmann::json::parse(presetToJson(e.registry(), e.params(), "v1", "granular.v1."));
  ASSERT_TRUE(doc["params"].is_object());
  EXPECT_GT(doc["params"].size(), 15u);
  for (const auto& [k, v] : doc["params"].items()) EXPECT_EQ(k.rfind("granular.v1.", 0), 0u) << k;
}

TEST(ParamStore, NonFiniteInputNeverReachesTheStore) {
  ParamRegistry reg;
  ParamSpec s;
  s.key = "x";
  s.label = "X";
  s.min = 10.f;
  s.max = 1000.f;
  s.def = 50.f;
  s.scale = ParamScale::Log;
  const ParamId id = reg.add(s);
  reg.freeze();
  ParamStore st(reg);
  for (float bad : {std::nanf(""), INFINITY, -INFINITY}) {
    st.set(id, 300.f);
    st.set(id, bad);
    EXPECT_TRUE(std::isfinite(st.get(id)));
    EXPECT_GE(st.get(id), 10.f);
    EXPECT_LE(st.get(id), 1000.f);
    st.setNormalized(id, 0.5f);
    st.setNormalized(id, bad);  // used to store NaN: fromNormalized() only clamped
    EXPECT_TRUE(std::isfinite(st.get(id))) << "setNormalized(" << bad << ")";
  }
}
