#include <gtest/gtest.h>

#include <cmath>

#include "autodj/granular/EnvelopeTables.h"

using namespace autodj::granular;

namespace {
float lerpTable(const float* t, double x) {
  const double p = x * kEnvTableSize;
  const int i = static_cast<int>(p);
  const float f = static_cast<float>(p - i);
  return t[i] + (t[i + 1] - t[i]) * f;
}
}  // namespace

TEST(Envelopes, AllShapesStartAndEndAtExactlyZero) {
  EnvelopeTables tables;
  for (int s = 0; s < static_cast<int>(EnvShape::Count); ++s) {
    const float* t = tables.table(s);
    EXPECT_EQ(t[0], 0.f) << "shape " << s;
    EXPECT_EQ(t[kEnvTableSize], 0.f) << "shape " << s;
    for (int g = kEnvTableSize + 1; g < kEnvTableStride; ++g) EXPECT_EQ(t[g], 0.f) << "guard " << g;
  }
}

TEST(Envelopes, SymmetricBoundedAndPeakAtCenter) {
  EnvelopeTables tables;
  for (int s = 0; s < static_cast<int>(EnvShape::Count); ++s) {
    const float* t = tables.table(s);
    float mx = 0.f;
    for (int i = 0; i <= kEnvTableSize; ++i) {
      EXPECT_GE(t[i], 0.f);
      EXPECT_LE(t[i], 1.0f + 1e-6f);
      EXPECT_NEAR(t[i], t[kEnvTableSize - i], 1e-6f) << "shape " << s << " i " << i;
      mx = std::max(mx, t[i]);
    }
    EXPECT_NEAR(mx, 1.f, 1e-6f) << "shape " << s;
    EXPECT_NEAR(t[kEnvTableSize / 2], 1.f, 1e-6f) << "shape " << s;
  }
}

TEST(Envelopes, RiseMonotonicallyToTheCenter) {
  EnvelopeTables tables;
  for (int s = 0; s < static_cast<int>(EnvShape::Count); ++s) {
    const float* t = tables.table(s);
    for (int i = 1; i <= kEnvTableSize / 2; ++i) EXPECT_GE(t[i], t[i - 1] - 1e-7f) << "shape " << s << " i " << i;
  }
}

TEST(Envelopes, LookupWithLinearInterpolationMatchesTheAnalyticShape) {
  EnvelopeTables tables;
  for (int s = 0; s < static_cast<int>(EnvShape::Count); ++s) {
    const float* t = tables.table(s);
    double worst = 0.0;
    for (int k = 0; k < 20000; ++k) {
      const double x = (k + 0.37) / 20000.0;
      worst = std::max(worst, std::fabs(lerpTable(t, x) - EnvelopeTables::evaluate(static_cast<EnvShape>(s), x)));
    }
    // Hann/Tukey/Gauss are smooth: error ~ h^2 f''/8 ~ 6e-7. Trapezoid/Square/Triangle have kinks (error <= slope * h;
    // the 3 % ramp of Square is the steepest: 1 / (0.03 * 2048) per table entry).
    const bool kinked = s == static_cast<int>(EnvShape::Trapezoid) || s == static_cast<int>(EnvShape::Square) ||
                        s == static_cast<int>(EnvShape::Triangle);
    EXPECT_LT(worst, kinked ? 1e-2 : 4e-6) << "shape " << s;
  }
}

TEST(Envelopes, HannAtHalfOverlapSumsToOne) {
  for (double x = 0.0; x < 0.5; x += 0.01) {
    const double sum = EnvelopeTables::evaluate(EnvShape::Hann, x) + EnvelopeTables::evaluate(EnvShape::Hann, x + 0.5);
    EXPECT_NEAR(sum, 1.0, 1e-12);
  }
}

TEST(Envelopes, TukeyHasFlatTopAndTrapezoidLinearRamps) {
  for (double x = 0.26; x < 0.74; x += 0.02) EXPECT_DOUBLE_EQ(EnvelopeTables::evaluate(EnvShape::Tukey, x), 1.0);
  EXPECT_NEAR(EnvelopeTables::evaluate(EnvShape::Trapezoid, 0.125), 0.5, 1e-12);
  EXPECT_NEAR(EnvelopeTables::evaluate(EnvShape::Trapezoid, 0.875), 0.5, 1e-12);
  EXPECT_DOUBLE_EQ(EnvelopeTables::evaluate(EnvShape::Trapezoid, 0.5), 1.0);
}

TEST(Envelopes, OutOfRangeShapeFallsBackToHann) {
  EnvelopeTables tables;
  EXPECT_EQ(tables.table(-1), tables.table(0));
  EXPECT_EQ(tables.table(99), tables.table(0));
}

TEST(Envelopes, SquareIsFlatWithShortEdgesAndTriangleIsLinear) {
  // Square: reaches full level after 3 % and stays there until the last 3 %.
  EXPECT_NEAR(EnvelopeTables::evaluate(EnvShape::Square, 0.015), 0.5, 1e-12);
  EXPECT_DOUBLE_EQ(EnvelopeTables::evaluate(EnvShape::Square, 0.03), 1.0);
  EXPECT_DOUBLE_EQ(EnvelopeTables::evaluate(EnvShape::Square, 0.5), 1.0);
  EXPECT_NEAR(EnvelopeTables::evaluate(EnvShape::Square, 0.985), 0.5, 1e-12);
  EXPECT_DOUBLE_EQ(EnvelopeTables::evaluate(EnvShape::Square, 0.0), 0.0);
  EXPECT_DOUBLE_EQ(EnvelopeTables::evaluate(EnvShape::Square, 1.0), 0.0);
  // Triangle: x = 0.25 -> 0.5, peak at the center.
  EXPECT_NEAR(EnvelopeTables::evaluate(EnvShape::Triangle, 0.25), 0.5, 1e-12);
  EXPECT_NEAR(EnvelopeTables::evaluate(EnvShape::Triangle, 0.5), 1.0, 1e-12);
  EXPECT_NEAR(EnvelopeTables::evaluate(EnvShape::Triangle, 0.9), 0.2, 1e-12);
}

TEST(Envelopes, ExistingShapeIndicesNeverMove) {
  // Presets store the enum index: appending shapes is fine, renumbering would silently change old presets.
  EXPECT_EQ(static_cast<int>(EnvShape::Hann), 0);
  EXPECT_EQ(static_cast<int>(EnvShape::Gauss), 1);
  EXPECT_EQ(static_cast<int>(EnvShape::Tukey), 2);
  EXPECT_EQ(static_cast<int>(EnvShape::Trapezoid), 3);
  EXPECT_EQ(static_cast<int>(EnvShape::Square), 4);
  EXPECT_EQ(static_cast<int>(EnvShape::Triangle), 5);
  EXPECT_EQ(static_cast<int>(EnvShape::Count), 6);
}
