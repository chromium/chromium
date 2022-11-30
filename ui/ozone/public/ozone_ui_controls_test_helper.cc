// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ozone_ui_controls_test_helper.h"

#include "ui/ozone/platform_object.h"

#include "base/logging.h"

namespace ui {

std::unique_ptr<OzoneUIControlsTestHelper> CreateOzoneUIControlsTestHelper() {
  return PlatformObject<OzoneUIControlsTestHelper>::Create();
}

}  // namespace ui
