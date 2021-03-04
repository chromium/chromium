// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_TWEEN_H_
#define UI_GFX_ANIMATION_TWEEN_H_

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_operations.h"

namespace base {
class TimeTicks;
}

namespace gfx {

class ANIMATION_EXPORT Tween {
 public:
  enum Type {
    LINEAR,            // Linear.
    EASE_OUT,          // Fast in, slow out (default).
    EASE_OUT_2,        // Variant of EASE_OUT that ends slower than EASE_OUT.
    EASE_OUT_3,        // Variant of EASE_OUT that ends slower than EASE_OUT_2.
    EASE_OUT_4,        // Variant of EASE_OUT that start slower than EASE_OUT_3,
                       // and ends faster. Best used to lead into a bounce
                       // animation.
    EASE_IN,           // Slow in, fast out.
    EASE_IN_2,         // Variant of EASE_IN that starts out slower than
                       // EASE_IN.
    EASE_IN_OUT,       // Slow in and out, fast in the middle.
    EASE_IN_OUT_2,     // Variant of EASE_IN_OUT that starts and ends slower
                       // than EASE_IN_OUT.
    SMOOTH_IN_OUT,     // Smooth, consistent speeds in and out (sine wave).
    FAST_OUT_SLOW_IN,  // Variant of EASE_IN_OUT which should be used in most
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
    // ACCEL_40_DECEL_80 = (0.4, 0, 0.2, 1): https://cubic-bezier.com/#.4,0,.2,1
    ACCEL_LIN_DECEL_60,  // Pulling a small to medium element into a place.
    ACCEL_20_DECEL_60,   // Moving a small, low emphasis or responsive elements.
  };

  // Returns the value based on the tween type. |state| is from 0-1.
  static double CalculateValue(Type type, double state);

  // Conveniences for getting a value between a start and end point.
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

  DISALLOW_COPY_AND_ASSIGN(Tween);
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_TWEEN_H_
