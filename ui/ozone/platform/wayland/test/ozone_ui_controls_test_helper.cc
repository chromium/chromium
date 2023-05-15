// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/weston_test_ozone_ui_controls_test_helper.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/logging.h"
#include "ui/ozone/platform/wayland/test/wayland_ozone_ui_controls_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace ui {

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperWayland() {
  // Older versions of Ash used for version skew tests don't implement the
  // ui_controls protocol extension yet. If this extension is not available, we
  // fall back to using weston_test, so that we can still run version skew
  // tests.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* helper = new wl::WaylandOzoneUIControlsTestHelper();
  if (helper->Initialize()) {
    LOG(WARNING) << "Using ui_controls protocol version 2";
    return helper;
  } else {
    delete helper;
    LOG(WARNING) << "Compositor doesn't support ui_controls version 2, falling "
                    "back to weston_test";
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  return new wl::WestonTestOzoneUIControlsTestHelper();
}

}  // namespace ui
