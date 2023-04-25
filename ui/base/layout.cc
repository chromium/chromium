// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/layout.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/check.h"
#include "base/check_op.h"
#include "build/build_config.h"
#include "ui/base/pointer/pointer_device.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"

namespace ui {

namespace {

std::vector<ResourceScaleFactor>* g_supported_resource_scale_factors = nullptr;

}  // namespace

void SetSupportedResourceScaleFactors(
    const std::vector<ResourceScaleFactor>& scale_factors) {
  CHECK(!scale_factors.empty());

  if (g_supported_resource_scale_factors != nullptr) {
    delete g_supported_resource_scale_factors;
  }

  g_supported_resource_scale_factors =
      new std::vector<ResourceScaleFactor>(scale_factors);
  std::sort(g_supported_resource_scale_factors->begin(),
            g_supported_resource_scale_factors->end(),
            [](ResourceScaleFactor lhs, ResourceScaleFactor rhs) {
              return GetScaleForResourceScaleFactor(lhs) <
                     GetScaleForResourceScaleFactor(rhs);
            });

  // Set ImageSkia's supported scales.
  std::vector<float> scales;
  for (std::vector<ResourceScaleFactor>::const_iterator it =
           g_supported_resource_scale_factors->begin();
       it != g_supported_resource_scale_factors->end(); ++it) {
    scales.push_back(GetScaleForResourceScaleFactor(*it));
  }
  gfx::ImageSkia::SetSupportedScales(scales);
}

const std::vector<ResourceScaleFactor>& GetSupportedResourceScaleFactors() {
  CHECK_NE(g_supported_resource_scale_factors, nullptr)
      << "ResourceBundle needs to be intialized.";

  return *g_supported_resource_scale_factors;
}

ResourceScaleFactor GetSupportedResourceScaleFactor(float scale) {
  CHECK_NE(g_supported_resource_scale_factors, nullptr)
      << "ResourceBundle needs to be intialized.";

  ResourceScaleFactor closest_match = k100Percent;
  float smallest_diff = std::numeric_limits<float>::max();
  for (const auto scale_factor : *g_supported_resource_scale_factors) {
    const float diff =
        std::abs(GetScaleForResourceScaleFactor(scale_factor) - scale);
    if (diff < smallest_diff) {
      closest_match = scale_factor;
      smallest_diff = diff;
    } else {
      break;
    }
  }

  CHECK_NE(closest_match, kScaleFactorNone);
  return closest_match;
}

ResourceScaleFactor GetMaxSupportedResourceScaleFactor() {
  CHECK_NE(g_supported_resource_scale_factors, nullptr)
      << "ResourceBundle needs to be intialized.";

  return g_supported_resource_scale_factors->back();
}

bool IsSupportedScale(float scale) {
  CHECK_NE(g_supported_resource_scale_factors, nullptr)
      << "ResourceBundle needs to be intialized.";

  for (const auto scale_factor_idx : *g_supported_resource_scale_factors) {
    if (GetScaleForResourceScaleFactor(scale_factor_idx) == scale) {
      return true;
    }
  }
  return false;
}

namespace test {

ScopedSetSupportedResourceScaleFactors::ScopedSetSupportedResourceScaleFactors(
    const std::vector<ResourceScaleFactor>& new_scale_factors) {
  if (g_supported_resource_scale_factors) {
    original_scale_factors_ =
        std::make_unique<std::vector<ResourceScaleFactor>>(
            *g_supported_resource_scale_factors);
  }
  SetSupportedResourceScaleFactors(new_scale_factors);
}

ScopedSetSupportedResourceScaleFactors::
    ~ScopedSetSupportedResourceScaleFactors() {
  if (original_scale_factors_) {
    SetSupportedResourceScaleFactors(*original_scale_factors_);
  } else {
    delete g_supported_resource_scale_factors;
    g_supported_resource_scale_factors = nullptr;
  }
}

}  // namespace test

float GetScaleFactorForNativeView(gfx::NativeView view) {
  // A number of unit tests do not setup the screen.
  if (!display::Screen::GetScreen())
    return 1.0f;
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(view);

  // GetDisplayNearestView() may return null Display if the |view| is not shown
  // on the screen and there is no primary display. In that case use scale
  // factor 1.0.
  if (!display.is_valid())
    return 1.0f;

  return display.device_scale_factor();
}

}  // namespace ui
