// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/tween.h"

#include <math.h>
#include <stdint.h>

#include <algorithm>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/cubic_bezier.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"

#if defined(OS_WIN)
#include <float.h>
#endif

namespace gfx {

// static
double Tween::CalculateValue(Tween::Type type, double state) {
  DCHECK_GE(state, 0);
  DCHECK_LE(state, 1);

  switch (type) {
    case EASE_IN:
      return pow(state, 2);

    case EASE_IN_2:
      return pow(state, 4);

    case EASE_IN_OUT:
      if (state < 0.5)
        return pow(state * 2, 2) / 2.0;
      return 1.0 - (pow((state - 1.0) * 2, 2) / 2.0);

    case LINEAR:
      return state;

    case EASE_OUT:
      return 1.0 - pow(1.0 - state, 2);

    case SMOOTH_IN_OUT:
      return sin(state);

    case FAST_OUT_SLOW_IN:
      return gfx::CubicBezier(0.4, 0, 0.2, 1).Solve(state);

    case FAST_OUT_SLOW_IN_2:
      return gfx::CubicBezier(0.2, 0, 0.2, 1).Solve(state);

    case LINEAR_OUT_SLOW_IN:
      return gfx::CubicBezier(0, 0, .2, 1).Solve(state);

    case SLOW_OUT_LINEAR_IN:
      return gfx::CubicBezier(0, 0, 1, .2).Solve(state);

    case FAST_OUT_LINEAR_IN:
      return gfx::CubicBezier(0.4, 0, 1, 1).Solve(state);

    case ZERO:
      return 0;
  }

  NOTREACHED();
  return state;
}

namespace {

uint8_t FloatToColorByte(float f) {
  return base::saturated_cast<uint8_t>(ToRoundedInt(f * 255.f));
}

uint8_t BlendColorComponents(uint8_t start,
                             uint8_t target,
                             float start_alpha,
                             float target_alpha,
                             float blended_alpha,
                             double progress) {
  // Since progress can be outside [0, 1], blending can produce a value outside
  // [0, 255].
  float blended_premultiplied = Tween::FloatValueBetween(
      progress, start / 255.f * start_alpha, target / 255.f * target_alpha);
  return FloatToColorByte(blended_premultiplied / blended_alpha);
}

double TimeDeltaDivide(base::TimeDelta dividend, base::TimeDelta divisor) {
  return static_cast<double>(dividend.InMicroseconds()) /
         static_cast<double>(divisor.InMicroseconds());
}

}  // namespace

// static
SkColor Tween::ColorValueBetween(double value, SkColor start, SkColor target) {
  float start_a = SkColorGetA(start) / 255.f;
  float target_a = SkColorGetA(target) / 255.f;
  float blended_a = FloatValueBetween(value, start_a, target_a);
  if (blended_a <= 0.f)
    return SkColorSetARGB(0, 0, 0, 0);
  blended_a = std::min(blended_a, 1.f);

  uint8_t blended_r =
      BlendColorComponents(SkColorGetR(start), SkColorGetR(target), start_a,
                           target_a, blended_a, value);
  uint8_t blended_g =
      BlendColorComponents(SkColorGetG(start), SkColorGetG(target), start_a,
                           target_a, blended_a, value);
  uint8_t blended_b =
      BlendColorComponents(SkColorGetB(start), SkColorGetB(target), start_a,
                           target_a, blended_a, value);

  return SkColorSetARGB(
      FloatToColorByte(blended_a), blended_r, blended_g, blended_b);
}

// static
double Tween::DoubleValueBetween(double value, double start, double target) {
  return start + (target - start) * value;
}

// static
float Tween::FloatValueBetween(double value, float start, float target) {
  return static_cast<float>(start + (target - start) * value);
}

// static
float Tween::ClampedFloatValueBetween(const base::TimeTicks& time,
                                      const base::TimeTicks& start_time,
                                      float start,
                                      const base::TimeTicks& target_time,
                                      float target) {
  if (time <= start_time)
    return start;
  if (time >= target_time)
    return target;

  double progress =
      TimeDeltaDivide(time - start_time, target_time - start_time);
  return FloatValueBetween(progress, start, target);
}

// static
int Tween::IntValueBetween(double value, int start, int target) {
  if (start == target)
    return start;
  double delta = static_cast<double>(target - start);
  if (delta < 0)
    delta--;
  else
    delta++;
#if defined(OS_WIN)
  return start + static_cast<int>(value * _nextafter(delta, 0));
#else
  return start + static_cast<int>(value * nextafter(delta, 0));
#endif
}

// static
int Tween::LinearIntValueBetween(double value, int start, int target) {
  // NOTE: Do not use ToRoundedInt()!  See comments on function declaration.
  return ToFlooredInt(0.5 + DoubleValueBetween(value, start, target));
}

// static
gfx::Rect Tween::RectValueBetween(double value,
                                  const gfx::Rect& start,
                                  const gfx::Rect& target) {
  const int x = LinearIntValueBetween(value, start.x(), target.x());
  const int y = LinearIntValueBetween(value, start.y(), target.y());
  const int right = LinearIntValueBetween(value, start.right(), target.right());
  const int bottom =
      LinearIntValueBetween(value, start.bottom(), target.bottom());
  return gfx::Rect(x, y, right - x, bottom - y);
}

// static
gfx::RectF Tween::RectFValueBetween(double value,
                                    const gfx::RectF& start,
                                    const gfx::RectF& target) {
  const float x = FloatValueBetween(value, start.x(), target.x());
  const float y = FloatValueBetween(value, start.y(), target.y());
  const float right = FloatValueBetween(value, start.right(), target.right());
  const float bottom =
      FloatValueBetween(value, start.bottom(), target.bottom());
  return gfx::RectF(x, y, right - x, bottom - y);
}

// static
gfx::Transform Tween::TransformValueBetween(double value,
                                            const gfx::Transform& start,
                                            const gfx::Transform& target) {
  if (value >= 1.0)
    return target;
  if (value <= 0.0)
    return start;

  gfx::Transform to_return = target;
  to_return.Blend(start, value);
  return to_return;
}

gfx::Size Tween::SizeValueBetween(double value,
                                  const gfx::Size& start,
                                  const gfx::Size& target) {
  return gfx::Size(
      Tween::LinearIntValueBetween(value, start.width(), target.width()),
      Tween::LinearIntValueBetween(value, start.height(), target.height()));
}

gfx::SizeF Tween::SizeFValueBetween(double value,
                                    const gfx::SizeF& start,
                                    const gfx::SizeF& target) {
  return gfx::SizeF(
      Tween::FloatValueBetween(value, start.width(), target.width()),
      Tween::FloatValueBetween(value, start.height(), target.height()));
}

}  // namespace gfx
