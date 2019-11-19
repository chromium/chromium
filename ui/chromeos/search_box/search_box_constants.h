// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_SEARCH_BOX_SEARCH_BOX_CONSTANTS_H_
#define UI_CHROMEOS_SEARCH_BOX_SEARCH_BOX_CONSTANTS_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/search_box/search_box_export.h"

namespace search_box {

// Default color used when wallpaper customized color is not available for
// searchbox, #000 at 87% opacity.
SEARCH_BOX_EXPORT constexpr SkColor kDefaultSearchboxColor =
    SkColorSetARGB(0xDE, 0x00, 0x00, 0x00);

// The horizontal padding of the box layout of the search box.
SEARCH_BOX_EXPORT constexpr int kPadding = 12;

// The default background color of the search box.
SEARCH_BOX_EXPORT constexpr SkColor kSearchBoxBackgroundDefault = SK_ColorWHITE;

// The background border corner radius of the search box.
SEARCH_BOX_EXPORT constexpr int kSearchBoxBorderCornerRadius = 24;

// The background border corner radius of the expanded search box.
SEARCH_BOX_EXPORT constexpr int kSearchBoxBorderCornerRadiusSearchResult = 20;

// Preferred height of search box.
SEARCH_BOX_EXPORT constexpr int kSearchBoxPreferredHeight = 48;

// The size of the icon in the search box.
SEARCH_BOX_EXPORT constexpr int kIconSize = 24;

// The size of the image button in the search box.
SEARCH_BOX_EXPORT constexpr int kButtonSizeDip = 40;

}  // namespace search_box

#endif  // UI_CHROMEOS_SEARCH_BOX_SEARCH_BOX_CONSTANTS_H_
