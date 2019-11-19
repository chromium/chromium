// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_TEST_NATIVE_THEME_H_
#define UI_NATIVE_THEME_TEST_NATIVE_THEME_H_

#include "base/macros.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

class TestNativeTheme : public NativeTheme {
 public:
  TestNativeTheme();
  ~TestNativeTheme() override;

  // NativeTheme:
  SkColor GetSystemColor(ColorId color_id,
                         ColorScheme color_scheme) const override;
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;
  void Paint(cc::PaintCanvas* canvas,
             Part part,
             State state,
             const gfx::Rect& rect,
             const ExtraParams& extra,
             ColorScheme color_scheme) const override;
  bool SupportsNinePatch(Part part) const override;
  gfx::Size GetNinePatchCanvasSize(Part part) const override;
  gfx::Rect GetNinePatchAperture(Part part) const override;
  bool UsesHighContrastColors() const override;
  bool ShouldUseDarkColors() const override;
  PreferredColorScheme GetPreferredColorScheme() const override;

  void SetDarkMode(bool dark_mode) { dark_mode_ = dark_mode; }
  void SetUsesHighContrastColors(bool high_contrast) {
    high_contrast_ = high_contrast;
  }
  void AddColorSchemeNativeThemeObserver(NativeTheme* theme_to_update);

 private:
  bool dark_mode_ = false;
  bool high_contrast_ = false;

  std::unique_ptr<NativeTheme::ColorSchemeNativeThemeObserver>
      color_scheme_observer_;

  DISALLOW_COPY_AND_ASSIGN(TestNativeTheme);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_TEST_NATIVE_THEME_H_
