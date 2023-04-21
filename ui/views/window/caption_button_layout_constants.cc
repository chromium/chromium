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

gfx::Size GetCaptionButtonLayoutSize(CaptionButtonLayoutSize size) {
#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::features::IsJellyrollEnabled()) {
    return gfx::Size(
        36,
        size == CaptionButtonLayoutSize::kBrowserCaptionMaximized ? 34 : 40);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (size == CaptionButtonLayoutSize::kNonBrowserCaption) {
    return gfx::Size(32, 32);
  }

  // |kBrowserMaximizedCaptionButtonHeight| should be kept in sync with those
  // for TAB_HEIGHT in // chrome/browser/ui/layout_constants.cc.
  // TODO(pkasting): Ideally these values should be obtained from a common
  // location.
  int height = ui::TouchUiController::Get()->touch_ui() ? 41 : 34;
  if (size == CaptionButtonLayoutSize::kBrowserCaptionRestored) {
    // Restored window titlebars are 8 DIP taller than maximized.
    height += 8;
  }

  return gfx::Size(32, height);
}

}  // namespace views
