#include "autodj/analysis/PeakPyramid.h"

#include <algorithm>
#include <cmath>

namespace autodj::analysis {

PeakPyramid::PeakPyramid(const SampleBuffer& buffer) : frames_(buffer.frames()) {
  if (frames_ <= 0) return;
  const float* d = buffer.data();
  const std::int64_t blocks0 = (frames_ + kBlock - 1) / kBlock;
  std::vector<float> l0(static_cast<std::size_t>(blocks0) * 2);
  for (std::int64_t b = 0; b < blocks0; ++b) {
    const std::int64_t a = b * kBlock;
    const std::int64_t e = std::min(frames_, a + kBlock);
    float lo = 0.5f * (d[2 * a] + d[2 * a + 1]);
    float hi = lo;
    for (std::int64_t i = a + 1; i < e; ++i) {
      const float m = 0.5f * (d[2 * i] + d[2 * i + 1]);
      lo = std::min(lo, m);
      hi = std::max(hi, m);
    }
    l0[static_cast<std::size_t>(2 * b)] = lo;
    l0[static_cast<std::size_t>(2 * b + 1)] = hi;
  }
  levels_.push_back(std::move(l0));
  while (levels_.back().size() / 2 > 16) {
    const std::vector<float>& prev = levels_.back();
    const std::size_t pn = prev.size() / 2;
    const std::size_t n = (pn + kFactor - 1) / kFactor;
    std::vector<float> next(n * 2);
    for (std::size_t i = 0; i < n; ++i) {
      float lo = prev[2 * i * kFactor];
      float hi = prev[2 * i * kFactor + 1];
      for (std::size_t j = 1; j < static_cast<std::size_t>(kFactor); ++j) {
        const std::size_t k = i * kFactor + j;
        if (k >= pn) break;
        lo = std::min(lo, prev[2 * k]);
        hi = std::max(hi, prev[2 * k + 1]);
      }
      next[2 * i] = lo;
      next[2 * i + 1] = hi;
    }
    levels_.push_back(std::move(next));
  }
}

void PeakPyramid::query(const SampleBuffer& buffer, std::int64_t startFrame, std::int64_t endFrame, int columns, float* out) const {
  if (columns <= 0 || out == nullptr) return;
  const double span = static_cast<double>(endFrame - startFrame);
  const double perColumn = span / columns;
  const float* d = buffer.data();
  const std::int64_t frames = buffer.frames();

  // pick the coarsest level whose block is not larger than a column
  int level = -1;
  std::int64_t blockFrames = kBlock;
  for (std::size_t l = 0; l < levels_.size(); ++l) {
    if (static_cast<double>(blockFrames) <= perColumn) level = static_cast<int>(l);
    blockFrames *= kFactor;
  }
  blockFrames = kBlock;
  for (int l = 0; l < level; ++l) blockFrames *= kFactor;

  for (int c = 0; c < columns; ++c) {
    const std::int64_t a = startFrame + static_cast<std::int64_t>(std::floor(perColumn * c));
    std::int64_t b = startFrame + static_cast<std::int64_t>(std::floor(perColumn * (c + 1)));
    if (b <= a) b = a + 1;
    float lo = 0.f, hi = 0.f;
    bool any = false;
    if (level < 0) {
      for (std::int64_t i = std::max<std::int64_t>(a, 0); i < std::min(b, frames); ++i) {
        const float m = 0.5f * (d[2 * i] + d[2 * i + 1]);
        if (!any) { lo = hi = m; any = true; }
        lo = std::min(lo, m);
        hi = std::max(hi, m);
      }
    } else {
      const std::vector<float>& lv = levels_[static_cast<std::size_t>(level)];
      const std::int64_t n = static_cast<std::int64_t>(lv.size() / 2);
      std::int64_t ba = std::max<std::int64_t>(0, a / blockFrames);
      std::int64_t bb = std::min<std::int64_t>(n, (b + blockFrames - 1) / blockFrames);
      for (std::int64_t k = ba; k < bb; ++k) {
        const float l = lv[static_cast<std::size_t>(2 * k)], h = lv[static_cast<std::size_t>(2 * k + 1)];
        if (!any) { lo = l; hi = h; any = true; }
        lo = std::min(lo, l);
        hi = std::max(hi, h);
      }
    }
    out[2 * c] = lo;
    out[2 * c + 1] = hi;
  }
}

}  // namespace autodj::analysis
