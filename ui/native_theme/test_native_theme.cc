// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/test_native_theme.h"

namespace ui {

TestNativeTheme::TestNativeTheme() : NativeTheme(false) {}
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
                            ColorScheme color_scheme,
                            bool in_forced_colors,
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

bool TestNativeTheme::UserHasContrastPreference() const {
  return contrast_preference_;
}

bool TestNativeTheme::ShouldUseDarkColors() const {
  return dark_mode_;
}

NativeTheme::PreferredColorScheme TestNativeTheme::GetPreferredColorScheme()
    const {
  return CalculatePreferredColorScheme();
}

NativeTheme::ColorScheme TestNativeTheme::GetDefaultSystemColorScheme() const {
  if (is_platform_high_contrast_)
    return ColorScheme::kPlatformHighContrast;
  return NativeTheme::GetDefaultSystemColorScheme();
}

void TestNativeTheme::AddColorSchemeNativeThemeObserver(
    NativeTheme* theme_to_update) {
  color_scheme_observer_ =
      std::make_unique<ui::NativeTheme::ColorSchemeNativeThemeObserver>(
          theme_to_update);
  AddObserver(color_scheme_observer_.get());
}

}  // namespace ui
