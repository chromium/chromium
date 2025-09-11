// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_TEST_NATIVE_THEME_H_
#define UI_NATIVE_THEME_TEST_NATIVE_THEME_H_

#include "ui/native_theme/native_theme.h"

namespace ui {

class TestNativeTheme : public NativeTheme {
 public:
  TestNativeTheme();

  TestNativeTheme(const TestNativeTheme&) = delete;
  TestNativeTheme& operator=(const TestNativeTheme&) = delete;

  ~TestNativeTheme() override;

  // NativeTheme:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;
  void Paint(cc::PaintCanvas* canvas,
             const ui::ColorProvider* color_provider,
             Part part,
             State state,
             const gfx::Rect& rect,
             const ExtraParams& extra,
             PreferredColorScheme color_scheme,
             bool in_forced_colors,
             const std::optional<SkColor>& accent_color) const override;
  bool SupportsNinePatch(Part part) const override;
  gfx::Size GetNinePatchCanvasSize(Part part) const override;
  gfx::Rect GetNinePatchAperture(Part part) const override;

  void SetPreferredColorScheme(PreferredColorScheme color_scheme);
  void AddColorSchemeNativeThemeObserver(NativeTheme* theme_to_update);

 private:
  std::unique_ptr<NativeTheme::ColorSchemeNativeThemeObserver>
      color_scheme_observer_;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_TEST_NATIVE_THEME_H_
