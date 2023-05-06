// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/weston_test_ozone_ui_controls_test_helper.h"

namespace ui {

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperWayland() {
  return new wl::WestonTestOzoneUIControlsTestHelper();
}

}  // namespace ui
