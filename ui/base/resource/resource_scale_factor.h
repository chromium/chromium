// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_RESOURCE_SCALE_FACTOR_H_
#define UI_BASE_RESOURCE_RESOURCE_SCALE_FACTOR_H_

#include "ui/base/resource/data_pack_export.h"

namespace ui {

// Supported resource scale factors for the platform. This is used as an index
// into the array |kScaleFactorScales| which maps the enum value to a float.
// kScaleFactorNone is used for density independent resources such as
// string, html/js files or an image that can be used for any scale factors
// (such as wallpapers).
enum ResourceScaleFactor : int {
  kScaleFactorNone = 0,
  k100Percent,
  k200Percent,
  k300Percent,

  NUM_SCALE_FACTORS  // This always appears last.
};

// Returns the image scale for the scale factor passed in.
UI_DATA_PACK_EXPORT float GetScaleForResourceScaleFactor(
    ResourceScaleFactor scale_factor);

}  // namespace ui

#endif  // UI_BASE_RESOURCE_RESOURCE_SCALE_FACTOR_H_
