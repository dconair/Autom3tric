#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "autodj/core/DemoSources.h"
#include "autodj/core/Dsp.h"
#include "autodj/core/SampleBuffer.h"
#include "autodj/granular/EnvelopeTables.h"
#include "autodj/granular/GrainKernels.h"
#include "test_util.h"

using namespace autodj;
using namespace autodj::granular;

namespace {

constexpr double kSr = 48000.0;

std::vector<float> flatEnv() {
  std::vector<float> e(kEnvTableStride, 1.f);
  for (int i = kEnvTableSize + 1; i < kEnvTableStride; ++i) e[i] = 1.f;  // flat, so the LUT never changes the signal
  return e;
}

// Source with an exact sine at `f` Hz in both channels.
std::shared_ptr<const SampleBuffer> sineSource(double f, double seconds) { return demo::sine(f, seconds, 48000, 1.0f); }

}  // namespace

TEST(Kernels, BackendIsReported) {
  const std::string name = kernelBackendName();
  EXPECT_TRUE(name == "neon" || name == "sse2" || name == "scalar") << name;
  SCOPED_TRACE(name);
}

// A pure source sine read at rate r must come out as a sine of frequency f*r.
// Linear interpolation error bound for a sine: (w^2 / 8) with w = 2*pi*f/sr  (w is the *source* phase step).
TEST(Kernels, InterpolationErrorAgainstAnalyticSine) {
  const double f = 1000.0;
  const auto src = sineSource(f, 2.0);
  const auto env = flatEnv();
  const double w = 2.0 * testutil::kPi * f / kSr;
  const double linBound = w * w / 8.0 * 1.05;

  struct Case { float rate; double start; };
  for (bool simd : {false, true}) {
    for (Interp interp : {Interp::Linear, Interp::Hermite}) {
      for (Case c : {Case{1.0f, 100.3}, Case{0.5f, 5000.77}, Case{1.5f, 1234.5}, Case{std::exp2(7.f / 12.f), 777.1},
                     Case{std::exp2(-12.f / 12.f), 2000.0}, Case{4.0f, 300.9}, Case{-1.0f, 50000.4}, Case{-0.7f, 40000.0}}) {
        const int n = 1500;
        std::vector<float> out(n * 2, 0.f);
        GrainJob job{src->data(), c.start, c.rate, 0.f, 0.f, env.data(), 1.f, 1.f, n, out.data()};
        selectKernel(interp, simd)(job);
        double worst = 0.0;
        for (int i = 0; i < n; ++i) {
          const double p = c.start + static_cast<double>(i) * static_cast<double>(c.rate);
          const double expect = std::sin(w * p);
          worst = std::max({worst, std::fabs(out[2 * i] - expect), std::fabs(out[2 * i + 1] - expect)});
        }
        const double bound = interp == Interp::Linear ? linBound : 3e-4;
        EXPECT_LT(worst, bound) << "simd=" << simd << " interp=" << static_cast<int>(interp) << " rate=" << c.rate;
      }
    }
  }
}

TEST(Kernels, OutputFrequencyEqualsSourceFrequencyTimesRate) {
  const double f = 2000.0;
  const auto src = sineSource(f, 3.0);
  const auto env = flatEnv();
  for (double semis : {-24.0, -12.0, -5.0, 0.0, 0.37, 7.0, 12.0, 24.0}) {
    const float rate = std::exp2(static_cast<float>(semis / 12.0));
    const int n = 24000;  // 500 ms, the longest grain
    std::vector<float> out(n * 2, 0.f);
    GrainJob job{src->data(), 1000.0, rate, 0.f, 0.f, env.data(), 1.f, 1.f, n, out.data()};
    selectKernel(Interp::Linear, true)(job);
    const double expected = f * std::pow(2.0, semis / 12.0);
    const double measured = testutil::peakFrequency(out.data(), 16384, kSr, expected * 0.9, expected * 1.1, 2);
    EXPECT_LT(std::fabs(testutil::centsBetween(measured, expected)), 0.5) << semis << " st: " << measured << " vs " << expected;
  }
}

namespace {

// Double-precision model of one grain job: exactly what the float kernels approximate. Adds into `out`.
void modelRender(const GrainJob& j, Interp interp, std::vector<double>& out) {
  for (int i = 0; i < j.n; ++i) {
    const double p = j.pos + static_cast<double>(i) * static_cast<double>(j.rate);
    const std::int64_t idx = static_cast<std::int64_t>(std::floor(p));
    const double f = p - static_cast<double>(idx);
    const float* a = j.src + 2 * idx;
    double y[2];
    for (int ch = 0; ch < 2; ++ch) {
      const double x0 = a[ch], x1 = a[ch + 2];
      if (interp == Interp::Linear) {
        y[ch] = x0 + (x1 - x0) * f;
      } else {
        const double xm1 = a[ch - 2], x2 = a[ch + 4];
        const double c1 = 0.5 * (x1 - xm1), c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2, c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
        y[ch] = ((c3 * f + c2) * f + c1) * f + x0;
      }
    }
    const double e = static_cast<double>(j.envPos) + static_cast<double>(i) * static_cast<double>(j.envStep);
    const std::int64_t ei = static_cast<std::int64_t>(std::floor(e));
    const double w = j.env[ei] + (static_cast<double>(j.env[ei + 1]) - j.env[ei]) * (e - static_cast<double>(ei));
    out[2 * i] += y[0] * w * j.gainL;
    out[2 * i + 1] += y[1] * w * j.gainR;
  }
}

}  // namespace

// Both kernels are checked against a double-precision model. Why not simply "SIMD == scalar"? The NEON kernels use
// fused multiply-adds (the SSE2 ones and the scalar reference round after every operation), and a source position is
// a float within the job (up to ~1000 frames), whose rounding step is ~6e-5 frames. One ULP of position times the
// signal slope is the unavoidable difference between two correct implementations (~ -90 dBFS on full-band noise,
// far below that on music). The bound below is that ULP argument, not a fudge factor; a wrong coefficient, lane or
// index is off by orders of magnitude more. Where no FMA is involved (SSE2) SIMD and scalar must also agree tightly.
TEST(Kernels, SimdMatchesScalarReferenceOnRandomJobs) {
  const auto src = demo::noise(2.0, 48000, 0.8f);
  const auto envTables = std::make_unique<EnvelopeTables>();
  XorShift32 rng(2024);
  const bool fused = std::string(kernelBackendName()) == "neon";
  double worstVsModel = 0.0, worstBound = 0.0, worstSimdVsScalar = 0.0;
  for (Interp interp : {Interp::Linear, Interp::Hermite}) {
    const GrainKernelFn simd = selectKernel(interp, true);
    const GrainKernelFn ref = selectKernel(interp, false);
    for (int iter = 0; iter < 3000; ++iter) {
      const int n = 1 + static_cast<int>(rng.nextU32() % 256);
      float rate = std::exp2(rng.bipolar() * 2.0f);  // 0.25 .. 4
      if (rng.uniform() < 0.4f) rate = -rate;
      const double span = std::fabs(static_cast<double>(rate)) * n;
      const double lo = 4.0 + (rate < 0 ? span : 0.0);
      const double pos = lo + rng.uniform() * (static_cast<double>(src->frames()) - 12.0 - span);
      const int shape = static_cast<int>(rng.nextU32() % 4);
      const float envStep = static_cast<float>(kEnvTableSize) / static_cast<float>(n);
      const float gl = rng.uniform(), gr = rng.uniform();
      std::vector<float> a(n * 2 + 8, 0.25f), b(n * 2 + 8, 0.25f);  // non-zero start: the kernel must accumulate
      GrainJob ja{src->data(), pos, rate, 0.f, envStep, envTables->table(shape), gl, gr, n, a.data()};
      GrainJob jb = ja;
      jb.out = b.data();
      simd(ja);
      ref(jb);
      std::vector<double> m(static_cast<std::size_t>(n) * 2, 0.25);
      modelRender(ja, interp, m);

      // Error budget: ~4 ULP of the in-job source position times the signal slope (adjacent-sample difference of
      // the 0.8-amplitude noise is <= 1.6, the Hermite derivative <= 4), plus the same for the envelope position.
      const double ulpPos = std::ldexp(1.0, static_cast<int>(std::floor(std::log2(span + 2.0))) - 23);
      const double ulpEnv = std::ldexp(1.0, static_cast<int>(std::floor(std::log2(static_cast<double>(kEnvTableSize)))) - 23);
      double envSlope = 0.0;
      for (int k = 0; k < kEnvTableSize; ++k) envSlope = std::max(envSlope, std::fabs(static_cast<double>(ja.env[k + 1] - ja.env[k])));
      const double bound = 1e-6 + 4.0 * ulpPos * (interp == Interp::Linear ? 1.6 : 4.0) + 4.0 * ulpEnv * envSlope * 1.2;

      for (std::size_t i = 0; i < m.size(); ++i) {
        ASSERT_NEAR(a[i], m[i], bound) << "SIMD vs model: iter " << iter << " n=" << n << " rate=" << rate << " i=" << i;
        ASSERT_NEAR(b[i], m[i], bound) << "scalar vs model: iter " << iter << " n=" << n << " rate=" << rate << " i=" << i;
        worstVsModel = std::max(worstVsModel, std::fabs(a[i] - m[i]));
        worstBound = std::max(worstBound, bound);
        worstSimdVsScalar = std::max(worstSimdVsScalar, static_cast<double>(std::fabs(a[i] - b[i])));
        if (!fused) {
          ASSERT_NEAR(a[i], b[i], 2e-6) << "iter " << iter << " n=" << n << " rate=" << rate << " i=" << i;
        }
      }
      // guard: nothing written past the requested frames
      for (std::size_t i = m.size(); i < a.size(); ++i) {
        ASSERT_EQ(a[i], 0.25f);
        ASSERT_EQ(b[i], 0.25f);
      }
    }
  }
  std::printf("[ info ] backend %s: worst |SIMD - model| %.3g (largest bound %.3g), worst |SIMD - scalar| %.3g\n",
              kernelBackendName(), worstVsModel, worstBound, worstSimdVsScalar);
}

TEST(Kernels, EdgeOfBufferReadsStayInsideTheGuardPadding) {
  // Place grains so their reads touch the first / last frames the scheduler may allow. Under ASan an
  // out-of-bounds read of the heap block would be reported.
  const auto src = demo::noise(0.1, 48000, 0.5f);
  const auto env = flatEnv();
  const double frames = static_cast<double>(src->frames());
  for (Interp interp : {Interp::Linear, Interp::Hermite}) {
    for (bool simd : {false, true}) {
      for (float rate : {1.f, 2.f, -1.f, -3.f, 0.25f}) {
        const int n = 200;
        const double span = std::fabs(static_cast<double>(rate)) * n;
        // earliest legal start (scheduler: pos >= 2) and latest legal end (pos + span <= frames - 4)
        const double startLo = rate > 0 ? 2.0 : 2.0 + span;
        const double startHi = rate > 0 ? frames - 4.0 - span : frames - 4.0;
        for (double start : {startLo, startHi}) {
          std::vector<float> out(n * 2, 0.f);
          GrainJob job{src->data(), start, rate, 0.f, 0.f, env.data(), 1.f, 1.f, n, out.data()};
          selectKernel(interp, simd)(job);
          for (float v : out) ASSERT_TRUE(std::isfinite(v));
        }
      }
    }
  }
}

TEST(Kernels, EnvelopeAndGainsAreApplied) {
  const auto noise = demo::noise(1.0, 48000, 0.5f);
  const auto envTables = std::make_unique<EnvelopeTables>();
  const int n = 2400;
  std::vector<float> out(n * 2, 0.f);
  GrainJob job{noise->data(), 100.0, 1.0f, 0.f, static_cast<float>(kEnvTableSize) / n, envTables->table(0), 0.5f, 0.25f, n, out.data()};
  selectKernel(Interp::Linear, true)(job);
  for (int i = 0; i < n; i += 97) {
    const double w = EnvelopeTables::evaluate(EnvShape::Hann, static_cast<double>(i) / n);
    EXPECT_NEAR(out[2 * i], noise->data()[2 * (100 + i)] * w * 0.5, 2e-4) << i;
    EXPECT_NEAR(out[2 * i + 1], noise->data()[2 * (100 + i) + 1] * w * 0.25, 2e-4) << i;
  }
  EXPECT_NEAR(out[0], 0.0, 1e-6) << "grain starts silent";
  EXPECT_LT(std::fabs(out[2 * (n - 1)]), 1e-3f) << "grain ends (almost) silent";
}
