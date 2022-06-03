// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/caption_button_layout_constants.h"

#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/geometry/size.h"

namespace views {

gfx::Size GetCaptionButtonLayoutSize(CaptionButtonLayoutSize size) {
  if (size == CaptionButtonLayoutSize::kNonBrowserCaption)
    return gfx::Size(kCaptionButtonWidth, 32);

  // |kBrowserMaximizedCaptionButtonHeight| should be kept in sync with those
  // for TAB_HEIGHT in // chrome/browser/ui/layout_constants.cc.
  // TODO(pkasting): Ideally these values should be obtained from a common
  // location.
  int height = ui::TouchUiController::Get()->touch_ui() ? 41 : 34;
  if (size == CaptionButtonLayoutSize::kBrowserCaptionRestored)
    height += 8;  // Restored window titlebars are 8 DIP taller than maximized.
  return gfx::Size(kCaptionButtonWidth, height);
}

}  // namespace views
