// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_types_util.h"

#include <cmath>

namespace display {

namespace {

// The epsilon used to compare the equality of two floats, e.g. display mode
// refresh rate.
constexpr float kEpsilonVal = 0.0001f;

}  // namespace

bool IsWithinEpsilon(float a, float b) {
  return std::abs(a - b) < kEpsilonVal;
}

}  // namespace display
