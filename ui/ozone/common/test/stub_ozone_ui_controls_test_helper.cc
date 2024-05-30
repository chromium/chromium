// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/test/stub_ozone_ui_controls_test_helper.h"

#include "base/logging.h"
#include "base/notreached.h"

namespace ui {

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperDrm() {
  NOTREACHED_NORETURN();
}

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperFlatland() {
  NOTREACHED_NORETURN();
}

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperHeadless() {
  NOTREACHED_NORETURN();
}

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperCast() {
  NOTREACHED_NORETURN();
}

}  // namespace ui
