// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/desktop_screen_x11_test_api.h"

#include "ui/display/screen.h"
#include "ui/views/widget/desktop_aura/desktop_screen_x11.h"

namespace views {
namespace test {

void DesktopScreenX11TestApi::UpdateDisplays() {
  DesktopScreenX11* screen =
      static_cast<DesktopScreenX11*>(display::Screen::GetScreen());
  screen->x11_display_manager_->UpdateDisplayList();
}

}  // namespace test
}  // namespace views
