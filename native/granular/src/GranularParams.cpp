#include "autodj/granular/GranularParams.h"

#include <string>

#include "autodj/core/Transport.h"

namespace autodj::granular {

namespace {

ParamSpec makeFloat(std::string key, std::string label, std::string group, std::string unit, float mn, float mx,
                    float def, ParamScale scale = ParamScale::Linear, float smoothMs = 0.f) {
  ParamSpec s;
  s.key = std::move(key);
  s.label = std::move(label);
  s.group = std::move(group);
  s.unit = std::move(unit);
  s.min = mn;
  s.max = mx;
  s.def = def;
  s.scale = scale;
  s.smoothMs = smoothMs;
  return s;
}

ParamSpec makeBool(std::string key, std::string label, std::string group, bool def) {
  ParamSpec s;
  s.key = std::move(key);
  s.label = std::move(label);
  s.group = std::move(group);
  s.type = ParamType::Bool;
  s.def = def ? 1.f : 0.f;
  return s;
}

ParamSpec makeEnum(std::string key, std::string label, std::string group, std::vector<std::string> names, int def) {
  ParamSpec s;
  s.key = std::move(key);
  s.label = std::move(label);
  s.group = std::move(group);
  s.type = ParamType::Enum;
  s.enumNames = std::move(names);
  s.def = static_cast<float>(def);
  return s;
}

}  // namespace

GranularParamIds registerGranularParams(ParamRegistry& reg, int numVoices) {
  GranularParamIds ids;
  if (numVoices < 1) numVoices = 1;
  if (numVoices > kMaxVoices) numVoices = kMaxVoices;
  ids.numVoices = numVoices;

  for (int v = 0; v < numVoices; ++v) {
    const std::string p = "granular.v" + std::to_string(v) + ".";
    const std::string g = "Granular " + std::to_string(v + 1);
    VoiceParamIds& id = ids.voice[v];

    id.gate = reg.add(makeBool(p + "gate", "Gate", g, false));

    ParamSpec src = makeFloat(p + "source", "Source", g, "", 0.f, 15.f, 0.f);
    src.type = ParamType::Int;
    id.source = reg.add(std::move(src));

    id.position = reg.add(makeFloat(p + "position", "Position", g, "", 0.f, 1.f, 0.5f));
    id.sizeMs = reg.add(makeFloat(p + "size_ms", "Grain size", g, "ms", 5.f, 500.f, 80.f, ParamScale::Log, 20.f));
    id.density = reg.add(makeFloat(p + "density", "Density", g, "grains/s", 0.5f, 2000.f, 20.f, ParamScale::Log, 20.f));
    id.sprayMs = reg.add(makeFloat(p + "spray_ms", "Spray", g, "ms", 0.f, 2000.f, 0.f, ParamScale::Linear, 20.f));
    id.jitter = reg.add(makeFloat(p + "jitter", "Timing jitter", g, "", 0.f, 1.f, 0.f));
    id.pitchSt = reg.add(makeFloat(p + "pitch_st", "Pitch", g, "st", -24.f, 24.f, 0.f, ParamScale::Linear, 15.f));
    id.pitchRandSt = reg.add(makeFloat(p + "pitch_rand_st", "Pitch random", g, "st", 0.f, 12.f, 0.f));
    id.envShape = reg.add(makeEnum(p + "env_shape", "Envelope", g, {"Hann", "Gauss", "Tukey", "Trapezoid", "Square", "Triangle"}, 0));
    id.reverseProb = reg.add(makeFloat(p + "reverse_prob", "Reverse", g, "%", 0.f, 1.f, 0.f));
    id.spread = reg.add(makeFloat(p + "spread", "Stereo spread", g, "", 0.f, 1.f, 0.f, ParamScale::Linear, 20.f));
    id.ampRand = reg.add(makeFloat(p + "amp_rand", "Amp random", g, "", 0.f, 1.f, 0.f));
    id.scanOn = reg.add(makeBool(p + "scan_on", "Scan", g, false));
    id.scanSpeed = reg.add(makeFloat(p + "scan_speed", "Scan speed", g, "x", -4.f, 4.f, 1.f));
    id.loopStart = reg.add(makeFloat(p + "loop_start", "Loop start", g, "", 0.f, 1.f, 0.f));
    id.loopEnd = reg.add(makeFloat(p + "loop_end", "Loop end", g, "", 0.f, 1.f, 1.f));
    id.freeze = reg.add(makeBool(p + "freeze", "Freeze", g, false));
    id.syncMode = reg.add(makeEnum(p + "sync_mode", "Trigger", g, {"Free", "Beat sync"}, 0));
    id.syncDiv = reg.add(makeEnum(p + "sync_div", "Sync division", g,
                                   {"1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T"},
                                   static_cast<int>(SyncDivision::Sixteenth)));
    id.quality = reg.add(makeEnum(p + "quality", "Interpolation", g, {"Linear", "Hermite"}, 0));
    id.volume = reg.add(makeFloat(p + "volume", "Volume", g, "", 0.f, 2.f, 0.8f, ParamScale::Linear, 20.f));
    id.releaseMs = reg.add(makeFloat(p + "release_ms", "Release", g, "ms", 5.f, 2000.f, 120.f, ParamScale::Log));
    id.autoGain = reg.add(makeBool(p + "auto_gain", "Auto gain", g, true));

    id.cellMode = reg.add(makeEnum(p + "cell_mode", "Mode", g, {"Hold", "Step", "Keys"}, static_cast<int>(CellMode::Hold)));
    id.cellCount = reg.add(makeEnum(p + "cell_count", "Cells", g, {"8", "16"}, 0));
    ParamSpec ci = makeFloat(p + "cell_index", "Cell", g, "", 0.f, static_cast<float>(kMaxCells - 1), 0.f);
    ci.type = ParamType::Int;
    id.cellIndex = reg.add(std::move(ci));
    id.stepDiv = reg.add(makeEnum(p + "step_div", "Step length", g, {"2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16"}, 3));
    id.seqOn = reg.add(makeBool(p + "seq_on", "Pattern", g, false));
  }

  ids.adaptive = reg.add(makeBool("granular.adaptive", "Adaptive quality", "Granular", true));
  ids.stealFadeMs = reg.add(makeFloat("granular.steal_fade_ms", "Steal fade", "Granular", "ms", 1.f, 10.f, 4.f));
  ParamSpec mv = makeFloat("granular.max_voices", "Max voices", "Granular", "", 1.f, static_cast<float>(numVoices),
                           static_cast<float>(numVoices));
  mv.type = ParamType::Int;
  ids.maxVoices = reg.add(std::move(mv));
  ParamSpec gc = makeFloat("granular.grain_cap", "Grain cap", "Granular", "", 16.f, 512.f, 512.f);
  gc.type = ParamType::Int;
  ids.grainCap = reg.add(std::move(gc));
  ids.chaos = reg.add(makeFloat("granular.chaos", "Chaos", "Granular", "", 0.f, 1.f, 0.f, ParamScale::Linear, 40.f));
  ids.outputDb = reg.add(makeFloat("granular.output_db", "Output", "Granular", "dB", -24.f, 6.f, 0.f, ParamScale::Linear, 20.f));
  return ids;
}

}  // namespace autodj::granular
