// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_TEST_STUB_OZONE_UI_CONTROLS_TEST_HELPER_H_
#define UI_OZONE_COMMON_TEST_STUB_OZONE_UI_CONTROLS_TEST_HELPER_H_

namespace ui {

class OzoneUIControlsTestHelper;

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperWayland();
OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperWindows();
OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperDrm();
OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperScenic();
OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperHeadless();
OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperCast();

}  // namespace ui

#endif  // UI_OZONE_COMMON_TEST_STUB_OZONE_UI_CONTROLS_TEST_HELPER_H_
