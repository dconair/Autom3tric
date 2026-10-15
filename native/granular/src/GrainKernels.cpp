#include "autodj/granular/GrainKernels.h"

#include <cmath>

#if defined(AUTODJ_NEON) && !defined(AUTODJ_FORCE_SCALAR)
#include <arm_neon.h>
#define AUTODJ_KERNEL_NEON 1
#elif defined(AUTODJ_SSE2) && !defined(AUTODJ_FORCE_SCALAR)
#include <emmintrin.h>
#define AUTODJ_KERNEL_SSE2 1
#endif

namespace autodj::granular {

namespace {

struct Prepared {
  const float* s;  // pointer to source frame `base`
  float f0;        // fractional start position relative to `base`, always >= 0
};

// Positions inside one job are expressed relative to the integer frame `base` that lies at or
// below the smallest position the job touches. That keeps every in-block offset non-negative, so
// float->int truncation is a floor, and keeps float precision independent of the position in a
// ten-minute song (the double position is only split once per job).
AUTODJ_INLINE Prepared prepare(const GrainJob& j) noexcept {
  const double last = j.pos + static_cast<double>(j.n - 1) * static_cast<double>(j.rate);
  const double lo = j.pos < last ? j.pos : last;
  const std::int64_t base = static_cast<std::int64_t>(std::floor(lo));
  return {j.src + 2 * base, static_cast<float>(j.pos - static_cast<double>(base))};
}

AUTODJ_INLINE void hermiteCoeffs(float xm1, float x0, float x1, float x2, float f, float& y) noexcept {
  const float c1 = 0.5f * (x1 - xm1);
  const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
  const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
  y = ((c3 * f + c2) * f + c1) * f + x0;
}

template <bool Hermite>
AUTODJ_INLINE void renderScalarRange(const GrainJob& j, const Prepared& pr, int begin, int end) noexcept {
  float* out = j.out;
  for (int i = begin; i < end; ++i) {
    // Same evaluation order as the SIMD kernels (base of the 4-frame group + lane * step), so the
    // scalar tail and the scalar reference path agree with them to float rounding of the filter.
    const float p = (pr.f0 + static_cast<float>(i & ~3) * j.rate) + static_cast<float>(i & 3) * j.rate;
    const int idx = static_cast<int>(p);
    const float f = p - static_cast<float>(idx);
    const float* a = pr.s + 2 * idx;
    float l, r;
    if constexpr (Hermite) {
      hermiteCoeffs(a[-2], a[0], a[2], a[4], f, l);
      hermiteCoeffs(a[-1], a[1], a[3], a[5], f, r);
    } else {
      l = a[0] + (a[2] - a[0]) * f;
      r = a[1] + (a[3] - a[1]) * f;
    }
    const float e = (j.envPos + static_cast<float>(i & ~3) * j.envStep) + static_cast<float>(i & 3) * j.envStep;
    const int ei = static_cast<int>(e);
    const float ef = e - static_cast<float>(ei);
    const float w = j.env[ei] + (j.env[ei + 1] - j.env[ei]) * ef;
    out[2 * i] += l * w * j.gainL;
    out[2 * i + 1] += r * w * j.gainR;
  }
}

#if defined(AUTODJ_KERNEL_NEON)

AUTODJ_INLINE float32x4_t hermiteNeon(float32x4_t xm1, float32x4_t x0, float32x4_t x1, float32x4_t x2,
                                      float32x4_t f) noexcept {
  const float32x4_t half = vdupq_n_f32(0.5f);
  const float32x4_t c1 = vmulq_f32(half, vsubq_f32(x1, xm1));
  float32x4_t c2 = vfmaq_n_f32(xm1, x0, -2.5f);
  c2 = vfmaq_n_f32(c2, x1, 2.0f);
  c2 = vfmaq_n_f32(c2, x2, -0.5f);
  float32x4_t c3 = vmulq_f32(half, vsubq_f32(x2, xm1));
  c3 = vfmaq_n_f32(c3, vsubq_f32(x0, x1), 1.5f);
  float32x4_t y = vfmaq_f32(c2, c3, f);
  y = vfmaq_f32(c1, y, f);
  return vfmaq_f32(x0, y, f);
}

template <bool Hermite>
void renderNeon(const GrainJob& j) noexcept {
  const Prepared pr = prepare(j);
  const float32x4_t k0123 = {0.f, 1.f, 2.f, 3.f};
  const float32x4_t vgL = vdupq_n_f32(j.gainL);
  const float32x4_t vgR = vdupq_n_f32(j.gainR);
  int i = 0;
  for (; i + 4 <= j.n; i += 4) {
    const float32x4_t pv = vfmaq_n_f32(vdupq_n_f32(pr.f0 + static_cast<float>(i) * j.rate), k0123, j.rate);
    const int32x4_t iv = vcvtq_s32_f32(pv);
    const float32x4_t fv = vsubq_f32(pv, vcvtq_f32_s32(iv));
    const float32x4_t ev = vfmaq_n_f32(vdupq_n_f32(j.envPos + static_cast<float>(i) * j.envStep), k0123, j.envStep);
    const int32x4_t eiv = vcvtq_s32_f32(ev);
    const float32x4_t efv = vsubq_f32(ev, vcvtq_f32_s32(eiv));
    alignas(16) std::int32_t ia[4];
    alignas(16) std::int32_t ea[4];
    vst1q_s32(ia, iv);
    vst1q_s32(ea, eiv);

    // Envelope: gather (e[k], e[k+1]) pairs, de-interleave into "lower" and "upper" vectors.
    const float32x4_t e01 = vcombine_f32(vld1_f32(j.env + ea[0]), vld1_f32(j.env + ea[1]));
    const float32x4_t e23 = vcombine_f32(vld1_f32(j.env + ea[2]), vld1_f32(j.env + ea[3]));
    const float32x4_t elo = vuzp1q_f32(e01, e23);
    const float32x4_t ehi = vuzp2q_f32(e01, e23);
    const float32x4_t w = vfmaq_f32(elo, vsubq_f32(ehi, elo), efv);
    const float32x4_t wl = vmulq_f32(w, vgL);
    const float32x4_t wr = vmulq_f32(w, vgR);
    const float32x4_t w01 = vzip1q_f32(wl, wr);  // L0 R0 L1 R1
    const float32x4_t w23 = vzip2q_f32(wl, wr);  // L2 R2 L3 R3
    const float32x4_t f01 = vzip1q_f32(fv, fv);  // f0 f0 f1 f1
    const float32x4_t f23 = vzip2q_f32(fv, fv);

    const float* p0 = pr.s + 2 * ia[0];
    const float* p1 = pr.s + 2 * ia[1];
    const float* p2 = pr.s + 2 * ia[2];
    const float* p3 = pr.s + 2 * ia[3];

    float32x4_t y01, y23;
    if constexpr (Hermite) {
      y01 = hermiteNeon(vcombine_f32(vld1_f32(p0 - 2), vld1_f32(p1 - 2)), vcombine_f32(vld1_f32(p0), vld1_f32(p1)),
                        vcombine_f32(vld1_f32(p0 + 2), vld1_f32(p1 + 2)), vcombine_f32(vld1_f32(p0 + 4), vld1_f32(p1 + 4)),
                        f01);
      y23 = hermiteNeon(vcombine_f32(vld1_f32(p2 - 2), vld1_f32(p3 - 2)), vcombine_f32(vld1_f32(p2), vld1_f32(p3)),
                        vcombine_f32(vld1_f32(p2 + 2), vld1_f32(p3 + 2)), vcombine_f32(vld1_f32(p2 + 4), vld1_f32(p3 + 4)),
                        f23);
    } else {
      const float32x4_t a01 = vcombine_f32(vld1_f32(p0), vld1_f32(p1));
      const float32x4_t b01 = vcombine_f32(vld1_f32(p0 + 2), vld1_f32(p1 + 2));
      const float32x4_t a23 = vcombine_f32(vld1_f32(p2), vld1_f32(p3));
      const float32x4_t b23 = vcombine_f32(vld1_f32(p2 + 2), vld1_f32(p3 + 2));
      y01 = vfmaq_f32(a01, vsubq_f32(b01, a01), f01);
      y23 = vfmaq_f32(a23, vsubq_f32(b23, a23), f23);
    }
    float* o = j.out + 2 * i;
    vst1q_f32(o, vfmaq_f32(vld1q_f32(o), y01, w01));
    vst1q_f32(o + 4, vfmaq_f32(vld1q_f32(o + 4), y23, w23));
  }
  if (i < j.n) renderScalarRange<Hermite>(j, pr, i, j.n);
}

constexpr const char* kBackend = "neon";

#elif defined(AUTODJ_KERNEL_SSE2)

AUTODJ_INLINE __m128 mulAdd(__m128 a, __m128 b, __m128 c) noexcept { return _mm_add_ps(a, _mm_mul_ps(b, c)); }

AUTODJ_INLINE __m128 load2x2(const float* p, const float* q) noexcept {
  return _mm_loadh_pi(_mm_loadl_pi(_mm_setzero_ps(), reinterpret_cast<const __m64*>(p)),
                      reinterpret_cast<const __m64*>(q));
}

AUTODJ_INLINE __m128 hermiteSse(__m128 xm1, __m128 x0, __m128 x1, __m128 x2, __m128 f) noexcept {
  const __m128 half = _mm_set1_ps(0.5f);
  const __m128 c1 = _mm_mul_ps(half, _mm_sub_ps(x1, xm1));
  __m128 c2 = mulAdd(xm1, x0, _mm_set1_ps(-2.5f));
  c2 = mulAdd(c2, x1, _mm_set1_ps(2.0f));
  c2 = mulAdd(c2, x2, _mm_set1_ps(-0.5f));
  __m128 c3 = _mm_mul_ps(half, _mm_sub_ps(x2, xm1));
  c3 = mulAdd(c3, _mm_sub_ps(x0, x1), _mm_set1_ps(1.5f));
  __m128 y = mulAdd(c2, c3, f);
  y = mulAdd(c1, y, f);
  return mulAdd(x0, y, f);
}

template <bool Hermite>
void renderSse2(const GrainJob& j) noexcept {
  const Prepared pr = prepare(j);
  const __m128 k0123 = _mm_set_ps(3.f, 2.f, 1.f, 0.f);
  const __m128 vrate = _mm_set1_ps(j.rate);
  const __m128 vestep = _mm_set1_ps(j.envStep);
  const __m128 vgL = _mm_set1_ps(j.gainL);
  const __m128 vgR = _mm_set1_ps(j.gainR);
  int i = 0;
  for (; i + 4 <= j.n; i += 4) {
    const __m128 pv = mulAdd(_mm_set1_ps(pr.f0 + static_cast<float>(i) * j.rate), k0123, vrate);
    const __m128i iv = _mm_cvttps_epi32(pv);
    const __m128 fv = _mm_sub_ps(pv, _mm_cvtepi32_ps(iv));
    const __m128 ev = mulAdd(_mm_set1_ps(j.envPos + static_cast<float>(i) * j.envStep), k0123, vestep);
    const __m128i eiv = _mm_cvttps_epi32(ev);
    const __m128 efv = _mm_sub_ps(ev, _mm_cvtepi32_ps(eiv));
    alignas(16) std::int32_t ia[4];
    alignas(16) std::int32_t ea[4];
    _mm_store_si128(reinterpret_cast<__m128i*>(ia), iv);
    _mm_store_si128(reinterpret_cast<__m128i*>(ea), eiv);

    const __m128 e01 = load2x2(j.env + ea[0], j.env + ea[1]);  // a0 b0 a1 b1
    const __m128 e23 = load2x2(j.env + ea[2], j.env + ea[3]);  // a2 b2 a3 b3
    const __m128 elo = _mm_shuffle_ps(e01, e23, _MM_SHUFFLE(2, 0, 2, 0));
    const __m128 ehi = _mm_shuffle_ps(e01, e23, _MM_SHUFFLE(3, 1, 3, 1));
    const __m128 w = mulAdd(elo, _mm_sub_ps(ehi, elo), efv);
    const __m128 wl = _mm_mul_ps(w, vgL);
    const __m128 wr = _mm_mul_ps(w, vgR);
    const __m128 w01 = _mm_unpacklo_ps(wl, wr);
    const __m128 w23 = _mm_unpackhi_ps(wl, wr);
    const __m128 f01 = _mm_unpacklo_ps(fv, fv);
    const __m128 f23 = _mm_unpackhi_ps(fv, fv);

    const float* p0 = pr.s + 2 * ia[0];
    const float* p1 = pr.s + 2 * ia[1];
    const float* p2 = pr.s + 2 * ia[2];
    const float* p3 = pr.s + 2 * ia[3];

    __m128 y01, y23;
    if constexpr (Hermite) {
      y01 = hermiteSse(load2x2(p0 - 2, p1 - 2), load2x2(p0, p1), load2x2(p0 + 2, p1 + 2), load2x2(p0 + 4, p1 + 4), f01);
      y23 = hermiteSse(load2x2(p2 - 2, p3 - 2), load2x2(p2, p3), load2x2(p2 + 2, p3 + 2), load2x2(p2 + 4, p3 + 4), f23);
    } else {
      const __m128 a01 = load2x2(p0, p1);
      const __m128 b01 = load2x2(p0 + 2, p1 + 2);
      const __m128 a23 = load2x2(p2, p3);
      const __m128 b23 = load2x2(p2 + 2, p3 + 2);
      y01 = mulAdd(a01, _mm_sub_ps(b01, a01), f01);
      y23 = mulAdd(a23, _mm_sub_ps(b23, a23), f23);
    }
    float* o = j.out + 2 * i;
    _mm_storeu_ps(o, mulAdd(_mm_loadu_ps(o), y01, w01));
    _mm_storeu_ps(o + 4, mulAdd(_mm_loadu_ps(o + 4), y23, w23));
  }
  if (i < j.n) renderScalarRange<Hermite>(j, pr, i, j.n);
}

constexpr const char* kBackend = "sse2";

#else

constexpr const char* kBackend = "scalar";

#endif

}  // namespace

void renderGrainScalarLinear(const GrainJob& job) noexcept {
  renderScalarRange<false>(job, prepare(job), 0, job.n);
}

void renderGrainScalarHermite(const GrainJob& job) noexcept {
  renderScalarRange<true>(job, prepare(job), 0, job.n);
}

GrainKernelFn selectKernel(Interp interp, bool simd) noexcept {
  const bool hermite = interp == Interp::Hermite;
#if defined(AUTODJ_KERNEL_NEON)
  if (simd) return hermite ? &renderNeon<true> : &renderNeon<false>;
#elif defined(AUTODJ_KERNEL_SSE2)
  if (simd) return hermite ? &renderSse2<true> : &renderSse2<false>;
#else
  (void)simd;
#endif
  return hermite ? &renderGrainScalarHermite : &renderGrainScalarLinear;
}

const char* kernelBackendName() noexcept { return kBackend; }

}  // namespace autodj::granular
