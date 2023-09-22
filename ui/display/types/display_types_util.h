// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_DISPLAY_TYPES_UTIL_H_
#define UI_DISPLAY_TYPES_DISPLAY_TYPES_UTIL_H_

#include "ui/display/types/display_types_export.h"

namespace display {

// Return true if two floats are close enough, e.g. compare the equality of two
// display mode refresh rates.
DISPLAY_TYPES_EXPORT bool IsWithinEpsilon(float a, float b);

}  // namespace display

#endif  // UI_DISPLAY_TYPES_DISPLAY_TYPES_UTIL_H_
