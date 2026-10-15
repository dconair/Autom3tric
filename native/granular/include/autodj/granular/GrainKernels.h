#pragma once
// Grain inner loops: read the source with interpolation, multiply by the envelope LUT and the
// per-grain stereo gains, accumulate into an interleaved stereo bus.
//
// Contract (enforced by the scheduler, not checked here):
//   every source index touched lies inside the guard-padded SampleBuffer:
//   linear   reads  [floor(p), floor(p) + 1]
//   hermite  reads  [floor(p) - 1, floor(p) + 2]
//   the envelope LUT is indexed with envPos + i * envStep, never beyond kEnvTableSize + 2.
//
// Backends: AArch64 NEON (4 frames per iteration), x86-64 SSE2 (same structure, for the emulator
// and host benchmarks) and a scalar reference that is also used for the < 4 frame tail.

#include <cstdint>

#include "autodj/core/Platform.h"

namespace autodj::granular {

struct GrainJob {
  const float* src;    // frame 0 of the source (interleaved L,R), guard-padded on both sides
  double pos;          // source position in frames of the first sample rendered by this job
  float rate;          // source frames per output sample (negative = reversed)
  float envPos;        // envelope LUT position of the first sample
  float envStep;       // LUT entries per output sample
  const float* env;    // envelope table (kEnvTableStride entries)
  float gainL;
  float gainR;
  int n;               // number of output frames, >= 1
  float* out;          // interleaved stereo accumulator, n frames
};

enum class Interp : int { Linear = 0, Hermite = 1 };

using GrainKernelFn = void (*)(const GrainJob&) noexcept;

/// `simd = false` forces the scalar reference implementation (tests, A/B benchmarks).
GrainKernelFn selectKernel(Interp interp, bool simd = true) noexcept;

/// "neon", "sse2" or "scalar" - what selectKernel(.., true) resolves to in this build.
const char* kernelBackendName() noexcept;

void renderGrainScalarLinear(const GrainJob& job) noexcept;
void renderGrainScalarHermite(const GrainJob& job) noexcept;

}  // namespace autodj::granular
