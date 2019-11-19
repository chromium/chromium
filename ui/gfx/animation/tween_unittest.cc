// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/tween.h"

#include <math.h>

#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/test/gfx_util.h"

#if defined(OS_WIN)
#include <float.h>
#endif

namespace gfx {
namespace {

double next_double(double d) {
#if defined(OS_WIN)
  return _nextafter(d, d + 1);
#else
  // Step two units of least precision towards positive infinity. On some 32
  // bit x86 compilers a single step was not enough due to loss of precision in
  // optimized code.
  return nextafter(nextafter(d, d + 1), d + 1);
#endif
}

// Validates that the same interpolations are made as in Blink.
TEST(TweenTest, ColorValueBetween) {
  // From blink's AnimatableColorTest.
  EXPECT_SKCOLOR_EQ(0xFF00FF00,
                  Tween::ColorValueBetween(-10.0, 0xFF00FF00, 0xFF00FF00));
  EXPECT_SKCOLOR_EQ(0xFF00FF00,
                  Tween::ColorValueBetween(-10.0, 0xFF00FF00, 0xFFFF00FF));
  EXPECT_SKCOLOR_EQ(0xFF00FF00,
                  Tween::ColorValueBetween(0.0, 0xFF00FF00, 0xFFFF00FF));
  EXPECT_SKCOLOR_EQ(0xFF01FE01,
                  Tween::ColorValueBetween(1.0 / 255, 0xFF00FF00, 0xFFFF00FF));
  EXPECT_SKCOLOR_EQ(0xFF808080,
                  Tween::ColorValueBetween(0.5, 0xFF00FF00, 0xFFFF00FF));
  EXPECT_SKCOLOR_EQ(
      0xFFFE01FE,
      Tween::ColorValueBetween(254.0 / 255.0, 0xFF00FF00, 0xFFFF00FF));
  EXPECT_SKCOLOR_EQ(0xFFFF00FF,
                  Tween::ColorValueBetween(1.0, 0xFF00FF00, 0xFFFF00FF));
  EXPECT_SKCOLOR_EQ(0xFFFF00FF,
                  Tween::ColorValueBetween(10.0, 0xFF00FF00, 0xFFFF00FF));
  EXPECT_SKCOLOR_EQ(0xFF0C253E,
                  Tween::ColorValueBetween(3.0 / 16.0, 0xFF001020, 0xFF4080C0));
  EXPECT_SKCOLOR_EQ(0x80FF00FF,
                  Tween::ColorValueBetween(0.5, 0x0000FF00, 0xFFFF00FF));
  EXPECT_SKCOLOR_EQ(0x60AA55AA,
                  Tween::ColorValueBetween(0.5, 0x4000FF00, 0x80FF00FF));
  EXPECT_SKCOLOR_EQ(0x60FFAAFF,
                  Tween::ColorValueBetween(0.5, 0x40FF00FF, 0x80FFFFFF));
  EXPECT_SKCOLOR_EQ(0x103060A0,
                  Tween::ColorValueBetween(0.5, 0x10204080, 0x104080C0));
}

// Ensures that each of the 3 integers in [0, 1, 2] ae selected with equal
// weight.
TEST(TweenTest, IntValueBetween) {
  EXPECT_EQ(0, Tween::IntValueBetween(0.0, 0, 2));
  EXPECT_EQ(0, Tween::IntValueBetween(0.5 / 3.0, 0, 2));
  EXPECT_EQ(0, Tween::IntValueBetween(1.0 / 3.0, 0, 2));

  EXPECT_EQ(1, Tween::IntValueBetween(next_double(1.0 / 3.0), 0, 2));
  EXPECT_EQ(1, Tween::IntValueBetween(1.5 / 3.0, 0, 2));
  EXPECT_EQ(1, Tween::IntValueBetween(2.0 / 3.0, 0, 2));

  EXPECT_EQ(2, Tween::IntValueBetween(next_double(2.0 / 3.0), 0, 2));
  EXPECT_EQ(2, Tween::IntValueBetween(2.5 / 3.0, 0, 2));
  EXPECT_EQ(2, Tween::IntValueBetween(3.0 / 3.0, 0, 2));
}

TEST(TweenTest, IntValueBetweenNegative) {
  EXPECT_EQ(-2, Tween::IntValueBetween(0.0, -2, 0));
  EXPECT_EQ(-2, Tween::IntValueBetween(0.5 / 3.0, -2, 0));
  EXPECT_EQ(-2, Tween::IntValueBetween(1.0 / 3.0, -2, 0));

  EXPECT_EQ(-1, Tween::IntValueBetween(next_double(1.0 / 3.0), -2, 0));
  EXPECT_EQ(-1, Tween::IntValueBetween(1.5 / 3.0, -2, 0));
  EXPECT_EQ(-1, Tween::IntValueBetween(2.0 / 3.0, -2, 0));

  EXPECT_EQ(0, Tween::IntValueBetween(next_double(2.0 / 3.0), -2, 0));
  EXPECT_EQ(0, Tween::IntValueBetween(2.5 / 3.0, -2, 0));
  EXPECT_EQ(0, Tween::IntValueBetween(3.0 / 3.0, -2, 0));
}

TEST(TweenTest, IntValueBetweenReverse) {
  EXPECT_EQ(2, Tween::IntValueBetween(0.0, 2, 0));
  EXPECT_EQ(2, Tween::IntValueBetween(0.5 / 3.0, 2, 0));
  EXPECT_EQ(2, Tween::IntValueBetween(1.0 / 3.0, 2, 0));

  EXPECT_EQ(1, Tween::IntValueBetween(next_double(1.0 / 3.0), 2, 0));
  EXPECT_EQ(1, Tween::IntValueBetween(1.5 / 3.0, 2, 0));
  EXPECT_EQ(1, Tween::IntValueBetween(2.0 / 3.0, 2, 0));

  EXPECT_EQ(0, Tween::IntValueBetween(next_double(2.0 / 3.0), 2, 0));
  EXPECT_EQ(0, Tween::IntValueBetween(2.5 / 3.0, 2, 0));
  EXPECT_EQ(0, Tween::IntValueBetween(3.0 / 3.0, 2, 0));
}

TEST(TweenTest, LinearIntValueBetween) {
  EXPECT_EQ(0, Tween::LinearIntValueBetween(0.0, 0, 2));
  EXPECT_EQ(0, Tween::LinearIntValueBetween(0.5 / 4.0, 0, 2));
  EXPECT_EQ(0, Tween::LinearIntValueBetween(0.99 / 4.0, 0, 2));

  EXPECT_EQ(1, Tween::LinearIntValueBetween(1.0 / 4.0, 0, 2));
  EXPECT_EQ(1, Tween::LinearIntValueBetween(1.5 / 4.0, 0, 2));
  EXPECT_EQ(1, Tween::LinearIntValueBetween(2.0 / 4.0, 0, 2));
  EXPECT_EQ(1, Tween::LinearIntValueBetween(2.5 / 4.0, 0, 2));
  EXPECT_EQ(1, Tween::LinearIntValueBetween(2.99 / 4.0, 0, 2));

  EXPECT_EQ(2, Tween::LinearIntValueBetween(3.0 / 4.0, 0, 2));
  EXPECT_EQ(2, Tween::LinearIntValueBetween(3.5 / 4.0, 0, 2));
  EXPECT_EQ(2, Tween::LinearIntValueBetween(4.0 / 4.0, 0, 2));
}

TEST(TweenTest, LinearIntValueBetweenNegative) {
  EXPECT_EQ(-2, Tween::LinearIntValueBetween(0.0, -2, 0));
  EXPECT_EQ(-2, Tween::LinearIntValueBetween(0.5 / 4.0, -2, 0));
  EXPECT_EQ(-2, Tween::LinearIntValueBetween(0.99 / 4.0, -2, 0));

  EXPECT_EQ(-1, Tween::LinearIntValueBetween(1.0 / 4.0, -2, 0));
  EXPECT_EQ(-1, Tween::LinearIntValueBetween(1.5 / 4.0, -2, 0));
  EXPECT_EQ(-1, Tween::LinearIntValueBetween(2.0 / 4.0, -2, 0));
  EXPECT_EQ(-1, Tween::LinearIntValueBetween(2.5 / 4.0, -2, 0));
  EXPECT_EQ(-1, Tween::LinearIntValueBetween(2.99 / 4.0, -2, 0));

  EXPECT_EQ(0, Tween::LinearIntValueBetween(3.0 / 4.0, -2, 0));
  EXPECT_EQ(0, Tween::LinearIntValueBetween(3.5 / 4.0, -2, 0));
  EXPECT_EQ(0, Tween::LinearIntValueBetween(4.0 / 4.0, -2, 0));
}

TEST(TweenTest, ClampedFloatValueBetweenTimeTicks) {
  const float v1 = 10.0f;
  const float v2 = 20.0f;

  const auto t0 = base::TimeTicks();

  base::TimeTicks from = t0 + base::TimeDelta::FromSecondsD(1);
  base::TimeTicks to = t0 + base::TimeDelta::FromSecondsD(2);

  base::TimeTicks t_before = t0 + base::TimeDelta::FromSecondsD(0.9);
  base::TimeTicks t_between = t0 + base::TimeDelta::FromSecondsD(1.6);
  base::TimeTicks t_after = t0 + base::TimeDelta::FromSecondsD(2.2);

  EXPECT_EQ(v1, Tween::ClampedFloatValueBetween(t_before, from, v1, to, v2));
  EXPECT_EQ(16.0, Tween::ClampedFloatValueBetween(t_between, from, v1, to, v2));
  EXPECT_EQ(v2, Tween::ClampedFloatValueBetween(t_after, from, v1, to, v2));
}

// Verifies the corners of the rect are animated, rather than the origin/size
// (which would result in different rounding).
TEST(TweenTest, RectValueBetween) {
  constexpr gfx::Rect r1(0, 0, 10, 10);
  constexpr gfx::Rect r2(10, 10, 30, 30);

  // TODO(pkasting): Move the geometry test helpers from
  // cc/test/geometry_test_utils.h to ui/gfx/test/gfx_util.h or similar and use
  // a rect-comparison function here.
  const gfx::Rect tweened = Tween::RectValueBetween(0.08, r1, r2);
  EXPECT_EQ(11, tweened.width());
  EXPECT_EQ(11, tweened.height());
}

TEST(TweenTest, SizeValueBetween) {
  constexpr gfx::Size kSize1(12, 24);
  constexpr gfx::Size kSize2(36, 48);

  constexpr double kBefore = -0.125;
  constexpr double kFrom = 0.0;
  constexpr double kBetween = 0.5;
  constexpr double kTo = 1.0;
  constexpr double kAfter = 1.125;

  EXPECT_EQ(gfx::Size(9, 21), Tween::SizeValueBetween(kBefore, kSize1, kSize2));
  EXPECT_EQ(kSize1, Tween::SizeValueBetween(kFrom, kSize1, kSize2));
  EXPECT_EQ(gfx::Size(24, 36),
            Tween::SizeValueBetween(kBetween, kSize1, kSize2));
  EXPECT_EQ(kSize2, Tween::SizeValueBetween(kTo, kSize1, kSize2));
  EXPECT_EQ(gfx::Size(39, 51), Tween::SizeValueBetween(kAfter, kSize1, kSize2));
}

TEST(TweenTest, SizeValueBetweenClampedExtrapolation) {
  constexpr gfx::Size kSize1(0, 0);
  constexpr gfx::Size kSize2(36, 48);

  constexpr double kBefore = -1.0f;

  // We should not extrapolate in this case as it would result in a negative and
  // invalid size.
  EXPECT_EQ(kSize1, Tween::SizeValueBetween(kBefore, kSize1, kSize2));
}

TEST(TweenTest, SizeFValueBetween) {
  const gfx::SizeF s1(12.0f, 24.0f);
  const gfx::SizeF s2(36.0f, 48.0f);

  constexpr double kBefore = -0.125;
  constexpr double kFrom = 0.0;
  constexpr double kBetween = 0.5;
  constexpr double kTo = 1.0;
  constexpr double kAfter = 1.125;

  EXPECT_SIZEF_EQ(gfx::SizeF(9.0f, 21.0f),
                  Tween::SizeFValueBetween(kBefore, s1, s2));
  EXPECT_SIZEF_EQ(s1, Tween::SizeFValueBetween(kFrom, s1, s2));
  EXPECT_SIZEF_EQ(gfx::SizeF(24.0f, 36.0f),
                  Tween::SizeFValueBetween(kBetween, s1, s2));
  EXPECT_SIZEF_EQ(s2, Tween::SizeFValueBetween(kTo, s1, s2));
  EXPECT_SIZEF_EQ(gfx::SizeF(39.0f, 51.0f),
                  Tween::SizeFValueBetween(kAfter, s1, s2));
}

TEST(TweenTest, SizeFValueBetweenClampedExtrapolation) {
  const gfx::SizeF s1(0.0f, 0.0f);
  const gfx::SizeF s2(36.0f, 48.0f);

  constexpr double kBefore = -1.0f;

  // We should not extrapolate in this case as it would result in a negative and
  // invalid size.
  EXPECT_SIZEF_EQ(s1, Tween::SizeFValueBetween(kBefore, s1, s2));
}

}  // namespace
}  // namespace gfx
