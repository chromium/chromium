// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MAC_MENU_TEXT_ELIDER_MAC_H_
#define UI_GFX_MAC_MENU_TEXT_ELIDER_MAC_H_

#include <string>

#include "base/component_export.h"

namespace gfx {

// Default maximum width for menu item titles in pixels.
// This value (~400px) provides a reasonable width that matches Safari's
// behavior for menu item truncation.
inline constexpr float kDefaultMenuItemTitleMaxWidth = 400.0f;

// Truncates the given title to fit within the specified maximum width using
// middle ellipsis. Uses the system menu font for measuring text width.
// This is useful for menu items in the menu bar, Dock menu, and context menus.
//
// Example: "Very long title that needs truncation here" becomes
//          "Very long ti…cation here"
//
// If the title fits within max_width, it is returned as-is.
COMPONENT_EXPORT(GFX)
std::u16string ElideMenuItemTitle(const std::u16string& title, float max_width);

// Overload using the default maximum width (kDefaultMenuItemTitleMaxWidth).
COMPONENT_EXPORT(GFX)
std::u16string ElideMenuItemTitle(const std::u16string& title);

}  // namespace gfx

#endif  // UI_GFX_MAC_MENU_TEXT_ELIDER_MAC_H_
