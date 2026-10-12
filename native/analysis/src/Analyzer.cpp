#include "autodj/analysis/Analysis.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>

#include "autodj/analysis/Fft.h"

namespace autodj::analysis {

namespace {

constexpr double kAnalysisRate = 12000.0;  // decimated rate used for tempo / key (content above 6 kHz is irrelevant there)
constexpr int kOnsetN = 512;
constexpr int kOnsetHop = 64;

double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

void report(const ProgressFn& fn, float p) {
  if (fn) fn(p);
}

// ---------------------------------------------------------------------------------------------------------------------
// Decimated mono
// ---------------------------------------------------------------------------------------------------------------------

struct Mono {
  std::vector<float> x;
  double fs = 0.0;
};

Mono decimatedMono(const float* lr, std::int64_t frames, int sampleRate) {
  Mono m;
  const int dec = std::max(1, static_cast<int>(std::lround(sampleRate / kAnalysisRate)));
  m.fs = static_cast<double>(sampleRate) / dec;
  const std::int64_t outLen = frames / dec;
  m.x.assign(static_cast<std::size_t>(outLen), 0.f);
  if (dec == 1) {
    for (std::int64_t i = 0; i < outLen; ++i) m.x[static_cast<std::size_t>(i)] = 0.5f * (lr[2 * i] + lr[2 * i + 1]);
    return m;
  }
  // Blackman-windowed sinc low-pass at 0.45 * output rate.
  const int taps = 16 * dec + 1;
  const int half = taps / 2;
  const double fc = 0.45 / dec;  // cycles per input sample
  std::vector<double> h(static_cast<std::size_t>(taps));
  double sum = 0.0;
  for (int k = 0; k < taps; ++k) {
    const double t = k - half;
    const double s = (t == 0.0) ? 2.0 * fc : std::sin(2.0 * M_PI * fc * t) / (M_PI * t);
    const double w = 0.42 - 0.5 * std::cos(2.0 * M_PI * k / (taps - 1)) + 0.08 * std::cos(4.0 * M_PI * k / (taps - 1));
    h[static_cast<std::size_t>(k)] = s * w;
    sum += h[static_cast<std::size_t>(k)];
  }
  for (double& v : h) v /= sum;
  for (std::int64_t n = 0; n < outLen; ++n) {
    const std::int64_t centre = n * dec;
    double acc = 0.0;
    for (int k = 0; k < taps; ++k) {
      const std::int64_t i = centre + k - half;
      if (i < 0 || i >= frames) continue;
      acc += h[static_cast<std::size_t>(k)] * 0.5 * (static_cast<double>(lr[2 * i]) + lr[2 * i + 1]);
    }
    m.x[static_cast<std::size_t>(n)] = static_cast<float>(acc);
  }
  return m;
}

// ---------------------------------------------------------------------------------------------------------------------
// Onset strength (log-compressed spectral flux), plus a low-band version for downbeat detection
// ---------------------------------------------------------------------------------------------------------------------

struct OnsetEnv {
  std::vector<double> full;
  std::vector<double> low;
  double rate = 0.0;  // frames per second
};

OnsetEnv computeOnsets(const std::vector<float>& x, double fs, const ProgressFn& progress) {
  OnsetEnv env;
  env.rate = fs / kOnsetHop;
  if (x.size() < static_cast<std::size_t>(kOnsetN) * 2) return env;
  const std::size_t frames = (x.size() - kOnsetN) / kOnsetHop + 1;
  env.full.assign(frames, 0.0);
  env.low.assign(frames, 0.0);

  Fft fft(kOnsetN);
  const std::vector<double> win = hannWindow(kOnsetN);
  std::vector<double> frame(kOnsetN), mag(kOnsetN / 2 + 1), prev(kOnsetN / 2 + 1, 0.0);
  std::vector<std::complex<double>> scratch;
  const int kLo = 1;
  const int kLowTop = 8;                                                      // ~190 Hz at 12 kHz / 512
  const int kHi = std::min<int>(kOnsetN / 2, static_cast<int>(5000.0 / fs * kOnsetN));
  const double norm = 4.0 / kOnsetN;
  for (std::size_t t = 0; t < frames; ++t) {
    const float* src = x.data() + t * kOnsetHop;
    for (int i = 0; i < kOnsetN; ++i) frame[static_cast<std::size_t>(i)] = src[i] * win[static_cast<std::size_t>(i)];
    fft.magnitudes(frame.data(), mag.data(), scratch);
    double flux = 0.0, lowFlux = 0.0;
    for (int k = kLo; k <= kHi; ++k) {
      const double c = std::log1p(1000.0 * mag[static_cast<std::size_t>(k)] * norm);
      const double d = c - prev[static_cast<std::size_t>(k)];
      prev[static_cast<std::size_t>(k)] = c;
      if (d > 0.0) {
        flux += d;
        if (k <= kLowTop) lowFlux += d;
      }
    }
    env.full[t] = flux;
    env.low[t] = lowFlux;
    if ((t & 4095u) == 0) report(progress, 0.05f + 0.25f * static_cast<float>(t) / static_cast<float>(frames));
  }
  env.full[0] = 0.0;  // the first frame has no predecessor
  env.low[0] = 0.0;
  return env;
}

std::vector<double> movingAverage(const std::vector<double>& v, std::size_t radius) {
  std::vector<double> out(v.size());
  std::vector<double> csum(v.size() + 1, 0.0);
  for (std::size_t i = 0; i < v.size(); ++i) csum[i + 1] = csum[i] + v[i];
  for (std::size_t i = 0; i < v.size(); ++i) {
    const std::size_t a = i > radius ? i - radius : 0;
    const std::size_t b = std::min(v.size(), i + radius + 1);
    out[i] = (csum[b] - csum[a]) / static_cast<double>(b - a);
  }
  return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Tempo
// ---------------------------------------------------------------------------------------------------------------------

struct TempoEstimate {
  double bpm = 0.0;
  double confidence = 0.0;
};

double acfAt(const std::vector<double>& acf, double lag) {
  if (lag < 1.0) return 0.0;
  const std::size_t i = static_cast<std::size_t>(lag);
  if (i + 1 >= acf.size()) return 0.0;
  const double f = lag - static_cast<double>(i);
  return acf[i] * (1.0 - f) + acf[i + 1] * f;
}

TempoEstimate estimateTempo(const OnsetEnv& env, const AnalysisOptions& opt) {
  TempoEstimate out;
  const std::size_t n = env.full.size();
  if (n < static_cast<std::size_t>(env.rate * 8.0)) return out;  // under ~8 s there is nothing to measure

  // Flatten slow level changes, keep only rises.
  std::vector<double> o(n);
  const std::vector<double> avg = movingAverage(env.full, static_cast<std::size_t>(env.rate * 0.75));
  double energy = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    o[i] = std::max(0.0, env.full[i] - avg[i]);
    energy += o[i] * o[i];
  }
  if (energy < 1e-9) return out;

  // Autocorrelation up to 4 beats of the slowest tempo we accept.
  const std::size_t maxLag = static_cast<std::size_t>(env.rate * 60.0 / opt.minBpm * 4.0) + 2;
  std::vector<double> acf(std::min(maxLag + 1, n / 2), 0.0);
  for (std::size_t lag = 1; lag < acf.size(); ++lag) {
    double s = 0.0;
    for (std::size_t i = 0; i + lag < n; ++i) s += o[i] * o[i + lag];
    acf[lag] = s / static_cast<double>(n - lag);
  }
  const double a0 = energy / static_cast<double>(n);

  // Score every candidate tempo on a fine grid: periodicity at 1, 2, 3 and 4 beats, weighted by a broad prior around 124 BPM
  // that settles half/double-time ambiguity in favour of the usual range.
  const double step = 0.1;
  std::vector<double> scores;
  std::vector<double> bpms;
  for (double b = opt.minBpm; b <= opt.maxBpm; b += step) {
    const double lag = env.rate * 60.0 / b;
    const double s = acfAt(acf, lag) + 0.6 * acfAt(acf, 2.0 * lag) + 0.4 * acfAt(acf, 3.0 * lag) + 0.3 * acfAt(acf, 4.0 * lag);
    const double octave = std::log2(b / 124.0);
    const double prior = std::exp(-0.5 * (octave / 0.75) * (octave / 0.75));
    bpms.push_back(b);
    scores.push_back(s * prior);
  }
  if (scores.empty()) return out;
  std::size_t best = 0;
  for (std::size_t i = 1; i < scores.size(); ++i) {
    if (scores[i] > scores[best]) best = i;
  }
  if (scores[best] <= 0.0) return out;

  // The raw autocorrelation peak can be a few tenths of a BPM off; the grid fit below refines it. Confidence compares the
  // peak with the typical score level and with the zero-lag energy.
  std::vector<double> sorted = scores;
  std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(sorted.size() / 2), sorted.end());
  const double median = sorted[sorted.size() / 2];
  const double sharp = (scores[best] - median) / (scores[best] + 1e-12);
  const double strength = std::min(1.0, scores[best] / (a0 * 1.6));
  out.bpm = bpms[best];
  out.confidence = clamp01(0.6 * sharp + 0.4 * strength);
  return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Grid: coarse phase from the onset envelope, then least-squares refinement on a fine transient signal
// ---------------------------------------------------------------------------------------------------------------------

struct GridFit {
  bool ok = false;
  double periodSamples = 0.0;  // beat period in decimated samples
  double originSamples = 0.0;  // time of beat 0 (any beat, before the downbeat assignment), decimated samples
  double confidence = 0.0;
};

/// Fine "attack" signal: rise of the smoothed energy of the pre-emphasised input, ~2 ms resolution.
std::vector<float> attackSignal(const std::vector<float>& x, double fs) {
  const std::size_t n = x.size();
  const std::size_t w = std::max<std::size_t>(2, static_cast<std::size_t>(fs * 0.0017));
  std::vector<double> e(n, 0.0);
  float prev = 0.f;
  for (std::size_t i = 0; i < n; ++i) {
    const double d = x[i] - prev;
    prev = x[i];
    e[i] = d * d;
  }
  // boxcar smoothing of the energy
  std::vector<double> sm(n, 0.0);
  double acc = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    acc += e[i];
    if (i >= w) acc -= e[i - w];
    sm[i] = acc / static_cast<double>(w);
  }
  std::vector<float> a(n, 0.f);
  const std::size_t lookback = w;
  for (std::size_t i = lookback; i < n; ++i) {
    const double rise = sm[i] - sm[i - lookback];
    a[i] = rise > 0.0 ? static_cast<float>(std::sqrt(rise)) : 0.f;
  }
  return a;
}

GridFit fitGrid(const OnsetEnv& env, const std::vector<float>& attack, double fs, double bpm0) {
  GridFit g;
  const double envPeriod = env.rate * 60.0 / bpm0;  // beat period in envelope frames
  const std::size_t n = env.full.size();
  if (envPeriod < 4.0 || n < static_cast<std::size_t>(envPeriod * 8)) return g;

  // Coarse phase: sum of the onset envelope on the comb, for every offset within one beat.
  std::vector<double> o(n);
  const std::vector<double> avg = movingAverage(env.full, static_cast<std::size_t>(env.rate * 0.75));
  for (std::size_t i = 0; i < n; ++i) o[i] = std::max(0.0, env.full[i] - avg[i]);
  const int phases = static_cast<int>(std::ceil(envPeriod));
  double bestScore = -1.0;
  double bestPhase = 0.0;
  for (int p = 0; p < phases; ++p) {
    const double phase = p * envPeriod / phases;
    double s = 0.0;
    for (double t = phase; t < static_cast<double>(n) - 1.0; t += envPeriod) {
      const std::size_t i = static_cast<std::size_t>(t);
      const double f = t - static_cast<double>(i);
      s += o[i] * (1.0 - f) + o[i + 1] * f;
    }
    if (s > bestScore) {
      bestScore = s;
      bestPhase = phase;
    }
  }
  // Envelope frame t covers samples [t*hop, t*hop + N): an onset shows up around its centre.
  const double framesToSamples = static_cast<double>(kOnsetHop);
  const double centreShift = kOnsetN * 0.5 - 0.0 * kOnsetN;
  double period = envPeriod * framesToSamples;
  double origin = bestPhase * framesToSamples + centreShift;

  // Fine refinement: snap each predicted beat to the strongest attack nearby, then fit a line through the snapped beats.
  const std::size_t len = attack.size();
  for (int pass = 0; pass < 3; ++pass) {
    const double w = pass == 0 ? std::min(0.5 * period * 0.45, 0.05 * fs) : std::min(0.5 * period * 0.3, 0.02 * fs);
    struct Peak { double k, pos, strength; };
    std::vector<Peak> peaks;
    const double first = std::fmod(origin, period);
    for (int64_t k = 0;; ++k) {
      const double t = first + static_cast<double>(k) * period;
      if (t + w >= static_cast<double>(len) - 2.0) break;
      if (t - w < 2.0) continue;
      std::size_t bestI = 0;
      float bestV = -1.f;
      const std::size_t a = static_cast<std::size_t>(t - w), b = static_cast<std::size_t>(t + w);
      for (std::size_t i = a; i <= b; ++i) {
        if (attack[i] > bestV) {
          bestV = attack[i];
          bestI = i;
        }
      }
      peaks.push_back({static_cast<double>(k), static_cast<double>(bestI), static_cast<double>(bestV)});
    }
    if (peaks.size() < 8) return g;
    std::vector<double> strengths;
    for (const Peak& p : peaks) strengths.push_back(p.strength);
    std::nth_element(strengths.begin(), strengths.begin() + static_cast<std::ptrdiff_t>(strengths.size() / 2), strengths.end());
    const double medStrength = strengths[strengths.size() / 2];

    // Robust line fit pos = a + b*k with iterative re-weighting (only reasonably strong peaks vote).
    double a = first, b = period;
    for (int it = 0; it < 6; ++it) {
      double sw = 0, sk = 0, sp = 0, skk = 0, skp = 0;
      for (const Peak& p : peaks) {
        if (p.strength < 0.35 * medStrength) continue;
        const double resid = p.pos - (a + b * p.k);
        const double tol = (it == 0 ? 3.0 : 1.0) * 0.004 * fs + 0.004 * fs;  // 8 ms first, 4 ms later
        if (std::fabs(resid) > tol * 2.0) continue;
        const double wgt = 1.0 / (1.0 + (resid / tol) * (resid / tol));
        sw += wgt;
        sk += wgt * p.k;
        sp += wgt * p.pos;
        skk += wgt * p.k * p.k;
        skp += wgt * p.k * p.pos;
      }
      const double det = sw * skk - sk * sk;
      if (sw < 8 || std::fabs(det) < 1e-9) return g;
      b = (sw * skp - sk * sp) / det;
      a = (sp - b * sk) / sw;
    }
    // Sanity: refined period must stay within 3 % of the estimate, otherwise the fit latched onto something else.
    if (std::fabs(b - period) > 0.03 * period) return g;
    period = b;
    origin = a;
    if (pass == 2) {
      // A beat counts as "seen" when a clear attack sits on it: white noise peaks at ~4x the mean attack level everywhere,
      // real drum hits at 5x and far above (measured on the synthetic tracks), so 4.8x separates a grid from chance.
      double meanAttack = 0.0;
      for (float v : attack) meanAttack += v;
      meanAttack /= static_cast<double>(attack.size());
      std::size_t good = 0;
      for (const Peak& p : peaks) {
        if (p.strength >= 4.8 * meanAttack && std::fabs(p.pos - (a + b * p.k)) < 0.015 * fs) ++good;
      }
      g.confidence = clamp01(static_cast<double>(good) / static_cast<double>(peaks.size()));
    }
  }
  g.ok = true;
  g.periodSamples = period;
  g.originSamples = origin;
  return g;
}

// ---------------------------------------------------------------------------------------------------------------------
// Key (Krumhansl-Schmuckler on a chroma profile)
// ---------------------------------------------------------------------------------------------------------------------

constexpr double kMajorProfile[12] = {6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88};
constexpr double kMinorProfile[12] = {6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17};

double pearson(const double* a, const double* b, int n) {
  double ma = 0, mb = 0;
  for (int i = 0; i < n; ++i) {
    ma += a[i];
    mb += b[i];
  }
  ma /= n;
  mb /= n;
  double sab = 0, saa = 0, sbb = 0;
  for (int i = 0; i < n; ++i) {
    sab += (a[i] - ma) * (b[i] - mb);
    saa += (a[i] - ma) * (a[i] - ma);
    sbb += (b[i] - mb) * (b[i] - mb);
  }
  return (saa > 0 && sbb > 0) ? sab / std::sqrt(saa * sbb) : 0.0;
}

struct KeyEstimate {
  int tonic = -1;
  bool minor = false;
  double confidence = 0.0;
};

KeyEstimate estimateKey(const std::vector<float>& x, double fs, const ProgressFn& progress) {
  KeyEstimate k;
  constexpr int N = 8192;
  constexpr int hop = 4096;
  if (x.size() < static_cast<std::size_t>(N) * 2) return k;
  const std::size_t frames = (x.size() - N) / hop + 1;
  Fft fft(N);
  const std::vector<double> win = hannWindow(N);
  std::vector<double> frame(N), mag(N / 2 + 1);
  std::vector<std::complex<double>> scratch;

  // Per-bin pitch class and a weight that fades out towards the gaps between semitones.
  const int binLo = std::max(1, static_cast<int>(55.0 * N / fs));
  const int binHi = std::min(N / 2, static_cast<int>(2200.0 * N / fs));
  std::vector<int> pc(static_cast<std::size_t>(binHi + 1), 0);
  std::vector<double> wgt(static_cast<std::size_t>(binHi + 1), 0.0);
  for (int b = binLo; b <= binHi; ++b) {
    const double f = b * fs / N;
    const double midi = 69.0 + 12.0 * std::log2(f / 440.0);
    const double r = std::round(midi);
    const double dev = std::fabs(midi - r);
    pc[static_cast<std::size_t>(b)] = (static_cast<int>(r) % 12 + 12) % 12;
    wgt[static_cast<std::size_t>(b)] = std::max(0.0, 1.0 - dev / 0.5);
  }

  // Frame loudness gate: ignore near-silence.
  std::vector<double> frameRms(frames, 0.0);
  double maxRms = 0.0;
  for (std::size_t t = 0; t < frames; ++t) {
    double s = 0.0;
    const float* src = x.data() + t * hop;
    for (int i = 0; i < N; ++i) s += static_cast<double>(src[i]) * src[i];
    frameRms[t] = std::sqrt(s / N);
    maxRms = std::max(maxRms, frameRms[t]);
  }
  if (maxRms < 1e-5) return k;

  double chroma[12] = {};
  for (std::size_t t = 0; t < frames; ++t) {
    if (frameRms[t] < 0.05 * maxRms) continue;
    const float* src = x.data() + t * hop;
    for (int i = 0; i < N; ++i) frame[static_cast<std::size_t>(i)] = src[i] * win[static_cast<std::size_t>(i)];
    fft.magnitudes(frame.data(), mag.data(), scratch);
    double c[12] = {};
    for (int b = binLo; b <= binHi; ++b) {
      c[pc[static_cast<std::size_t>(b)]] += wgt[static_cast<std::size_t>(b)] * std::log1p(200.0 * mag[static_cast<std::size_t>(b)] * 4.0 / N);
    }
    double norm = 0.0;
    for (double v : c) norm += v * v;
    norm = std::sqrt(norm);
    if (norm <= 0.0) continue;
    for (int i = 0; i < 12; ++i) chroma[i] += c[i] / norm;
    if ((t & 63u) == 0) report(progress, 0.45f + 0.2f * static_cast<float>(t) / static_cast<float>(frames));
  }

  double best = -2.0, second = -2.0;
  int bestTonic = -1;
  bool bestMinor = false;
  for (int minor = 0; minor < 2; ++minor) {
    for (int tonic = 0; tonic < 12; ++tonic) {
      double rotated[12];
      for (int i = 0; i < 12; ++i) rotated[i] = (minor ? kMinorProfile : kMajorProfile)[(i - tonic + 12) % 12];
      const double r = pearson(chroma, rotated, 12);
      if (r > best) {
        second = best;
        best = r;
        bestTonic = tonic;
        bestMinor = minor == 1;
      } else if (r > second) {
        second = r;
      }
    }
  }
  if (bestTonic < 0 || best < 0.2) return k;
  k.tonic = bestTonic;
  k.minor = bestMinor;
  k.confidence = clamp01(0.6 * best + 4.0 * (best - second));
  return k;
}

// ---------------------------------------------------------------------------------------------------------------------
// Loudness, bar energy, structure
// ---------------------------------------------------------------------------------------------------------------------

struct Loudness {
  double rmsDb = -120.0;
  double peakDb = -120.0;
};

std::vector<double> barRms(const float* lr, std::int64_t frames, int sr, double firstBarSec, double barSec, Loudness* loud) {
  std::vector<double> rms;
  if (barSec <= 0.0) return rms;
  double peak = 0.0;
  for (std::int64_t i = 0; i < frames * 2; ++i) peak = std::max(peak, static_cast<double>(std::fabs(lr[i])));
  const double dur = static_cast<double>(frames) / sr;
  const std::size_t bars = static_cast<std::size_t>(std::floor((dur - firstBarSec) / barSec + 1e-9));
  rms.assign(bars, 0.0);
  for (std::size_t b = 0; b < bars; ++b) {
    const std::int64_t a = static_cast<std::int64_t>((firstBarSec + b * barSec) * sr);
    const std::int64_t e = std::min<std::int64_t>(frames, static_cast<std::int64_t>((firstBarSec + (b + 1) * barSec) * sr));
    double s = 0.0;
    for (std::int64_t i = a; i < e; ++i) {
      const double m = 0.5 * (lr[2 * i] + lr[2 * i + 1]);
      s += m * m;
    }
    rms[b] = e > a ? std::sqrt(s / static_cast<double>(e - a)) : 0.0;
  }
  if (loud) {
    // RMS over the non-silent bars (a silent intro or tail must not pull the level down).
    double s = 0.0;
    std::size_t cnt = 0;
    double mx = 0.0;
    for (double r : rms) mx = std::max(mx, r);
    for (double r : rms) {
      if (r > 0.1 * mx) {
        s += r * r;
        ++cnt;
      }
    }
    const double r = cnt ? std::sqrt(s / static_cast<double>(cnt)) : 0.0;
    loud->rmsDb = r > 1e-9 ? 20.0 * std::log10(r) : -120.0;
    loud->peakDb = peak > 1e-9 ? 20.0 * std::log10(peak) : -120.0;
  }
  return rms;
}

double percentile(std::vector<double> v, double p) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  const double pos = p * static_cast<double>(v.size() - 1);
  const std::size_t i = static_cast<std::size_t>(pos);
  const double f = pos - static_cast<double>(i);
  return i + 1 < v.size() ? v[i] * (1.0 - f) + v[i + 1] * f : v[i];
}

std::vector<double> medianFilter3(const std::vector<double>& v) {
  std::vector<double> o(v.size());
  for (std::size_t i = 0; i < v.size(); ++i) {
    double a = v[i > 0 ? i - 1 : i], b = v[i], c = v[i + 1 < v.size() ? i + 1 : i];
    if (a > b) std::swap(a, b);
    if (b > c) std::swap(b, c);
    if (a > b) std::swap(a, b);
    o[i] = b;
  }
  return o;
}

void buildStructure(TrackAnalysis& a) {
  const std::size_t n = a.barEnergy.size();
  a.sections.clear();
  if (n == 0 || a.barSec <= 0.0) return;
  std::vector<double> e(n);
  for (std::size_t i = 0; i < n; ++i) e[i] = a.barEnergy[i];
  e = medianFilter3(e);
  const double ref = std::max(0.05, percentile(e, 0.75));
  std::vector<char> high(n);
  for (std::size_t i = 0; i < n; ++i) high[i] = e[i] >= 0.72 * ref ? 1 : 0;

  // Runs of equal "level", short runs (< 4 bars) are absorbed into their neighbours.
  struct Run { std::size_t start, end; bool high; };
  std::vector<Run> runs;
  for (std::size_t i = 0; i < n;) {
    std::size_t j = i;
    while (j < n && high[j] == high[i]) ++j;
    runs.push_back({i, j, high[i] != 0});
    i = j;
  }
  bool changed = true;
  while (changed && runs.size() > 1) {
    changed = false;
    for (std::size_t r = 0; r < runs.size(); ++r) {
      if (runs[r].end - runs[r].start >= 4) continue;
      if (runs.size() == 1) break;
      // merge into the longer neighbour (the first/last run only has one)
      std::size_t into = (r == 0) ? 1 : (r + 1 == runs.size() ? r - 1 : ((runs[r - 1].end - runs[r - 1].start) >= (runs[r + 1].end - runs[r + 1].start) ? r - 1 : r + 1));
      runs[into].start = std::min(runs[into].start, runs[r].start);
      runs[into].end = std::max(runs[into].end, runs[r].end);
      runs.erase(runs.begin() + static_cast<std::ptrdiff_t>(r));
      // neighbours with the same level fuse
      for (std::size_t q = 0; q + 1 < runs.size();) {
        if (runs[q].high == runs[q + 1].high) {
          runs[q].end = runs[q + 1].end;
          runs.erase(runs.begin() + static_cast<std::ptrdiff_t>(q + 1));
        } else {
          ++q;
        }
      }
      changed = true;
      break;
    }
  }

  auto timeOf = [&](std::size_t bar) { return a.firstBarSec + static_cast<double>(bar) * a.barSec; };
  for (std::size_t r = 0; r < runs.size(); ++r) {
    Section s;
    s.startSec = r == 0 ? 0.0 : timeOf(runs[r].start);
    s.endSec = r + 1 == runs.size() ? a.durationSec : timeOf(runs[r].end);
    if (!runs[r].high) {
      if (r == 0 && runs.size() > 1) s.label = "intro";
      else if (r + 1 == runs.size() && runs.size() > 1) s.label = "outro";
      else s.label = "break";
    } else {
      s.label = (r > 0) ? "drop" : "body";
    }
    a.sections.push_back(s);
  }
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Names
// ---------------------------------------------------------------------------------------------------------------------

std::string keyName(int tonic, bool minor) {
  static const char* names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  if (tonic < 0 || tonic > 11) return {};
  return std::string(names[tonic]) + (minor ? " minor" : " major");
}

std::string camelotName(int tonic, bool minor) {
  static const int major[12] = {8, 3, 10, 5, 12, 7, 2, 9, 4, 11, 6, 1};
  static const int minorN[12] = {5, 12, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10};
  if (tonic < 0 || tonic > 11) return {};
  return std::to_string(minor ? minorN[tonic] : major[tonic]) + (minor ? "A" : "B");
}

bool camelotCompatible(const std::string& a, const std::string& b) {
  if (a.size() < 2 || b.size() < 2) return false;
  const int na = std::atoi(a.substr(0, a.size() - 1).c_str());
  const int nb = std::atoi(b.substr(0, b.size() - 1).c_str());
  const char la = a.back(), lb = b.back();
  if (na <= 0 || nb <= 0) return false;
  if (na == nb) return true;  // same number: identical key or relative major/minor
  if (la != lb) return false;
  const int d = std::abs(na - nb);
  return d == 1 || d == 11;
}

// ---------------------------------------------------------------------------------------------------------------------
// Orchestration
// ---------------------------------------------------------------------------------------------------------------------

TrackAnalysis analyzeTrack(const float* lr, std::int64_t frames, int sampleRate, const AnalysisOptions& opt,
                           const ProgressFn& progress) {
  TrackAnalysis a;
  if (lr == nullptr || frames <= 0 || sampleRate <= 0) return a;
  a.durationSec = static_cast<double>(frames) / sampleRate;
  report(progress, 0.f);

  const Mono mono = decimatedMono(lr, frames, sampleRate);
  report(progress, 0.05f);

  // Tempo and grid.
  const OnsetEnv env = computeOnsets(mono.x, mono.fs, progress);
  TempoEstimate tempo = estimateTempo(env, opt);
  report(progress, 0.32f);
  GridFit grid;
  if (tempo.bpm > 0.0) {
    const std::vector<float> attack = attackSignal(mono.x, mono.fs);
    grid = fitGrid(env, attack, mono.fs, tempo.bpm);
  }
  report(progress, 0.4f);

  if (tempo.bpm > 0.0 && grid.ok) {
    const double beatSec = grid.periodSamples / mono.fs;
    a.beatSec = beatSec;
    a.bpm = 60.0 / beatSec;
    a.tempoConfidence = tempo.confidence;
    a.gridConfidence = grid.confidence;

    // Downbeat: of the four beat positions in a bar, the one with the most low-frequency onset energy.
    double score[4] = {};
    int count[4] = {};
    const double firstBeatSample = std::fmod(grid.originSamples, grid.periodSamples);
    // Beat index of `firstBeatSample` relative to the fitted origin.
    const double k0 = std::round((firstBeatSample - grid.originSamples) / grid.periodSamples);
    for (int64_t k = 0;; ++k) {
      const double t = firstBeatSample + static_cast<double>(k) * grid.periodSamples;  // decimated samples
      const double frame = (t - kOnsetN * 0.5) / kOnsetHop;
      if (frame < 1.0 || frame + 2.0 >= static_cast<double>(env.low.size())) {
        if (frame >= static_cast<double>(env.low.size())) break;
        continue;
      }
      const std::size_t i = static_cast<std::size_t>(frame);
      // max over +-4 frames (+-21 ms): the low-band flux peak sits a few frames away from the transient
      double v = 0.0;
      for (std::size_t j = i > 4 ? i - 4 : 0; j <= std::min(env.low.size() - 1, i + 4); ++j) v = std::max(v, env.low[j]);
      score[k & 3] += v;
      ++count[k & 3];
    }
    int bestPos = 0;
    double total = 0.0, best = -1.0;
    for (int i = 0; i < 4; ++i) {
      const double m = count[i] ? score[i] / count[i] : 0.0;
      score[i] = m;
      total += m;
      if (m > best) {
        best = m;
        bestPos = i;
      }
    }
    const double meanOthers = (total - best) / 3.0;
    a.downbeatConfidence = clamp01((best - meanOthers) / (best + 1e-12) * 2.0);
    (void)k0;
    // Time of the first downbeat at or after t = 0: beats k with (k & 3) == bestPos, counting from firstBeatSample.
    const double downbeatSample = firstBeatSample + static_cast<double>(bestPos) * grid.periodSamples;
    a.gridOffsetSec = downbeatSample / mono.fs;
    // Keep it inside the first bar so that "bar 0" starts near the beginning of the file.
    const double barSecTmp = 4.0 * beatSec;
    a.gridOffsetSec = std::fmod(a.gridOffsetSec, barSecTmp);
    if (a.gridOffsetSec < 0.0) a.gridOffsetSec += barSecTmp;
  }
  report(progress, 0.45f);

  // Key.
  const KeyEstimate key = estimateKey(mono.x, mono.fs, progress);
  if (key.tonic >= 0) {
    a.keyTonic = key.tonic;
    a.keyMinor = key.minor;
    a.keyConfidence = key.confidence;
    a.keyName = keyName(key.tonic, key.minor);
    a.camelot = camelotName(key.tonic, key.minor);
  }
  report(progress, 0.65f);

  // Bars, energy, structure, loudness.
  if (a.hasTempo()) {
    a.barSec = 4.0 * a.beatSec;
    a.firstBarSec = std::fmod(a.gridOffsetSec, a.barSec);
  } else {
    a.barSec = 4.0;
    a.firstBarSec = 0.0;
  }
  Loudness loud;
  const std::vector<double> rms = barRms(lr, frames, sampleRate, a.firstBarSec, a.barSec, &loud);
  a.rmsDb = loud.rmsDb;
  a.peakDb = loud.peakDb;
  double trim = std::clamp(-18.0 - loud.rmsDb, -9.0, 9.0);
  trim = std::min(trim, -loud.peakDb + 1.0);
  a.trimDb = loud.rmsDb > -100.0 ? trim : 0.0;

  const double p90 = std::max(1e-9, percentile(rms, 0.9));
  a.barEnergy.resize(rms.size());
  for (std::size_t i = 0; i < rms.size(); ++i) a.barEnergy[i] = static_cast<float>(std::min(1.0, rms[i] / p90));
  buildStructure(a);
  report(progress, 0.85f);

  // Mix points: enter on the first downbeat, leave at the start of the outro (or the last 16-beat phrase boundary).
  a.mixInSec = a.hasTempo() ? a.firstBarSec : 0.0;
  double outStart = a.durationSec;
  for (const Section& s : a.sections) {
    if (s.label == "outro") outStart = s.startSec;
  }
  if (a.hasTempo()) {
    const double phrase = 16.0 * a.beatSec;
    if (outStart >= a.durationSec - 1e-6) outStart = a.durationSec - phrase - 2.0 * a.barSec;  // no outro found: last bars
    const double rel = outStart - a.firstBarSec;
    outStart = a.firstBarSec + std::floor(rel / a.barSec) * a.barSec;
  } else {
    outStart = std::max(0.0, a.durationSec - 20.0);
  }
  a.mixOutSec = std::clamp(outStart, a.mixInSec, a.durationSec);
  report(progress, 1.f);
  return a;
}

}  // namespace autodj::analysis
