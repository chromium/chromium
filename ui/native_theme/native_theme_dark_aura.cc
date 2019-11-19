// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_dark_aura.h"

#include "ui/gfx/color_palette.h"
#include "ui/native_theme/common_theme.h"

namespace ui {

NativeThemeDarkAura* NativeThemeDarkAura::instance() {
  static base::NoDestructor<NativeThemeDarkAura> s_native_theme;
  return s_native_theme.get();
}

SkColor NativeThemeDarkAura::GetSystemColor(ColorId color_id,
                                            ColorScheme color_scheme) const {
  return GetAuraColor(color_id, this, color_scheme);
}

bool NativeThemeDarkAura::ShouldUseDarkColors() const {
  return true;
}

NativeTheme::PreferredColorScheme NativeThemeDarkAura::GetPreferredColorScheme()
    const {
  return NativeTheme::PreferredColorScheme::kDark;
}

NativeThemeDarkAura::NativeThemeDarkAura() : NativeThemeAura(false) {}

NativeThemeDarkAura::~NativeThemeDarkAura() {}

}  // namespace ui
