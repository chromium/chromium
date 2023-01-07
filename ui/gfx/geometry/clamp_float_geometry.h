// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_CLAMP_FLOAT_GEOMETRY_H_
#define UI_GFX_GEOMETRY_CLAMP_FLOAT_GEOMETRY_H_

#include <limits>

#include "base/numerics/safe_conversions.h"

namespace gfx {

template <typename T>
struct FloatGeometrySaturationHandler {
  static constexpr float NaN() { return 0; }
  static constexpr float Overflow() { return max(); }
  static constexpr float Underflow() { return lowest(); }
  static constexpr float max() {
    return std::numeric_limits<float>::max() / 1e6;
  }
  static constexpr float lowest() {
    return std::numeric_limits<float>::lowest() / 1e6;
  }
};

// Clamps |value| (float, double or long double) within the range of
// [numeric_limits<float>::lowest() / 1e6, numeric_limits<float::max() / 1e6f].
// Returns 0 for NaN. This avoids NaN and infinity values immediately, and
// reduce the chance of producing NaN and infinity values for future unclamped
// operations like offsetting and scaling by devices / page scale factor.
template <typename T>
constexpr float ClampFloatGeometry(T value) {
  return base::saturated_cast<float, FloatGeometrySaturationHandler, T>(value);
}

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_CLAMP_FLOAT_GEOMETRY_H_
