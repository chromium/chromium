// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef UI_GFX_GEOMETRY_SIN_COS_DEGREES_H_
#define UI_GFX_GEOMETRY_SIN_COS_DEGREES_H_

#include <algorithm>
#include <cmath>
#include <numbers>

#include "base/numerics/angle_conversions.h"

namespace gfx {

struct SinCos {
  double sin;
  double cos;
  bool IsZeroAngle() const { return sin == 0 && cos == 1; }
};

inline SinCos SinCosDegrees(double degrees) {
  // Some math libraries have poor accuracy with large arguments,
  // so range-reduce explicitly before we call sin() or cos(). However, unless
  // we're _really_ large (out of range of an int), we can do that faster than
  // fmod(), since we have an integer divisor (and as an extra bonus, we've
  // already got it precomputed). We pick a pretty arbitrary limit that should
  // be safe.
  //
  // We range-reduce to [0..45]. This should hit the fast path of sincos()
  // on most platforms (since no further reduction is needed; reducing
  // accurately modulo a trancendental can we slow), using only branches that
  // should be possible to do using conditional operations; using a switch
  // instead would be possible, but benchmarked much slower on M1.
  // For platforms that don't use sincos() (e.g., it seems Clang doesn't
  // manage the rewrite on Linux), we also save on having the range reduction
  // done only once.
  if (degrees > -90000000.0 && degrees < 90000000.0) {
    // Make sure 0, 90, 180 and 270 degrees get exact results. (We also have
    // precomputed values for 45, 135, etc., but only as a side effect of using
    // 45 instead of 90, for the benefit of the range reduction algorithm below.
    // The error for e.g. sin(45 degrees) is typically only 1 ulp.)
    double n45degrees = degrees / 45.0;
    int octant = static_cast<int>(n45degrees);
    if (octant == n45degrees) {
      constexpr SinCos kSinCosN45[] = {
          {0, 1},  {std::numbers::sqrt2 / 2, std::numbers::sqrt2 / 2},
          {1, 0},  {std::numbers::sqrt2 / 2, -std::numbers::sqrt2 / 2},
          {0, -1}, {-std::numbers::sqrt2 / 2, -std::numbers::sqrt2 / 2},
          {-1, 0}, {-std::numbers::sqrt2 / 2, std::numbers::sqrt2 / 2}};

      return kSinCosN45[octant & 7];
    }

    if (degrees < 0) {
      // This will cause the range-reduction below to move us
      // into [0..45], as desired, instead of [-45..0].
      --octant;
    }
    degrees -= octant * 45.0;  // Range-reduce to [0..45].

    // Deal with 45..90 the same as 45..0. This also covers
    // 135..180, 225..270 and 315..360, i.e. the odd octants.
    // The relevant trigonometric identities is that
    // sin(90 - a) = cos(a) and vice versa; we do the sin/cos
    // flip below.
    if (octant & 1) {
      degrees = 45.0 - degrees;
    }

    double rad = base::DegToRad(degrees);
    double s = std::sin(rad);
    double c = std::cos(rad);

    // 45..135 and -135..-45 can be moved into the opposite areas
    // simply by flipping the x and y axis (in conjunction with
    // the conversion from CW to CCW done above).
    using std::swap;
    if ((octant + 1) & 2) {
      swap(s, c);
    }

    // For sine, 180..360 (lower half) is the same as 0..180,
    // except negative.
    if (octant & 4) {
      s = -s;
    }

    // For cosine, 90..270 (right half) is the same as -90..90,
    // except negative.
    if ((octant + 2) & 4) {
      c = -c;
    }

    return SinCos{s, c};
  }

  // Slow path for extreme cases.
  degrees = std::fmod(degrees, 360.0);
  double rad = base::DegToRad(degrees);
  return SinCos{std::sin(rad), std::cos(rad)};
}

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_SIN_COS_DEGREES_H_
