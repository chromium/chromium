// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_GAMMA_RAMP_RGB_ENTRY_H_
#define UI_DISPLAY_TYPES_GAMMA_RAMP_RGB_ENTRY_H_

#include <stdint.h>

#include "ui/display/types/display_types_export.h"

namespace display {

// Provides a single entry for a gamma correction table in a GPU.
// Each color component is UNORM16 fixed point format. This means each component
// must be in the range [0, 2^16) and 0xffff represents the maximum value for
// the color component.
struct DISPLAY_TYPES_EXPORT GammaRampRGBEntry {
  uint16_t r;
  uint16_t g;
  uint16_t b;
};

}  // namespace display

#endif  // UI_DISPLAY_TYPES_GAMMA_RAMP_RGB_ENTRY_H_
