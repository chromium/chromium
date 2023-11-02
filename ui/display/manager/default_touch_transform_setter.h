// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DEFAULT_TOUCH_TRANSFORM_SETTER_H_
#define UI_DISPLAY_MANAGER_DEFAULT_TOUCH_TRANSFORM_SETTER_H_

#include "ui/display/manager/touch_transform_setter.h"

namespace display {

class DISPLAY_MANAGER_EXPORT DefaultTouchTransformSetter
    : public TouchTransformSetter {
 public:
  DefaultTouchTransformSetter();

  DefaultTouchTransformSetter(const DefaultTouchTransformSetter&) = delete;
  DefaultTouchTransformSetter& operator=(const DefaultTouchTransformSetter&) =
      delete;

  ~DefaultTouchTransformSetter() override;

  // TouchTransformSetter:
  void ConfigureTouchDevices(
      const std::vector<ui::TouchDeviceTransform>& transforms) override;
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DEFAULT_TOUCH_TRANSFORM_SETTER_H_
