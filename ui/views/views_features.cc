// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/views_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace views::features {

// Please keep alphabetized.

// Used to enable additional a11y attributes when announcing text.
BASE_FEATURE(kAnnounceTextAdditionalAttributes,
             "AnnounceTextAdditionalAttributes",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Use a high-contrast style for ink drops when in platform high-contrast mode,
// including full opacity and a high-contrast color
BASE_FEATURE(kEnablePlatformHighContrastInkDrop,
             "EnablePlatformHighContrastInkDrop",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Handle mouse cursor being out sync with a touch drag operation. If mouse
// cursor is over different window, drag will not start, and cursor will be
// moved to the window where the touch drag started. This is a kill switch
// for this new logic,  crbug.com/370856871.
BASE_FEATURE(kEnableTouchDragCursorSync,
             "EnableTouchDragCursorSync",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables enlargement of HWNDs to a minimum size of 64x64 to handle reported
// graphical glitches on certain hardware.
// TODO(crbug.com/401996981): Remove this once enlargement is confirmed to no
// longer be needed.
BASE_FEATURE(kEnableTransparentHwndEnlargement,
             "EnableTransparentHwndEnlargement",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used to enable keyboard-accessible tooltips in Views UI, as opposed
// to kKeyboardAccessibleTooltip in //ui/base/ui_base_features.cc.
BASE_FEATURE(kKeyboardAccessibleTooltipInViews,
             "KeyboardAccessibleTooltipInViews",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace views::features
