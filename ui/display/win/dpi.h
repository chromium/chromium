// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_DPI_H_
#define UI_DISPLAY_WIN_DPI_H_

#include "ui/display/display_export.h"

namespace display {
namespace win {

// Deprecated. Use --force-device-scale-factor instead.
//
// Sets the device scale factor that will be used unless overridden on the
// command line by --force-device-scale-factor.
DISPLAY_EXPORT void SetDefaultDeviceScaleFactor(float scale);

// Deprecated. Use win::ScreenWin::GetScaleFactorForHWND instead.
//
// Gets the system's scale factor. For example, if the system DPI is 96 then the
// scale factor is 1.0. This does not handle per-monitor DPI.
DISPLAY_EXPORT float GetDPIScale();

// Deprecated. Use win::ScreenWin::GetDPIForHWND instead.
//
// Returns the equivalent DPI for |device_scaling_factor|.
DISPLAY_EXPORT int GetDPIFromScalingFactor(float device_scaling_factor);

// Returns a factor to adjust a system font's height by, to adjust for
// accessibility measures already built into the font, in order to prevent
// applying the same scale factor twice. Value should be in the range
// 0.0 (exclusive) to 1.0 (inclusive).
//
// Windows will add text scaling factor into the logical size of its default
// system fonts (which it does *not* do for DPI scaling). Since we're scaling
// the entire UI by a combination of text scale and DPI scale, this results in
// double scaling. Call this function to unscale the font before using it in
// any of our rendering code.
DISPLAY_EXPORT double GetAccessibilityFontScale();

namespace internal {
// Note: These methods do not take accessibility adjustments into account.

// Equivalent to GetDPIScale() but ignores the --force-device-scale-factor flag.
float GetUnforcedDeviceScaleFactor();

// Returns the equivalent scaling factor for |dpi|.
float GetScalingFactorFromDPI(int dpi);

// Gets the default DPI for the system.
int GetDefaultSystemDPI();

}  // namespace internal
}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_DPI_H_
