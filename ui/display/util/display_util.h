// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_UTIL_DISPLAY_UTIL_H_
#define UI_DISPLAY_UTIL_DISPLAY_UTIL_H_

#include <stdint.h>

#include "ui/display/util/display_util_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace display {

class EdidParser;

// 1 inch in mm.
constexpr float kInchInMm = 25.4f;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EdidColorSpaceChecksOutcome {
  kSuccess = 0,
  kErrorBadCoordinates = 1,
  kErrorPrimariesAreaTooSmall = 2,
  kErrorBluePrimaryIsBroken = 3,
  kErrorCannotExtractToXYZD50 = 4,
  kErrorBadGamma = 5,
  kMaxValue = kErrorBadGamma
};

// Returns true if a given size is in the list of bogus sizes in mm that should
// be ignored.
DISPLAY_UTIL_EXPORT bool IsDisplaySizeBlackListed(
    const gfx::Size& physical_size);

// Returns 64-bit persistent ID for the specified manufacturer's ID and
// product_code_hash, and the index of the output it is connected to.
// |output_index| is used to distinguish the displays of the same type. For
// example, swapping two identical display between two outputs will not be
// treated as swap. The 'serial number' field in EDID isn't used here because
// it is not guaranteed to have unique number and it may have the same fixed
// value (like 0).
DISPLAY_UTIL_EXPORT int64_t GenerateDisplayID(uint16_t manufacturer_id,
                                              uint32_t product_code_hash,
                                              uint8_t output_index);

// Uses |edid_parser| to extract a gfx::ColorSpace which will be IsValid() if
// both gamma and the color primaries were correctly found.
DISPLAY_UTIL_EXPORT gfx::ColorSpace GetColorSpaceFromEdid(
    const display::EdidParser& edid_parser);

}  // namespace display

#endif  // UI_DISPLAY_UTIL_DISPLAY_UTIL_H_
