// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_RESOURCE_SCALE_FACTOR_H_
#define UI_BASE_RESOURCE_RESOURCE_SCALE_FACTOR_H_

#include <memory>
#include <vector>

#include "base/component_export.h"

namespace ui {

// Supported resource scale factors for the platform. This is used as an index
// into the array `kResourceScaleFactorScales` which maps the enum value to a
// float. `kScaleFactorNone` is used for density independent resources such as
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
COMPONENT_EXPORT(UI_DATA_PACK)
float GetScaleForResourceScaleFactor(ResourceScaleFactor scale_factor);

// Changes the value of `GetSupportedScaleFactors` to `scale_factors`. Use
// `ScopedSetSupportedResourceScaleFactors` for unit tests to avoid affecting
// the state of other tests.
COMPONENT_EXPORT(UI_DATA_PACK)
void SetSupportedResourceScaleFactors(
    const std::vector<ResourceScaleFactor>& scale_factors);

// Returns a vector with the scale factors which are supported by this
// platform, in ascending order.
COMPONENT_EXPORT(UI_DATA_PACK)
const std::vector<ResourceScaleFactor>& GetSupportedResourceScaleFactors();

// Returns the supported ResourceScaleFactor which most closely matches `scale`.
COMPONENT_EXPORT(UI_DATA_PACK)
ResourceScaleFactor GetSupportedResourceScaleFactor(float scale);

// Returns a resource scale factor value that can be used to load a resource
// that will later be scaled to `scale` minimizing quality loss. This means
// that, instead of the closest resource scale factor, it might return a
// resource meant for a bigger scale factor.
COMPONENT_EXPORT(UI_DATA_PACK)
ResourceScaleFactor GetSupportedResourceScaleFactorForRescale(float scale);

// Returns the maximum supported ResourceScaleFactor.
COMPONENT_EXPORT(UI_DATA_PACK)
ResourceScaleFactor GetMaxSupportedResourceScaleFactor();

// Returns the maximum supported ResourceScaleFactor as a float.
COMPONENT_EXPORT(UI_DATA_PACK)
float GetScaleForMaxSupportedResourceScaleFactor();

// Returns true if the scale passed in is the list of supported scales for
// the platform.
COMPONENT_EXPORT(UI_DATA_PACK)
bool IsScaleFactorSupported(ResourceScaleFactor scale_factor);

namespace test {

// Class which changes the value of GetSupportedResourceScaleFactors() to
// `new_scale_factors` for the duration of its lifetime.
class COMPONENT_EXPORT(UI_DATA_PACK) ScopedSetSupportedResourceScaleFactors {
 public:
  explicit ScopedSetSupportedResourceScaleFactors(
      const std::vector<ResourceScaleFactor>& new_scale_factors);
  ScopedSetSupportedResourceScaleFactors(
      const ScopedSetSupportedResourceScaleFactors&) = delete;
  ScopedSetSupportedResourceScaleFactors& operator=(
      const ScopedSetSupportedResourceScaleFactors&) = delete;
  ~ScopedSetSupportedResourceScaleFactors();

 private:
  std::unique_ptr<std::vector<ResourceScaleFactor>> original_scale_factors_;
};

}  // namespace test

}  // namespace ui

#endif  // UI_BASE_RESOURCE_RESOURCE_SCALE_FACTOR_H_
