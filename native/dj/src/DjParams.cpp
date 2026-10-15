#include "autodj/dj/DjParams.h"

#include <string>

namespace autodj::dj {

namespace {
ParamSpec spec(std::string key, std::string label, std::string group, std::string unit, float mn, float mx, float def, float smoothMs = 0.f) {
  ParamSpec s;
  s.key = std::move(key);
  s.label = std::move(label);
  s.group = std::move(group);
  s.unit = std::move(unit);
  s.min = mn;
  s.max = mx;
  s.def = def;
  s.smoothMs = smoothMs;
  return s;
}
ParamSpec boolSpec(std::string key, std::string label, std::string group, bool def) {
  ParamSpec s = spec(std::move(key), std::move(label), std::move(group), "", 0.f, 1.f, def ? 1.f : 0.f);
  s.type = ParamType::Bool;
  return s;
}
}  // namespace

DjParamIds registerDjParams(ParamRegistry& reg) {
  DjParamIds ids;
  for (int d = 0; d < 2; ++d) {
    const std::string p = d == 0 ? "deck.a." : "deck.b.";
    const std::string g = d == 0 ? "Deck A" : "Deck B";
    DeckParamIds& id = ids.deck[d];
    id.volume = reg.add(spec(p + "volume", "Volume", g, "", 0.f, 1.f, 1.f, 12.f));
    id.trimDb = reg.add(spec(p + "trim_db", "Trim", g, "dB", -12.f, 12.f, 0.f, 12.f));
    id.autoTrim = reg.add(boolSpec(p + "auto_trim", "Auto level", g, true));
    id.eqLow = reg.add(spec(p + "eq_low_db", "Low", g, "dB", -26.f, 6.f, 0.f, 12.f));
    id.eqMid = reg.add(spec(p + "eq_mid_db", "Mid", g, "dB", -26.f, 6.f, 0.f, 12.f));
    id.eqHigh = reg.add(spec(p + "eq_high_db", "High", g, "dB", -26.f, 6.f, 0.f, 12.f));
    id.filter = reg.add(spec(p + "filter", "Filter", g, "", -1.f, 1.f, 0.f, 12.f));
    id.tempoPct = reg.add(spec(p + "tempo_pct", "Tempo", g, "%", -16.f, 16.f, 0.f));
    id.sync = reg.add(boolSpec(p + "sync", "Sync", g, false));
    id.keylock = reg.add(boolSpec(p + "keylock", "Key lock", g, true));
  }
  ids.crossfader = reg.add(spec("mixer.crossfader", "Crossfader", "Mixer", "", -1.f, 1.f, 0.f, 8.f));
  ParamSpec c;
  c.key = "mixer.crossfader_curve";
  c.label = "Crossfader curve";
  c.group = "Mixer";
  c.type = ParamType::Enum;
  c.enumNames = {"blend", "equal power", "cut"};
  c.def = 0.f;
  ids.curve = reg.add(std::move(c));
  return ids;
}

}  // namespace autodj::dj
