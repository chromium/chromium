// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_TOUCH_TRANSFORM_SETTER_H_
#define UI_DISPLAY_MANAGER_TOUCH_TRANSFORM_SETTER_H_

#include <vector>

#include "ui/display/manager/display_manager_export.h"

namespace ui {
struct TouchDeviceTransform;
}  // namespace ui

namespace display {

// TouchTransformSetter is used by TouchTransformController to apply the actual
// settings.
class DISPLAY_MANAGER_EXPORT TouchTransformSetter {
 public:
  virtual ~TouchTransformSetter() {}

  // |transforms| contains the transform for each device and display pair.
  virtual void ConfigureTouchDevices(
      const std::vector<ui::TouchDeviceTransform>& transforms) = 0;
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_TOUCH_TRANSFORM_SETTER_H_
