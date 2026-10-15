// Sound audit: renders what a user hears in the first minutes (granular demo sources, the demo songs through the auto mix,
// the drum pads, the effects) with the same engine the apps run, and measures what makes a sound "unclean": level and
// soft-clip engagement, hard clipping, clicks (outliers of the second difference), DC offset.
//
//   audio_audit [--out-dir dir] [--strict]
//
// Prints one line per scene. With --strict a scene that clips, clicks or carries DC makes the exit code non-zero.
// With --out-dir every scene is also written as a 16-bit WAV, for listening or for a look at the spectrogram.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include "autodj/core/DemoSources.h"
#include "autodj/engine/Engine.h"
#include "autodj/engine/Showcase.h"

using namespace autodj;

namespace {

constexpr int kSr = 48000;

struct Stats {
  double peak = 0, rmsDb = -200, dc = 0;
  double fracAboveKnee = 0;  // share of samples with |x| > 0.8 (the soft clipper bends them)
  int hardClip = 0;          // samples with |x| >= 0.995
  int clicks = 0;            // sharp discontinuities, see countClicks()
  double worstClick = 0;
};

/// A click is a second difference far above its surroundings: |x[n] - 2 x[n-1] + x[n-2]| > 0.12 and more than 14 times
/// the rms of the second difference around it (+-5 ms). Smooth music, even loud kicks, stays far below that.
void countClicks(const std::vector<float>& mono, Stats& s) {
  const std::size_t n = mono.size();
  if (n < 1000) return;
  std::vector<float> d2(n, 0.f);
  for (std::size_t i = 2; i < n; ++i) d2[i] = mono[i] - 2.f * mono[i - 1] + mono[i - 2];
  const std::size_t w = 240;
  double acc = 0;
  for (std::size_t i = 0; i < std::min<std::size_t>(2 * w, n); ++i) acc += static_cast<double>(d2[i]) * d2[i];
  std::size_t lastClick = 0;
  for (std::size_t i = w; i + w < n; ++i) {
    // sliding energy of d2 in [i-w, i+w)
    if (i - w > 0) acc -= static_cast<double>(d2[i - w - 1]) * d2[i - w - 1];
    acc += static_cast<double>(d2[i + w - 1]) * d2[i + w - 1];
    const double rms = std::sqrt(std::max(acc, 0.0) / (2.0 * w));
    const double a = std::fabs(d2[i]);
    if (a > 0.12 && a > 14.0 * rms && i > lastClick + 96) {
      ++s.clicks;
      s.worstClick = std::max(s.worstClick, a);
      lastClick = i;
    }
  }
}

Stats analyse(const std::vector<float>& lr, double skipSeconds = 0.5) {
  Stats s;
  const std::size_t from = static_cast<std::size_t>(skipSeconds * kSr);
  std::vector<float> mono;
  double e = 0, sum = 0;
  std::size_t above = 0, count = 0;
  for (std::size_t i = from; i * 2 + 1 < lr.size(); ++i) {
    const float l = lr[2 * i], r = lr[2 * i + 1];
    mono.push_back(0.5f * (l + r));
    for (float v : {l, r}) {
      const double a = std::fabs(v);
      s.peak = std::max(s.peak, a);
      if (a > 0.8) ++above;
      if (a >= 0.995) ++s.hardClip;
      e += static_cast<double>(v) * v;
      sum += v;
      ++count;
    }
  }
  if (count > 0) {
    s.rmsDb = 10.0 * std::log10(std::max(e / count, 1e-20));
    s.dc = sum / count;
    s.fracAboveKnee = static_cast<double>(above) / count;
  }
  countClicks(mono, s);
  return s;
}

void writeWav(const std::string& path, const std::vector<float>& lr) {
  std::ofstream f(path, std::ios::binary);
  const std::uint32_t frames = static_cast<std::uint32_t>(lr.size() / 2), bytes = frames * 4;
  auto w32 = [&](std::uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
  auto w16 = [&](std::uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
  f.write("RIFF", 4);
  w32(36 + bytes);
  f.write("WAVEfmt ", 8);
  w32(16);
  w16(1);
  w16(2);
  w32(kSr);
  w32(kSr * 4);
  w16(4);
  w16(16);
  f.write("data", 4);
  w32(bytes);
  for (float v : lr) {
    const float c = std::max(-1.f, std::min(1.f, v));
    w16(static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lrint(c * 32767.f))));
  }
}

void set(Engine& e, const std::string& key, float v) {
  const ParamId id = e.registry().find(key);
  if (id == kInvalidParam) {
    std::fprintf(stderr, "warning: unknown parameter %s\n", key.c_str());
    return;
  }
  e.params().set(id, v);
}

std::vector<float> render(Engine& e, double seconds, const std::function<void(double)>& each = {}) {
  const int block = 480;
  std::vector<float> out;
  std::vector<float> buf(static_cast<std::size_t>(block) * 2);
  const int blocks = static_cast<int>(seconds * kSr / block);
  out.reserve(static_cast<std::size_t>(blocks) * block * 2);
  for (int b = 0; b < blocks; ++b) {
    if (each) each(static_cast<double>(b) * block / kSr);
    e.process(buf.data(), block);
    out.insert(out.end(), buf.begin(), buf.end());
  }
  return out;
}

EngineConfig offline() {
  EngineConfig c;
  c.sampleRate = kSr;
  c.realtime = false;
  return c;
}

/// The desktop app's start-up state for the instrument (DesktopHost.init).
void instrumentDefaults(Engine& e) {
  for (int k = 0; k < 4; ++k) e.setSource(k, showcase::make(k, kSr));
  for (int v = 0; v < 4; ++v) set(e, "granular.v" + std::to_string(v) + ".volume", 1.6f);
  set(e, "granular.v0.gate", 1);
  set(e, "granular.v0.density", 40);
  set(e, "granular.v0.size_ms", 160);
  set(e, "granular.v0.spray_ms", 300);
  set(e, "granular.v0.spread", 0.8f);
}

std::shared_ptr<Track> demoSong(int kind) {
  struct Demo { const char* name; double bpm; int root; bool minor; unsigned seed; };
  static const Demo demos[4] = {{"Demo 1", 118.0, 57, true, 1}, {"Demo 2", 122.0, 52, true, 2}, {"Demo 3", 126.0, 60, false, 3}, {"Demo 4", 124.0, 50, true, 4}};
  const Demo& d = demos[kind % 4];
  demo::MusicSpec spec;
  spec.bpm = d.bpm;
  spec.rootMidi = d.root;
  spec.minor = d.minor;
  spec.seed = d.seed;
  spec.sampleRate = kSr;
  spec.name = d.name;
  auto t = std::make_shared<Track>();
  t->audio = demo::musicTrack(spec);
  t->name = d.name;
  return t;
}

struct Scene {
  std::string name;
  std::function<std::vector<float>()> run;
};

}  // namespace

int main(int argc, char** argv) {
  std::string outDir;
  bool strict = false;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--out-dir") && i + 1 < argc) outDir = argv[++i];
    else if (!std::strcmp(argv[i], "--strict")) strict = true;
  }

  std::vector<Scene> scenes;
  const char* srcNames[4] = {"pad", "drums", "pluck", "bass"};
  for (int src = 0; src < 4; ++src) {
    scenes.push_back({std::string("granular-") + srcNames[src] + " (app defaults)", [src] {
                        Engine e(offline());
                        instrumentDefaults(e);
                        set(e, "granular.v0.source", static_cast<float>(src));
                        e.noteOn(0, 0.f, 1.f);
                        return render(e, 10.0);
                      }});
  }
  scenes.push_back({"granular-drums step mode 1/16", [] {
                      Engine e(offline());
                      instrumentDefaults(e);
                      set(e, "granular.v0.source", 1);
                      set(e, "granular.v0.cell_mode", 1);
                      set(e, "granular.v0.cell_count", 0);
                      set(e, "transport.bpm", 120);
                      e.noteOn(0, 0.f, 1.f);
                      return render(e, 10.0);
                    }});
  scenes.push_back({"granular-pad, 4 voices at once", [] {
                      Engine e(offline());
                      instrumentDefaults(e);
                      for (int v = 0; v < 4; ++v) {
                        set(e, "granular.v" + std::to_string(v) + ".source", 0);
                        set(e, "granular.v" + std::to_string(v) + ".gate", 1);
                        set(e, "granular.v" + std::to_string(v) + ".density", 40);
                        set(e, "granular.v" + std::to_string(v) + ".size_ms", 160);
                        set(e, "granular.v" + std::to_string(v) + ".position", 0.2f + 0.2f * v);
                        e.noteOn(v, 0.f, 1.f);
                      }
                      return render(e, 10.0);
                    }});
  scenes.push_back({"granular-song, position sweep (touch + drag)", [] {
                      Engine e(offline());
                      instrumentDefaults(e);
                      e.setSource(4, demoSong(2)->audio);
                      set(e, "granular.v0.source", 4);
                      e.noteOn(0, 0.f, 1.f);
                      return render(e, 12.0, [&e](double t) { set(e, "granular.v0.position", static_cast<float>(t / 12.0)); });
                    }});
  scenes.push_back({"granular-song, keys mode, notes on/off", [] {
                      Engine e(offline());
                      instrumentDefaults(e);
                      e.setSource(4, demoSong(2)->audio);
                      set(e, "granular.v0.source", 4);
                      set(e, "granular.v0.cell_mode", 2);
                      return render(e, 12.0, [&e](double t) {
                        static int last = -1;
                        const int n = static_cast<int>(t / 0.75);
                        if (n != last) {
                          last = n;
                          if (n % 2 == 0) {
                            set(e, "granular.v0.cell_index", static_cast<float>((n / 2) % 8));
                            e.noteOn(0, 0.f, 1.f);
                          } else {
                            e.noteOff(0);
                          }
                        }
                      });
                    }});
  scenes.push_back({"granular-song, step mode 1/8", [] {
                      Engine e(offline());
                      instrumentDefaults(e);
                      e.setSource(4, demoSong(2)->audio);
                      set(e, "granular.v0.source", 4);
                      set(e, "granular.v0.cell_mode", 1);
                      set(e, "granular.v0.step_div", 2);
                      set(e, "granular.v0.gate", 0);
                      set(e, "transport.bpm", 126);
                      e.noteOn(0, 0.f, 1.f);
                      return render(e, 12.0);
                    }});
  for (int kind = 0; kind < 4; ++kind) {
    scenes.push_back({"deck, demo song " + std::to_string(kind + 1), [kind] {
                        Engine e(offline());
                        e.deckLoad(0, demoSong(kind));
                        e.deckPlay(0, true);
                        return render(e, 20.0);
                      }});
  }
  scenes.push_back({"drum pads: house pattern, sequencer on", [] {
                      Engine e(offline());
                      set(e, "pads.seq_on", 1);
                      return render(e, 10.0);
                    }});
  scenes.push_back({"drum pads: every pad once, one second apart", [] {
                      Engine e(offline());
                      return render(e, 17.0, [&e](double t) {
                        static int last = -1;
                        const int pad = static_cast<int>(t);
                        if (pad != last && pad < 16) {
                          e.padTrigger(pad, 1.f);
                          last = pad;
                        }
                      });
                    }});
  scenes.push_back({"deck + pads + echo + reverb", [] {
                      Engine e(offline());
                      e.deckLoad(0, demoSong(0));
                      e.deckPlay(0, true);
                      set(e, "pads.seq_on", 1);
                      set(e, "fx.echo.on", 1);
                      set(e, "fx.reverb.on", 1);
                      return render(e, 20.0);
                    }});

  if (!outDir.empty()) {  // the raw demo song, to compare the deck output against
    const auto src = demoSong(0);
    std::vector<float> lr(src->audio->frames() * 2);
    std::memcpy(lr.data(), src->audio->data(), lr.size() * sizeof(float));
    writeWav(outDir + "/source_demo_song_1.wav", lr);
  }
  int bad = 0;
  std::printf("%-46s %7s %8s %8s %6s %6s %7s\n", "scene", "peak", "rms dB", "> 0.8", "clip", "clicks", "dc");
  for (const Scene& s : scenes) {
    const std::vector<float> audio = s.run();
    const Stats st = analyse(audio);
    const bool flagged = st.hardClip > 0 || st.clicks > 0 || std::fabs(st.dc) > 0.01;
    std::printf("%-46s %7.3f %8.1f %7.2f%% %6d %6d %7.4f%s\n", s.name.c_str(), st.peak, st.rmsDb, 100.0 * st.fracAboveKnee, st.hardClip, st.clicks,
                st.dc, flagged ? "   <--" : "");
    if (flagged) ++bad;
    if (!outDir.empty()) {
      std::string f = s.name;
      for (char& c : f) if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
      writeWav(outDir + "/" + f + ".wav", audio);
    }
  }
  if (bad > 0) std::printf("%d scene(s) flagged\n", bad);
  return strict && bad > 0 ? 1 : 0;
}
