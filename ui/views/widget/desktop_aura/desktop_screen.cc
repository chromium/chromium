// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen.h"

#include "build/chromeos_buildflags.h"
#include "ui/display/screen.h"

namespace views {

void InstallDesktopScreenIfNecessary() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS ozone builds use another path instead, where display::Screen is
  // properly set. Thus, do early return here.
  return;
#endif

  // The screen may have already been set in test initialization.
  if (!display::Screen::GetScreen())
    CreateDesktopScreen();
}

}  // namespace views
