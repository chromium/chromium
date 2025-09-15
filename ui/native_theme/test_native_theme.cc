// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/test_native_theme.h"

namespace ui {

TestNativeTheme::TestNativeTheme() = default;
TestNativeTheme::~TestNativeTheme() = default;

gfx::Size TestNativeTheme::GetPartSize(Part part,
                                       State state,
                                       const ExtraParams& extra_params) const {
  return gfx::Size();
}

void TestNativeTheme::Paint(cc::PaintCanvas* canvas,
                            const ui::ColorProvider* color_provider,
                            Part part,
                            State state,
                            const gfx::Rect& rect,
                            const ExtraParams& extra_params,
                            bool forced_colors,
                            PreferredColorScheme color_scheme,
                            PreferredContrast contrast,
                            std::optional<SkColor> accent_color) const {}

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

}  // namespace ui
