// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_COMMON_THEME_H_
#define UI_NATIVE_THEME_COMMON_THEME_H_


#include "ui/color/color_id.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_export.h"

namespace ui {

class ColorProvider;

// Drawing code that is common for all platforms.

void NATIVE_THEME_EXPORT CommonThemePaintMenuItemBackground(
    const NativeTheme* theme,
    const ColorProvider* color_provider,
    cc::PaintCanvas* canvas,
    NativeTheme::State state,
    const gfx::Rect& rect,
    const NativeTheme::MenuItemExtraParams& menu_item);

}  // namespace ui

#endif  // UI_NATIVE_THEME_COMMON_THEME_H_
