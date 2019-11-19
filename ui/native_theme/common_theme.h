// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_COMMON_THEME_H_
#define UI_NATIVE_THEME_COMMON_THEME_H_

#include <memory>

#include "ui/native_theme/native_theme.h"


namespace ui {

// Drawing code that is common for all platforms.

// Returns the color to use on Aura for |color_id|.  For a few colors that are
// theme-specific, |base_theme| must be non-null; consult the code to see which
// color IDs fall into this category.
SkColor NATIVE_THEME_EXPORT GetAuraColor(
    NativeTheme::ColorId color_id,
    const NativeTheme* base_theme,
    NativeTheme::ColorScheme color_scheme = NativeTheme::ColorScheme::kDefault);

void NATIVE_THEME_EXPORT CommonThemePaintMenuItemBackground(
    const NativeTheme* theme,
    cc::PaintCanvas* canvas,
    NativeTheme::State state,
    const gfx::Rect& rect,
    const NativeTheme::MenuItemExtraParams& menu_item,
    NativeTheme::ColorScheme color_scheme);

}  // namespace ui

#endif  // UI_NATIVE_THEME_COMMON_THEME_H_
