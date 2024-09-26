// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_UTIL_DISPLAY_UTIL_H_
#define UI_DISPLAY_UTIL_DISPLAY_UTIL_H_

#include <stdint.h>

#include "base/containers/flat_set.h"
#include "ui/display/util/display_util_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/size.h"

namespace display {

class EdidParser;

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

const float kDefaultHdrMaxLuminanceRelative = 1.2f;

// Returns true if a given size is allowed. Will return false for certain bogus
// sizes in mm that should be ignored.
DISPLAY_UTIL_EXPORT bool IsDisplaySizeValid(const gfx::Size& physical_size);

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

// Returns true if one of following conditions is met.
// 1) id1 is internal.
// 2) output index of id1 < output index of id2 and id2 isn't internal.
DISPLAY_UTIL_EXPORT bool CompareDisplayIds(int64_t id1, int64_t id2);

// Returns true if the `display_id` is internal.
DISPLAY_UTIL_EXPORT bool IsInternalDisplayId(int64_t display_id);

// Returns true if the system has at least one internal display.
DISPLAY_UTIL_EXPORT bool HasInternalDisplay();

// Gets/Sets an id of display corresponding to internal panel.
DISPLAY_UTIL_EXPORT const base::flat_set<int64_t>& GetInternalDisplayIds();
DISPLAY_UTIL_EXPORT void SetInternalDisplayIds(
    base::flat_set<int64_t> display_ids);

// Converts the color string name into a gfx::ColorSpace profile.
DISPLAY_UTIL_EXPORT gfx::ColorSpace ForcedColorProfileStringToColorSpace(
    const std::string& value);

// Returns the forced display color profile, which is given by
// "--force-color-profile".
DISPLAY_UTIL_EXPORT gfx::ColorSpace GetForcedDisplayColorProfile();

// Indicates if a display color profile is being explicitly enforced from the
// command line via "--force-color-profile".
DISPLAY_UTIL_EXPORT bool HasForceDisplayColorProfile();

#if BUILDFLAG(IS_CHROMEOS)
// Taken from DisplayChangeObserver::CreateDisplayColorSpaces()
// Constructs the raster DisplayColorSpaces out of |snapshot_color_space|,
// including the HDR ones if present and |allow_high_bit_depth| is set.
DISPLAY_UTIL_EXPORT gfx::DisplayColorSpaces CreateDisplayColorSpaces(
    const gfx::ColorSpace& snapshot_color_space,
    bool allow_high_bit_depth,
    const std::optional<gfx::HDRStaticMetadata>& hdr_static_metadata);
#endif  // BUILDFLAG(IS_CHROMEOS)

DISPLAY_UTIL_EXPORT int ConnectorIndex8(int device_index, int display_index);

// A connector's index is a combination of:
// 1) |display_index| the display's index in DRM       bits 0-7
// 2) |device_index| the display's DRM's index         bits 8-15
// e.g. - A 3rd display in a 2nd DRM would produce a connector index == 0x0102
//        (since display index == 2 and DRM index == 1)
DISPLAY_UTIL_EXPORT uint16_t ConnectorIndex16(uint8_t device_index,
                                              uint8_t display_index);

}  // namespace display

#endif  // UI_DISPLAY_UTIL_DISPLAY_UTIL_H_
