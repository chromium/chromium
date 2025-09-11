// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/test_native_theme.h"

namespace ui {

TestNativeTheme::TestNativeTheme() = default;
TestNativeTheme::~TestNativeTheme() = default;

gfx::Size TestNativeTheme::GetPartSize(Part part,
                                       State state,
                                       const ExtraParams& extra) const {
  return gfx::Size();
}

void TestNativeTheme::Paint(cc::PaintCanvas* canvas,
                            const ui::ColorProvider* color_provider,
                            Part part,
                            State state,
                            const gfx::Rect& rect,
                            const ExtraParams& extra,
                            bool forced_colors,
                            PreferredColorScheme color_scheme,
                            PreferredContrast contrast,
                            const std::optional<SkColor>& accent_color) const {}

bool TestNativeTheme::SupportsNinePatch(Part part) const {
  return false;
}

gfx::Size TestNativeTheme::GetNinePatchCanvasSize(Part part) const {
  return gfx::Size();
}

gfx::Rect TestNativeTheme::GetNinePatchAperture(Part part) const {
  return gfx::Rect();
}

void TestNativeTheme::SetPreferredColorScheme(
    PreferredColorScheme color_scheme) {
  set_preferred_color_scheme(color_scheme);
}

void TestNativeTheme::AddColorSchemeNativeThemeObserver(
    NativeTheme* theme_to_update) {
  color_scheme_observer_ =
      std::make_unique<ui::NativeTheme::ColorSchemeNativeThemeObserver>(
          theme_to_update);
  AddObserver(color_scheme_observer_.get());
}

}  // namespace ui
