// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/test/touch_transform_controller_test_api.h"

namespace display::test {

TouchTransformControllerTestApi::TouchTransformControllerTestApi(
    TouchTransformController* controller)
    : controller_(controller) {}

TouchTransformControllerTestApi::~TouchTransformControllerTestApi() = default;

}  // namespace display::test
