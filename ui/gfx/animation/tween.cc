// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/tween.h"

#include <math.h>
#include <stdint.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/cubic_bezier.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/three_point_cubic_bezier.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_operations.h"

#if BUILDFLAG(IS_WIN)
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

    case EASE_IN_OUT_EMPHASIZED:
      return gfx::ThreePointCubicBezier(0.05, 0, 0.133333, 0.06, 0.166666, 0.4,
                                        0.208333, 0.82, 0.25, 1)
          .Solve(state);

    case EASE_IN_OUT_2:
      return gfx::CubicBezier(0.33, 0, 0.67, 1).Solve(state);

    case EASE_OUT_3:
      return gfx::CubicBezier(0.6, 0, 0, 1).Solve(state);

    case EASE_OUT_4:
      return gfx::CubicBezier(1, 0, 0.8, 1).Solve(state);

    case LINEAR:
      return state;

    case EASE_OUT:
      return 1.0 - pow(1.0 - state, 2);

    case EASE_OUT_2:
      return gfx::CubicBezier(0.4, 0, 0, 1).Solve(state);

    case SMOOTH_IN_OUT:
      return sin(state);

    case FAST_OUT_SLOW_IN:
      return gfx::CubicBezier(0.4, 0, 0.2, 1).Solve(state);

    case FAST_OUT_SLOW_IN_2:
      return gfx::CubicBezier(0.2, 0, 0.2, 1).Solve(state);

    case FAST_OUT_SLOW_IN_3:
      return gfx::CubicBezier(0.2, 0, 0, 1).Solve(state);

    case LINEAR_OUT_SLOW_IN:
      return gfx::CubicBezier(0, 0, .2, 1).Solve(state);

    case SLOW_OUT_LINEAR_IN:
      return gfx::CubicBezier(0, 0, 1, .2).Solve(state);

    case FAST_OUT_LINEAR_IN:
      return gfx::CubicBezier(0.4, 0, 1, 1).Solve(state);

    case ZERO:
      return 0;

    case ACCEL_LIN_DECEL_60:
      return gfx::CubicBezier(0, 0, 0.4, 1).Solve(state);

    case ACCEL_LIN_DECEL_100:
      return gfx::CubicBezier(0, 0, 0, 1).Solve(state);

    case ACCEL_LIN_DECEL_100_3:
      return gfx::CubicBezier(0, 0, 0, 0.97).Solve(state);

    case ACCEL_20_DECEL_60:
      return gfx::CubicBezier(0.2, 0, 0.4, 1).Solve(state);

    case ACCEL_20_DECEL_100:
      return gfx::CubicBezier(0.2, 0, 0, 1).Solve(state);

    case ACCEL_30_DECEL_20_85:
      return gfx::CubicBezier(0.3, 0, 0.8, 0.15).Solve(state);

    case ACCEL_40_DECEL_20:
      return gfx::CubicBezier(0.4, 0, 0.8, 1).Solve(state);

    case ACCEL_80_DECEL_20:
      return gfx::CubicBezier(0.8, 0, 0.8, 1).Solve(state);

    case ACCEL_0_40_DECEL_100:
      return gfx::CubicBezier(0, 0.4, 0, 1).Solve(state);

    case ACCEL_40_DECEL_100_3:
      return gfx::CubicBezier(0.40, 0, 0, 0.97).Solve(state);

    case ACCEL_0_80_DECEL_80:
      return gfx::CubicBezier(0, 0.8, 0.2, 1).Solve(state);

    case ACCEL_0_100_DECEL_80:
      return gfx::CubicBezier(0, 1, 0.2, 1).Solve(state);

    case ACCEL_5_70_DECEL_90:
      return gfx::CubicBezier(0.05, 0.7, 0.1, 1).Solve(state);
  }

  NOTREACHED_IN_MIGRATION();
  return state;
}

namespace {

uint8_t FloatToColorByte(float f) {
  return base::ClampRound<uint8_t>(f * 255.0f);
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

float BlendColorComponentsFloat(float start,
                                float target,
                                float start_alpha,
                                float target_alpha,
                                float blended_alpha,
                                double progress) {
  // Since progress can be outside [0, 1], blending can produce a value outside
  // [0, 1].
  float blended_premultiplied = Tween::FloatValueBetween(
      progress, start * start_alpha, target * target_alpha);
  return blended_premultiplied / blended_alpha;
}

}  // namespace

// static
SkColor4f Tween::ColorValueBetween(double value,
                                   SkColor4f start,
                                   SkColor4f target) {
  float start_a = start.fA;
  float target_a = target.fA;
  float blended_a = FloatValueBetween(value, start_a, target_a);
  if (blended_a <= 0.f)
    return SkColors::kTransparent;
  blended_a = std::min(blended_a, 1.f);

  auto blended_r = BlendColorComponentsFloat(start.fR, target.fR, start_a,
                                             target_a, blended_a, value);
  auto blended_g = BlendColorComponentsFloat(start.fG, target.fG, start_a,
                                             target_a, blended_a, value);
  auto blended_b = BlendColorComponentsFloat(start.fB, target.fB, start_a,
                                             target_a, blended_a, value);

  return SkColor4f{blended_r, blended_g, blended_b, blended_a};
}
SkColor Tween::ColorValueBetween(double value, SkColor start, SkColor target) {
  float start_a = SkColorGetA(start) / 255.f;
  float target_a = SkColorGetA(target) / 255.f;
  float blended_a = FloatValueBetween(value, start_a, target_a);
  if (blended_a <= 0.f)
    return SK_ColorTRANSPARENT;
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

  return SkColorSetARGB(FloatToColorByte(blended_a), blended_r, blended_g,
                        blended_b);
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

  const double progress = (time - start_time) / (target_time - start_time);
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
#if BUILDFLAG(IS_WIN)
  return start + static_cast<int>(value * _nextafter(delta, 0));
#else
  return start + static_cast<int>(value * nextafter(delta, 0));
#endif
}

// static
int Tween::LinearIntValueBetween(double value, int start, int target) {
  // NOTE: Do not use base::ClampRound()!  See comments on function declaration.
  return base::ClampFloor(0.5 + DoubleValueBetween(value, start, target));
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

// static
gfx::TransformOperations Tween::TransformOperationsValueBetween(
    double value,
    const gfx::TransformOperations& start,
    const gfx::TransformOperations& target) {
  return target.Blend(start, value);
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
