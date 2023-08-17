// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/caption_button_layout_constants.h"

#include "build/chromeos_buildflags.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace views {

int GetCaptionButtonWidth() {
#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::features::IsRoundedWindowsEnabled()) {
    return 36;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  return 32;
}

gfx::Size GetCaptionButtonLayoutSize(CaptionButtonLayoutSize size) {
  const int button_width = GetCaptionButtonWidth();
#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::features::IsRoundedWindowsEnabled()) {
    return gfx::Size(
        button_width,
        size == CaptionButtonLayoutSize::kBrowserCaptionMaximized ? 34 : 40);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (size == CaptionButtonLayoutSize::kNonBrowserCaption) {
    return gfx::Size(button_width, 32);
  }

  // |kBrowserMaximizedCaptionButtonHeight| should be kept in sync with those
  // for TAB_HEIGHT in // chrome/browser/ui/layout_constants.cc.
  // TODO(pkasting): Ideally these values should be obtained from a common
  // location like layout_constants.cc.
  int height = ui::TouchUiController::Get()->touch_ui() ? 41 : 34;
  if (size == CaptionButtonLayoutSize::kBrowserCaptionRestored) {
    // Restored window titlebars are 8 DIP taller than maximized.
    height += 8;
  }

  return gfx::Size(button_width, height);
}

}  // namespace views
