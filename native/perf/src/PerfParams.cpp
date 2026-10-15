#include "autodj/perf/PerfParams.h"

namespace autodj::perf {

namespace {
ParamSpec fl(const char* key, const char* label, const char* group, float mn, float mx, float def, float smoothMs = 0.f) {
  ParamSpec s;
  s.key = key;
  s.label = label;
  s.group = group;
  s.min = mn;
  s.max = mx;
  s.def = def;
  s.smoothMs = smoothMs;
  return s;
}
ParamSpec flag(const char* key, const char* label, const char* group, bool def = false) {
  ParamSpec s = fl(key, label, group, 0.f, 1.f, def ? 1.f : 0.f);
  s.type = ParamType::Bool;
  return s;
}
ParamSpec choice(const char* key, const char* label, const char* group, std::vector<std::string> names, int def) {
  ParamSpec s = fl(key, label, group, 0.f, static_cast<float>(names.size() - 1), static_cast<float>(def));
  s.type = ParamType::Enum;
  s.enumNames = std::move(names);
  return s;
}
}  // namespace

PerfParamIds registerPerfParams(ParamRegistry& reg) {
  PerfParamIds p;
  p.echoOn = reg.add(flag("fx.echo.on", "Echo", "Effects"));
  p.echoBeats = reg.add(choice("fx.echo.time", "Echo time", "Effects", {"1/8", "1/4", "1/2", "3/4", "1"}, 1));
  p.echoFeedback = reg.add(fl("fx.echo.feedback", "Echo feedback", "Effects", 0.f, 0.92f, 0.5f));
  p.echoMix = reg.add(fl("fx.echo.mix", "Echo level", "Effects", 0.f, 1.f, 0.5f));
  p.reverbOn = reg.add(flag("fx.reverb.on", "Reverb", "Effects"));
  p.reverbMix = reg.add(fl("fx.reverb.mix", "Reverb level", "Effects", 0.f, 1.f, 0.4f));
  p.flangerOn = reg.add(flag("fx.flanger.on", "Flanger", "Effects"));
  p.flangerMix = reg.add(fl("fx.flanger.mix", "Flanger level", "Effects", 0.f, 1.f, 0.6f));
  p.rollOn = reg.add(flag("fx.roll.on", "Beat roll", "Effects"));
  p.rollBeats = reg.add(choice("fx.roll.size", "Roll size", "Effects", {"1", "1/2", "1/4", "1/8"}, 1));
  p.stutterOn = reg.add(flag("fx.stutter.on", "Stutter", "Effects"));
  p.stutterBeats = reg.add(choice("fx.stutter.rate", "Stutter rate", "Effects", {"1/4", "1/8", "1/16"}, 1));
  p.padVolume = reg.add(fl("pads.volume", "Pad volume", "Pads", 0.f, 1.5f, 0.5f, 12.f));
  p.padSeqOn = reg.add(flag("pads.seq_on", "Pad sequencer", "Pads"));
  p.padSwing = reg.add(fl("pads.swing", "Swing", "Pads", 0.f, 0.6f, 0.f));
  return p;
}

}  // namespace autodj::perf
