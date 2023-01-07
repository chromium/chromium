// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/dpi.h"

#include <windows.h>

#include "base/win/scoped_hdc.h"
#include "ui/display/display.h"
#include "ui/display/win/uwp_text_scale_factor.h"

namespace display {
namespace win {

namespace {

const float kDefaultDPI = 96.f;

float g_device_scale_factor = 0.f;

}  // namespace

void SetDefaultDeviceScaleFactor(float scale) {
  DCHECK_NE(0.f, scale);
  g_device_scale_factor = scale;
}

float GetDPIScale() {
  if (Display::HasForceDeviceScaleFactor())
    return Display::GetForcedDeviceScaleFactor();
  return display::win::internal::GetUnforcedDeviceScaleFactor();
}

int GetDPIFromScalingFactor(float device_scaling_factor) {
  return kDefaultDPI * device_scaling_factor;
}

double GetAccessibilityFontScale() {
  return 1.0 / UwpTextScaleFactor::Instance()->GetTextScaleFactor();
}

namespace internal {

int GetDefaultSystemDPI() {
  static int dpi_x = 0;
  static int dpi_y = 0;
  static bool should_initialize = true;

  if (should_initialize) {
    should_initialize = false;
    base::win::ScopedGetDC screen_dc(NULL);
    // This value is safe to cache for the life time of the app since the
    // user must logout to change the DPI setting. This value also applies
    // to all screens.
    dpi_x = GetDeviceCaps(screen_dc, LOGPIXELSX);
    dpi_y = GetDeviceCaps(screen_dc, LOGPIXELSY);
    DCHECK_EQ(dpi_x, dpi_y);
  }
  return dpi_x;
}

float GetUnforcedDeviceScaleFactor() {
  return g_device_scale_factor ? g_device_scale_factor
                               : GetScalingFactorFromDPI(GetDefaultSystemDPI());
}

float GetScalingFactorFromDPI(int dpi) {
  return static_cast<float>(dpi) / kDefaultDPI;
}

}  // namespace internal
}  // namespace win
}  // namespace display
