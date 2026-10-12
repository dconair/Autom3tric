// Host benchmark of the granular engine.
//   bench_granular [--seconds 8] [--repeats 3] [--grains 64,128,256,512] [--callbacks 128,192]
//                  [--interp linear|hermite|both] [--kernel scalar|simd|both] [--chaos 0..1] [--json out.json] [--md out.md] [--quick]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "BenchCore.h"

namespace {

std::vector<int> parseList(const std::string& s) {
  std::vector<int> v;
  std::stringstream ss(s);
  std::string tok;
  while (std::getline(ss, tok, ',')) v.push_back(std::atoi(tok.c_str()));
  return v;
}

void progress(void*, int done, int total) {
  std::fprintf(stderr, "\r  %d / %d runs", done, total);
  if (done == total) std::fprintf(stderr, "\n");
}

}  // namespace

int main(int argc, char** argv) {
  autodj::bench::Config cfg;
  std::string jsonPath, mdPath;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--seconds") cfg.seconds = std::atof(next().c_str());
    else if (a == "--repeats") cfg.repeats = std::atoi(next().c_str());
    else if (a == "--grains") cfg.grains = parseList(next());
    else if (a == "--callbacks") cfg.callbackFrames = parseList(next());
    else if (a == "--interp") {
      const std::string v = next();
      cfg.linear = v != "hermite";
      cfg.hermite = v != "linear";
    } else if (a == "--kernel") {
      const std::string v = next();
      cfg.simd = v != "scalar";
      cfg.scalar = v != "simd";
    } else if (a == "--json") jsonPath = next();
    else if (a == "--md") mdPath = next();
    else if (a == "--chaos") cfg.chaos = static_cast<float>(std::atof(next().c_str()));
    else if (a == "--quick") { cfg.seconds = 2.0; cfg.warmupSeconds = 0.5; cfg.repeats = 1; }
    else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
  }
  const auto env = autodj::bench::environment();
  const auto results = autodj::bench::run(cfg, &progress, nullptr);
  const std::string md = autodj::bench::toMarkdown(env, cfg, results);
  std::fputs(md.c_str(), stdout);
  if (!jsonPath.empty()) std::ofstream(jsonPath) << autodj::bench::toJson(env, cfg, results);
  if (!mdPath.empty()) std::ofstream(mdPath) << md;
  return 0;
}
