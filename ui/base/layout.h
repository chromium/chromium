// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_LAYOUT_H_
#define UI_BASE_LAYOUT_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "build/build_config.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

// Changes the value of GetSupportedScaleFactors() to |scale_factors|.
// Use ScopedSetSupportedScaleFactors for unit tests as not to affect the
// state of other tests.
COMPONENT_EXPORT(UI_BASE)
void SetSupportedResourceScaleFactors(
    const std::vector<ResourceScaleFactor>& scale_factors);

// Returns a vector with the scale factors which are supported by this
// platform, in ascending order.
COMPONENT_EXPORT(UI_BASE)
const std::vector<ResourceScaleFactor>& GetSupportedResourceScaleFactors();

// Returns the supported ResourceScaleFactor which most closely matches |scale|.
// Converting from float to ResourceScaleFactor is inefficient and should be
// done as little as possible.
COMPONENT_EXPORT(UI_BASE)
ResourceScaleFactor GetSupportedResourceScaleFactor(float image_scale);

// Returns the ResourceScaleFactor used by |view|.
COMPONENT_EXPORT(UI_BASE)
float GetScaleFactorForNativeView(gfx::NativeView view);

// Returns true if the scale passed in is the list of supported scales for
// the platform.
// TODO(oshima): Deprecate this.
COMPONENT_EXPORT(UI_BASE) bool IsSupportedScale(float scale);

namespace test {
// Class which changes the value of GetSupportedResourceScaleFactors() to
// |new_scale_factors| for the duration of its lifetime.
class COMPONENT_EXPORT(UI_BASE) ScopedSetSupportedResourceScaleFactors {
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

#endif  // UI_BASE_LAYOUT_H_
