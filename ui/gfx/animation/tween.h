// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_TWEEN_H_
#define UI_GFX_ANIMATION_TWEEN_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_export.h"

namespace base {
class TimeTicks;
}

namespace gfx {

class Rect;
class RectF;
class Size;
class SizeF;
class Transform;
class TransformOperations;

class ANIMATION_EXPORT Tween {
 public:
  enum Type {
    LINEAR,       // Linear.
    EASE_OUT,     // Fast in, slow out (default).
    EASE_OUT_2,   // Variant of EASE_OUT that ends slower than EASE_OUT.
    EASE_OUT_3,   // Variant of EASE_OUT that ends slower than EASE_OUT_2.
    EASE_OUT_4,   // Variant of EASE_OUT that start slower than EASE_OUT_3,
                  // and ends faster. Best used to lead into a bounce
                  // animation.
    EASE_IN,      // Slow in, fast out.
    EASE_IN_2,    // Variant of EASE_IN that starts out slower than
                  // EASE_IN.
    EASE_IN_OUT,  // Slow in and out, fast in the middle.
    EASE_IN_OUT_EMPHASIZED,  // Variant of EASE_IN_OUT that starts slowly,
                             // speeds up shortly thereafter, and then ends
                             // slowly. This curve can be imagined as a steeper
                             // version of EASE_IN_OUT.
    EASE_IN_OUT_2,       // Variant of EASE_IN_OUT that starts and ends slower
                         // than EASE_IN_OUT.
    SMOOTH_IN_OUT,       // Smooth, consistent speeds in and out (sine wave).
    FAST_OUT_SLOW_IN,    // Variant of EASE_IN_OUT which should be used in most
                         // cases.
    FAST_OUT_SLOW_IN_2,  // Variant of FAST_OUT_SLOW_IN that starts out quicker.
    FAST_OUT_SLOW_IN_3,  // Variant of FAST_OUT_SLOW_IN that starts out quicker
                         // than FAST_OUT_SLOW_IN_2. Best used for rebound in
                         // bounce animation.
    LINEAR_OUT_SLOW_IN,  // Variant of EASE_OUT which should be used for
                         // fading in from 0% or motion when entering a scene.
    SLOW_OUT_LINEAR_IN,  // Reverse of LINEAR_OUT_SLOW_IN which should be used
                         // in reverse animation to create a rubberband effect.
    FAST_OUT_LINEAR_IN,  // Variant of EASE_IN which should should be used for
                         // fading out to 0% or motion when exiting a scene.
    ZERO,                // Returns a value of 0 always.

    // TODO(zxdan): New animation curve name convention will be used to resolve
    // the confusion caused by "IN" and "OUT".

    // The new name convention is below:
    // ACCEL_<1>_DECEL_<2> where <1> and <2> are used to express the
    // acceleration and deceleration speeds. The corresponding cubic bezier
    // curve parameters would be ( 0.01 * <1>, 0, 1 - 0.01 * <2>, 1 ). Note that
    // LIN means the speed is 0. For example,
    // ACCEL_20_DECEL_20 = (0.2, 0, 0.8, 1): https://cubic-bezier.com/#.2,0,.8,1
    // ACCEL_100_DECEL_100 = (1, 0, 0, 1): https://cubic-bezier.com/#1,0,0,1
    // ACCEL_LIN_DECEL_LIN = (0, 0, 1, 1): https://cubic-bezier.com/#0,0,1,1
    // ACCEL_40_DECEL_20 = (0.4, 0, 0.8, 1): https://cubic-bezier.com/#.4,0,.8,1
    // ACCEL_<1>_<2>_DECEL_<3>_<4> correspond to cubic bezier with curve
    // parameters (0.01 * <1>, 0.01 * <2>, 1 - 0.01 * <3>, 1 - 0.01 * <4>). For
    // example,
    // ACCEL_0_20_DECEL_100_10 = (0, 0.2, 0, 0.9):
    //     https://cubic-bezier.com/#0,.2,0,.9
    // ACCEL_40_DECEL_100_3 = (0.4, 0, 0,0.97):
    //     https://cubic-bezier.com/#.4,0,0,.97
    // ACCEL_LIN_DECEL_100_3 = (0, 0, 0, 0.97):
    //     https://cubic-bezier.com/#0,0,0,.97
    ACCEL_LIN_DECEL_60,   // Pulling a small to medium element into a place.
    ACCEL_LIN_DECEL_100,  // Pulling a small to medium element into a place that
                          // has very fast deceleration.
    // Starts with linear speed and soft deceleration. Use for elements that are
    // not visible at the beginning of a transition, but are visible at the end.
    ACCEL_LIN_DECEL_100_3,
    ACCEL_20_DECEL_60,  // Moving a small, low emphasis or responsive elements.
    ACCEL_20_DECEL_100,
    ACCEL_30_DECEL_20_85,
    ACCEL_40_DECEL_20,
    // Moderate acceleration and soft deceleration. Used for elements that are
    // visible at the beginning and end of a transition.
    ACCEL_40_DECEL_100_3,
    ACCEL_80_DECEL_20,     // Slow in and fast out with ease.
    ACCEL_0_40_DECEL_100,  // Specialized curve with an emphasized deceleration
                           // drift.
    ACCEL_0_80_DECEL_80,   // Variant of ACCEL_0_40_DECEL_100 which drops in
                           // value faster, but flattens out into the drift
                           // sooner.

    ACCEL_0_100_DECEL_80,  // Variant of ACCEL_0_80_DECEL_80 which drops in
                           // value even faster.

    ACCEL_5_70_DECEL_90,  // Start at peak velocity and very soft
                          // deceleration.
  };

  Tween(const Tween&) = delete;
  Tween& operator=(const Tween&) = delete;

  // Returns the value based on the tween type. |state| is from 0-1.
  static double CalculateValue(Type type, double state);

  // Conveniences for getting a value between a start and end point.
  static SkColor4f ColorValueBetween(double value,
                                     SkColor4f start,
                                     SkColor4f target);
  static SkColor ColorValueBetween(double value, SkColor start, SkColor target);
  static double DoubleValueBetween(double value, double start, double target);
  static float FloatValueBetween(double value, float start, float target);
  static float ClampedFloatValueBetween(const base::TimeTicks& time,
                                        const base::TimeTicks& start_time,
                                        float start,
                                        const base::TimeTicks& target_time,
                                        float target);

  // Interpolated between start and target, with every integer in this range
  // given equal weight.
  static int IntValueBetween(double value, int start, int target);

  // Interpolates between start and target as real numbers, and rounds the
  // result to the nearest integer, with ties broken by rounding towards
  // positive infinity. This gives start and target half the weight of the
  // other integers in the range. This is the integer interpolation approach
  // specified by www.w3.org/TR/css3-transitions.
  static int LinearIntValueBetween(double value, int start, int target);

  // Interpolates between |start| and |target| rects, animating the rect corners
  // (as opposed to animating the rect origin and size) to minimize rounding
  // error accumulation at intermediate stages.
  static gfx::Rect RectValueBetween(double value,
                                    const gfx::Rect& start,
                                    const gfx::Rect& target);

  static gfx::RectF RectFValueBetween(double value,
                                      const gfx::RectF& start,
                                      const gfx::RectF& target);

  static gfx::Transform TransformValueBetween(double value,
                                              const gfx::Transform& start,
                                              const gfx::Transform& target);

  static gfx::TransformOperations TransformOperationsValueBetween(
      double value,
      const gfx::TransformOperations& start,
      const gfx::TransformOperations& target);

  static gfx::Size SizeValueBetween(double value,
                                    const gfx::Size& start,
                                    const gfx::Size& target);

  static gfx::SizeF SizeFValueBetween(double value,
                                      const gfx::SizeF& start,
                                      const gfx::SizeF& target);

 private:
  Tween();
  ~Tween();
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_TWEEN_H_
