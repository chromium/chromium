// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/test/stub_ozone_ui_controls_test_helper.h"

#include "base/logging.h"
#include "base/notreached.h"

namespace ui {

namespace {
OzoneUIControlsTestHelper* PrintErrorAndReturnNullptr() {
  NOTREACHED()
      << "Notimplemented or not supported by the underlaying platform.";
  return nullptr;
}
}  // namespace

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperWindows() {
  return PrintErrorAndReturnNullptr();
}

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperDrm() {
  return PrintErrorAndReturnNullptr();
}

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperScenic() {
  return PrintErrorAndReturnNullptr();
}

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperFlatland() {
  return PrintErrorAndReturnNullptr();
}

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperHeadless() {
  return PrintErrorAndReturnNullptr();
}

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperCast() {
  return PrintErrorAndReturnNullptr();
}

}  // namespace ui
