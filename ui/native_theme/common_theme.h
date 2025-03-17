// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_COMMON_THEME_H_
#define UI_NATIVE_THEME_COMMON_THEME_H_

#include "base/component_export.h"
#include "ui/color/color_id.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

class ColorProvider;

// Drawing code that is common for all platforms.

void COMPONENT_EXPORT(NATIVE_THEME) CommonThemePaintMenuItemBackground(
    const NativeTheme* theme,
    const ColorProvider* color_provider,
    cc::PaintCanvas* canvas,
    NativeTheme::State state,
    const gfx::Rect& rect,
    const NativeTheme::MenuItemExtraParams& menu_item);

}  // namespace ui

#endif  // UI_NATIVE_THEME_COMMON_THEME_H_
