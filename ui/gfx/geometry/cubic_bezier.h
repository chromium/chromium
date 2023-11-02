// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_CUBIC_BEZIER_H_
#define UI_GFX_GEOMETRY_CUBIC_BEZIER_H_

#include "ui/gfx/geometry/geometry_export.h"

namespace gfx {

#define CUBIC_BEZIER_SPLINE_SAMPLES 11

class GEOMETRY_EXPORT CubicBezier {
 public:
  CubicBezier(double p1x, double p1y, double p2x, double p2y);
  CubicBezier(const CubicBezier& other);

  CubicBezier& operator=(const CubicBezier&) = delete;

  double SampleCurveX(double t) const {
    // `ax t^3 + bx t^2 + cx t' expanded using Horner's rule.
    // The x values are in the range [0, 1]. So it isn't needed toFinite
    // clamping.
    // https://drafts.csswg.org/css-easing-1/#funcdef-cubic-bezier-easing-function-cubic-bezier
    return ((ax_ * t + bx_) * t + cx_) * t;
  }

  double SampleCurveY(double t) const {
    return ToFinite(((ay_ * t + by_) * t + cy_) * t);
  }

  double SampleCurveDerivativeX(double t) const {
    return (3.0 * ax_ * t + 2.0 * bx_) * t + cx_;
  }

  double SampleCurveDerivativeY(double t) const {
    return ToFinite(
        ToFinite(ToFinite(3.0 * ay_) * t + ToFinite(2.0 * by_)) * t + cy_);
  }

  static double GetDefaultEpsilon();

  // Given an x value, find a parametric value it came from.
  // x must be in [0, 1] range. Doesn't use gradients.
  double SolveCurveX(double x, double epsilon) const;

  // Evaluates y at the given x with default epsilon.
  double Solve(double x) const;
  // Evaluates y at the given x. The epsilon parameter provides a hint as to the
  // required accuracy and is not guaranteed. Uses gradients if x is
  // out of [0, 1] range.
  double SolveWithEpsilon(double x, double epsilon) const {
    if (x < 0.0)
      return ToFinite(0.0 + start_gradient_ * x);
    if (x > 1.0)
      return ToFinite(1.0 + end_gradient_ * (x - 1.0));
    return SampleCurveY(SolveCurveX(x, epsilon));
  }

  // Returns an approximation of dy/dx at the given x with default epsilon.
  double Slope(double x) const;
  // Returns an approximation of dy/dx at the given x.
  // Clamps x to range [0, 1].
  double SlopeWithEpsilon(double x, double epsilon) const;

  // These getters are used rarely. We reverse compute them from coefficients.
  // See CubicBezier::InitCoefficients. The speed has been traded for memory.
  double GetX1() const;
  double GetY1() const;
  double GetX2() const;
  double GetY2() const;

  // Gets the bezier's minimum y value in the interval [0, 1].
  double range_min() const { return range_min_; }
  // Gets the bezier's maximum y value in the interval [0, 1].
  double range_max() const { return range_max_; }

 private:
  void InitCoefficients(double p1x, double p1y, double p2x, double p2y);
  void InitGradients(double p1x, double p1y, double p2x, double p2y);
  void InitRange(double p1y, double p2y);
  void InitSpline();
  static double ToFinite(double value);

  double ax_;
  double bx_;
  double cx_;

  double ay_;
  double by_;
  double cy_;

  double start_gradient_;
  double end_gradient_;

  double range_min_;
  double range_max_;

  double spline_samples_[CUBIC_BEZIER_SPLINE_SAMPLES];

#ifndef NDEBUG
  // Guard against attempted to solve for t given x in the event that the curve
  // may have multiple values for t for some values of x in [0, 1].
  bool monotonically_increasing_;
#endif
};

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_CUBIC_BEZIER_H_
