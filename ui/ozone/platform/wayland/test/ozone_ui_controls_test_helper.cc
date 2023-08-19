// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/ozone/platform/wayland/test/wayland_ozone_ui_controls_test_helper.h"
#else
#include "ui/ozone/platform/wayland/test/weston_test_ozone_ui_controls_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace ui {

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperWayland() {
  // The ui-controls protocol extension is currently only available on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return new wl::WaylandOzoneUIControlsTestHelper();
#else
  return new wl::WestonTestOzoneUIControlsTestHelper();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace ui
