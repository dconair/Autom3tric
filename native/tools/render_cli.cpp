// Offline renderer: the same Engine::process() the Oboe callback calls, driven from a loop and written to
// a WAV file. Used for the listening demos, golden-file generation and "export == live" checks.
//
//   render_cli --scene pad|shimmer|drums|texture|mix|steps|keys --seconds 20 --out demo.wav
//              [--callback 192] [--preset in.json] [--save-preset out.json] [--dump-params specs.json]
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "autodj/core/DemoSources.h"
#include "autodj/engine/Engine.h"

using namespace autodj;

namespace {

void set(Engine& e, const std::string& key, float v) {
  const ParamId id = e.registry().find(key);
  if (id == kInvalidParam) {
    std::fprintf(stderr, "warning: unknown parameter %s\n", key.c_str());
    return;
  }
  e.params().set(id, v);
}

std::string vk(int v, const char* name) { return "granular.v" + std::to_string(v) + "." + name; }

float lerp(float a, float b, float t) { return a + (b - a) * std::fmin(1.f, std::fmax(0.f, t)); }

struct Scene {
  const char* name;
  const char* what;
  std::function<void(Engine&)> setup;
  std::function<void(Engine&, double)> automate;  // called once per callback with the time in seconds
};

void loadSources(Engine& e, double bpm) {
  e.setSource(0, demo::chordPad(12.0, e.sampleRate()));
  e.setSource(1, demo::drumLoop(bpm, 4, e.sampleRate()));
  e.setSource(2, demo::noise(4.0, e.sampleRate(), 0.5f));
}

std::vector<Scene> scenes() {
  std::vector<Scene> s;

  s.push_back({"pad", "Slowly drifting tonal cloud from a chord pad (Hermite, wide stereo)",
               [](Engine& e) {
                 loadSources(e, 120);
                 set(e, "master.gain", 0.9f);
                 set(e, vk(0, "gate"), 1);
                 set(e, vk(0, "source"), 0);
                 set(e, vk(0, "size_ms"), 180);
                 set(e, vk(0, "density"), 55);
                 set(e, vk(0, "spray_ms"), 500);
                 set(e, vk(0, "jitter"), 0.35f);
                 set(e, vk(0, "pitch_rand_st"), 0.12f);
                 set(e, vk(0, "spread"), 0.95f);
                 set(e, vk(0, "quality"), 1);
                 set(e, vk(0, "scan_on"), 1);
                 set(e, vk(0, "scan_speed"), 0.15f);
               },
               [](Engine& e, double t) {
                 set(e, vk(0, "spray_ms"), 100.f + 700.f * (0.5f + 0.5f * std::sin(static_cast<float>(t) * 0.35f)));
               }});

  s.push_back({"shimmer", "Pad with an octave-up and a fifth-up layer, density breathing",
               [](Engine& e) {
                 loadSources(e, 120);
                 set(e, "master.gain", 0.85f);
                 const float pitch[3] = {0.f, 12.f, 7.f};
                 const float vol[3] = {0.8f, 0.45f, 0.35f};
                 for (int v = 0; v < 3; ++v) {
                   set(e, vk(v, "gate"), 1);
                   set(e, vk(v, "source"), 0);
                   set(e, vk(v, "position"), 0.25f + 0.2f * static_cast<float>(v));
                   set(e, vk(v, "size_ms"), 140.f + 60.f * static_cast<float>(v));
                   set(e, vk(v, "density"), 40.f);
                   set(e, vk(v, "spray_ms"), 350);
                   set(e, vk(v, "jitter"), 0.4f);
                   set(e, vk(v, "pitch_st"), pitch[v]);
                   set(e, vk(v, "volume"), vol[v]);
                   set(e, vk(v, "spread"), 1.f);
                   set(e, vk(v, "env_shape"), static_cast<float>(1 + v % 2));  // Gauss / Tukey
                   set(e, vk(v, "quality"), 1);
                 }
               },
               [](Engine& e, double t) {
                 for (int v = 0; v < 3; ++v)
                   set(e, vk(v, "density"), 30.f + 50.f * (0.5f + 0.5f * std::sin(static_cast<float>(t) * (0.4f + 0.13f * static_cast<float>(v)))));
               }});

  s.push_back({"drums", "Beat-synced drum slicer: 1/16 -> 1/16T -> 1/32, reverse grains from 12 s",
               [](Engine& e) {
                 loadSources(e, 124);
                 set(e, "transport.bpm", 124);
                 set(e, "master.gain", 0.9f);
                 set(e, vk(0, "gate"), 1);
                 set(e, vk(0, "source"), 1);
                 set(e, vk(0, "sync_mode"), 1);
                 set(e, vk(0, "sync_div"), 2);
                 set(e, vk(0, "size_ms"), 70);
                 set(e, vk(0, "scan_on"), 1);
                 set(e, vk(0, "scan_speed"), 1.f);
                 set(e, vk(0, "env_shape"), 2);
                 set(e, vk(0, "spread"), 0.4f);
               },
               [](Engine& e, double t) {
                 set(e, vk(0, "sync_div"), t < 6 ? 2.f : (t < 10 ? 6.f : 3.f));
                 set(e, vk(0, "reverse_prob"), t < 12 ? 0.f : 0.25f);
                 set(e, vk(0, "pitch_rand_st"), t < 14 ? 0.f : 3.f);
               }});

  s.push_back({"texture", "Noise rain: sparse -> dense, pitch scatter",
               [](Engine& e) {
                 loadSources(e, 120);
                 set(e, "master.gain", 0.7f);
                 set(e, vk(0, "gate"), 1);
                 set(e, vk(0, "source"), 2);
                 set(e, vk(0, "size_ms"), 12);
                 set(e, vk(0, "density"), 20);
                 set(e, vk(0, "spray_ms"), 1500);
                 set(e, vk(0, "pitch_rand_st"), 7);
                 set(e, vk(0, "spread"), 1);
                 set(e, vk(0, "amp_rand"), 0.6f);
                 set(e, vk(0, "env_shape"), 1);
               },
               [](Engine& e, double t) {
                 set(e, vk(0, "density"), 20.f * std::pow(60.f, static_cast<float>(std::fmin(t / 20.0, 1.0))));
               }});

  s.push_back({"mix", "Everything at once: pad cloud + drum slicer + noise texture, with a freeze section",
               [](Engine& e) {
                 loadSources(e, 120);
                 set(e, "master.gain", 0.8f);
                 set(e, vk(0, "gate"), 1); set(e, vk(0, "source"), 0); set(e, vk(0, "size_ms"), 200);
                 set(e, vk(0, "density"), 50); set(e, vk(0, "spray_ms"), 400); set(e, vk(0, "jitter"), 0.3f);
                 set(e, vk(0, "spread"), 0.9f); set(e, vk(0, "quality"), 1); set(e, vk(0, "volume"), 0.7f);
                 set(e, vk(1, "gate"), 1); set(e, vk(1, "source"), 1); set(e, vk(1, "sync_mode"), 1);
                 set(e, vk(1, "sync_div"), 2); set(e, vk(1, "size_ms"), 60); set(e, vk(1, "scan_on"), 1);
                 set(e, vk(1, "env_shape"), 2); set(e, vk(1, "volume"), 0.8f);
                 set(e, vk(2, "gate"), 1); set(e, vk(2, "source"), 2); set(e, vk(2, "size_ms"), 15);
                 set(e, vk(2, "density"), 30); set(e, vk(2, "spray_ms"), 1000); set(e, vk(2, "pitch_rand_st"), 5);
                 set(e, vk(2, "volume"), 0.25f); set(e, vk(2, "spread"), 1);
               },
               [](Engine& e, double t) {
                 set(e, vk(0, "freeze"), (t > 10 && t < 14) ? 1.f : 0.f);
                 set(e, vk(0, "scan_on"), t > 4 ? 1.f : 0.f);
                 set(e, vk(0, "scan_speed"), 0.2f);
                 set(e, vk(2, "density"), 30.f + 250.f * lerp(0.f, 1.f, static_cast<float>((t - 6.0) / 8.0)));
                 set(e, vk(1, "sync_div"), t < 8 ? 2.f : 3.f);
               }});

  s.push_back({"steps", "Instrument, Step mode: 16 cells of the drum loop follow the clock (1/8 -> 1/16), chaos rises at the end",
               [](Engine& e) {
                 loadSources(e, 124);
                 set(e, "transport.bpm", 124);
                 set(e, "master.gain", 0.9f);
                 set(e, vk(0, "gate"), 1);
                 set(e, vk(0, "source"), 1);
                 set(e, vk(0, "cell_mode"), 1);   // Step
                 set(e, vk(0, "cell_count"), 1);  // 16 cells
                 set(e, vk(0, "step_div"), 4);    // 1/8
                 set(e, vk(0, "position"), 0.05f);
                 set(e, vk(0, "sync_mode"), 1);
                 set(e, vk(0, "sync_div"), 2);    // 1/16 grains
                 set(e, vk(0, "size_ms"), 85);
                 set(e, vk(0, "env_shape"), 2);   // Tukey
                 set(e, vk(0, "spread"), 0.5f);
               },
               [](Engine& e, double t) {
                 set(e, vk(0, "step_div"), t < 8 ? 4.f : 5.f);  // 1/8, then 1/16
                 set(e, vk(0, "position"), t < 14 ? 0.05f : 0.3f);  // just after the cell start (kick/snare), later further in
                 set(e, "granular.chaos", 0.65f * lerp(0.f, 1.f, static_cast<float>((t - 12.0) / 6.0)));
               }});

  s.push_back({"keys", "Instrument, Keys mode: a melody played on the 8 cells of the pad (note on/off per cell), chaos from 8 s",
               [](Engine& e) {
                 loadSources(e, 120);
                 set(e, "master.gain", 0.9f);
                 set(e, vk(0, "gate"), 0);        // notes decide when it sounds
                 set(e, vk(0, "source"), 0);
                 set(e, vk(0, "cell_mode"), 2);   // Keys
                 set(e, vk(0, "cell_count"), 0);  // 8 cells
                 set(e, vk(0, "density"), 48);
                 set(e, vk(0, "size_ms"), 170);
                 set(e, vk(0, "jitter"), 0.25f);
                 set(e, vk(0, "spread"), 0.9f);
                 set(e, vk(0, "quality"), 1);
               },
               [last = -1](Engine& e, double t) mutable {
                 static const int melody[8] = {0, 2, 4, 7, 5, 4, 2, 6};
                 const int step = static_cast<int>(t / 0.6);
                 if (step != last) {
                   last = step;
                   e.noteOff(0);
                   set(e, vk(0, "cell_index"), static_cast<float>(melody[step % 8]));
                   e.noteOn(0);
                 }
                 set(e, "granular.chaos", 0.6f * lerp(0.f, 1.f, static_cast<float>((t - 8.0) / 6.0)));
               }});
  return s;
}

bool writeWav(const std::string& path, const std::vector<float>& stereo, int sampleRate) {
  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  const std::uint32_t dataBytes = static_cast<std::uint32_t>(stereo.size() * 2);
  auto w32 = [&](std::uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
  auto w16 = [&](std::uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
  f.write("RIFF", 4); w32(36 + dataBytes); f.write("WAVEfmt ", 8); w32(16); w16(1); w16(2);
  w32(static_cast<std::uint32_t>(sampleRate)); w32(static_cast<std::uint32_t>(sampleRate) * 4); w16(4); w16(16);
  f.write("data", 4); w32(dataBytes);
  std::uint32_t rng = 12345;  // TPDF dither, +-1 LSB
  auto r = [&]() { rng = rng * 1664525u + 1013904223u; return static_cast<float>(rng >> 8) * (1.f / 16777216.f); };
  for (float x : stereo) {
    const float d = (r() - r()) / 32768.f;
    const float v = std::fmax(-1.f, std::fmin(1.f, x + d));
    const std::int16_t s = static_cast<std::int16_t>(std::lrintf(v * 32767.f));
    f.write(reinterpret_cast<const char*>(&s), 2);
  }
  return static_cast<bool>(f);
}

std::string readFile(const std::string& path) {
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

}  // namespace

int main(int argc, char** argv) {
  std::string scene = "pad", out = "render.wav", preset, savePreset, dumpParams;
  double seconds = 20.0;
  int callback = 192;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--scene") scene = next();
    else if (a == "--seconds") seconds = std::atof(next().c_str());
    else if (a == "--out") out = next();
    else if (a == "--callback") callback = std::atoi(next().c_str());
    else if (a == "--preset") preset = next();
    else if (a == "--save-preset") savePreset = next();
    else if (a == "--dump-params") dumpParams = next();
    else if (a == "--list-scenes") {
      for (const Scene& s : scenes()) std::printf("%-8s %s\n", s.name, s.what);
      return 0;
    } else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
  }

  EngineConfig cfg;
  cfg.realtime = false;  // export mode: deterministic, no adaptive quality
  Engine e(cfg);
  if (!dumpParams.empty()) std::ofstream(dumpParams) << e.registry().specsToJson();

  const auto all = scenes();
  const Scene* sc = nullptr;
  for (const Scene& s : all) if (scene == s.name) sc = &s;
  if (!sc) { std::fprintf(stderr, "unknown scene '%s' (try --list-scenes)\n", scene.c_str()); return 2; }
  sc->setup(e);
  if (!preset.empty()) {
    const LoadReport rep = e.importPreset(readFile(preset));
    std::fprintf(stderr, "preset: %s (v%d, %d applied, %zu unknown, %zu clamped)\n", rep.ok ? "ok" : rep.error.c_str(),
                 rep.fromVersion, rep.applied, rep.unknownKeys.size(), rep.clampedKeys.size());
  }
  if (!savePreset.empty()) std::ofstream(savePreset) << e.exportPreset(sc->name);

  e.play();
  const int total = static_cast<int>(seconds * cfg.sampleRate);
  std::vector<float> pcm(static_cast<std::size_t>(total) * 2);
  const auto t0 = std::chrono::steady_clock::now();
  for (int done = 0; done < total; done += callback) {
    const int n = std::min(callback, total - done);
    sc->automate(e, static_cast<double>(done) / cfg.sampleRate);
    e.process(pcm.data() + static_cast<std::size_t>(done) * 2, n);
    if ((done / callback) % 64 == 0) e.collectGarbage();
  }
  const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

  double peak = 0.0, sq = 0.0;
  for (float x : pcm) { peak = std::fmax(peak, std::fabs(x)); sq += static_cast<double>(x) * x; }
  if (!writeWav(out, pcm, cfg.sampleRate)) { std::fprintf(stderr, "cannot write %s\n", out.c_str()); return 1; }
  std::printf("%s: %.1f s, peak %.3f, rms %.3f, rendered in %.2f s (%.0fx real time, %s kernels) -> %s\n", sc->name,
              seconds, peak, std::sqrt(sq / static_cast<double>(pcm.size())), wall, seconds / wall,
              granular::kernelBackendName(), out.c_str());
  return 0;
}
