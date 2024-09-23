// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ozone_ui_controls_test_helper.h"

#include "base/notreached.h"
#include "ui/ozone/platform_object.h"

#include "base/logging.h"

namespace ui {

#if BUILDFLAG(IS_LINUX)
void OzoneUIControlsTestHelper::ForceUseScreenCoordinatesOnce() {
  NOTREACHED_IN_MIGRATION();
}
#endif  // BUILDFLAG(IS_LINUX)

std::unique_ptr<OzoneUIControlsTestHelper> CreateOzoneUIControlsTestHelper() {
  return PlatformObject<OzoneUIControlsTestHelper>::Create();
}

}  // namespace ui
