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
             base::FEATURE_ENABLED_BY_DEFAULT);

// If mouse cursor is over different window Windows will not start a Drag
// and drop. This feature moves the cursor to the location of a touch on
// press-down so that by the time a Drag and drop is started, the cursor will
// already be inside the window. This is a kill switch for this new logic, see
// crbug.com/370856871.
BASE_FEATURE(kEnableTouchDragCursorSync, base::FEATURE_ENABLED_BY_DEFAULT);

// Used to enable keyboard-accessible tooltips in Views UI, as opposed
// to kKeyboardAccessibleTooltip in //ui/base/ui_base_features.cc.
BASE_FEATURE(kKeyboardAccessibleTooltipInViews,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Used to apply the initial URL to the WebContents in WebView. This is a kill
// switch for this new logic, see crbug.com/456058558.
// TODO(https://crbug.com/456058558): Remove this flag once the feature becomes
// stable.
BASE_FEATURE(kApplyInitialUrlToWebContents, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace views::features
