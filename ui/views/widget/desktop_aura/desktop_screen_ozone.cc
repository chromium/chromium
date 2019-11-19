// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/screen_ozone.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"

namespace views {

display::Screen* CreateDesktopScreen() {
  return new aura::ScreenOzone();
}

}  // namespace views
