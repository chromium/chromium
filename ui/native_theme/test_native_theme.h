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
             ColorScheme color_scheme,
             bool in_forced_colors,
             const std::optional<SkColor>& accent_color) const override;
  bool SupportsNinePatch(Part part) const override;
  gfx::Size GetNinePatchCanvasSize(Part part) const override;
  gfx::Rect GetNinePatchAperture(Part part) const override;
  bool UserHasContrastPreference() const override;
  bool ShouldUseDarkColors() const override;
  PreferredColorScheme GetPreferredColorScheme() const override;
  ColorScheme GetDefaultSystemColorScheme() const override;

  void SetDarkMode(bool dark_mode) { dark_mode_ = dark_mode; }
  void SetUserHasContrastPreference(bool contrast_preference) {
    contrast_preference_ = contrast_preference;
  }
  void SetIsPlatformHighContrast(bool is_platform_high_contrast) {
    is_platform_high_contrast_ = is_platform_high_contrast;
  }
  void AddColorSchemeNativeThemeObserver(NativeTheme* theme_to_update);

 private:
  bool dark_mode_ = false;
  bool contrast_preference_ = false;
  bool is_platform_high_contrast_ = false;

  std::unique_ptr<NativeTheme::ColorSchemeNativeThemeObserver>
      color_scheme_observer_;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_TEST_NATIVE_THEME_H_
