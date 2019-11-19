// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_win.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"

namespace ui {

using ColorScheme = NativeTheme::ColorScheme;
using PrefScheme = NativeTheme::PreferredColorScheme;
using SystemThemeColor = NativeTheme::SystemThemeColor;
using ColorId = NativeTheme::ColorId;

class TestNativeThemeWin : public NativeThemeWin {
 public:
  TestNativeThemeWin() {}
  ~TestNativeThemeWin() override {}

  // NativeTheme:
  bool UsesHighContrastColors() const override { return high_contrast_; }
  bool ShouldUseDarkColors() const override { return dark_mode_; }

  void SetDarkMode(bool dark_mode) { dark_mode_ = dark_mode; }
  void SetUsesHighContrastColors(bool high_contrast) {
    high_contrast_ = high_contrast;
  }
  void SetSystemColor(SystemThemeColor system_color, SkColor color) {
    system_colors_[system_color] = color;
  }

 private:
  bool dark_mode_ = false;
  bool high_contrast_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestNativeThemeWin);
};

TEST(NativeThemeWinTest, CalculatePreferredColorScheme) {
  TestNativeThemeWin theme;

  theme.SetUsesHighContrastColors(false);
  theme.SetDarkMode(true);
  ASSERT_EQ(theme.CalculatePreferredColorScheme(), PrefScheme::kDark);

  theme.SetDarkMode(false);
  ASSERT_EQ(theme.CalculatePreferredColorScheme(), PrefScheme::kLight);

  theme.SetUsesHighContrastColors(true);
  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorBLACK);
  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorWHITE);
  ASSERT_EQ(theme.CalculatePreferredColorScheme(), PrefScheme::kDark);

  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorWHITE);
  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorBLACK);
  ASSERT_EQ(theme.CalculatePreferredColorScheme(), PrefScheme::kLight);

  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorBLUE);
  ASSERT_EQ(theme.CalculatePreferredColorScheme(), PrefScheme::kNoPreference);

  theme.SetUsesHighContrastColors(false);
  ASSERT_EQ(theme.CalculatePreferredColorScheme(), PrefScheme::kLight);
}

TEST(NativeThemeWinTest, GetDefaultSystemColorScheme) {
  TestNativeThemeWin theme;

  theme.SetUsesHighContrastColors(false);
  theme.SetDarkMode(true);
  ASSERT_EQ(theme.GetDefaultSystemColorScheme(), ColorScheme::kDark);

  theme.SetDarkMode(false);
  ASSERT_EQ(theme.GetDefaultSystemColorScheme(), ColorScheme::kLight);

  theme.SetUsesHighContrastColors(true);
  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorBLACK);
  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorWHITE);
  ASSERT_EQ(theme.GetDefaultSystemColorScheme(),
            ColorScheme::kPlatformHighContrast);

  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorWHITE);
  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorBLACK);
  ASSERT_EQ(theme.GetDefaultSystemColorScheme(),
            ColorScheme::kPlatformHighContrast);

  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorBLUE);
  ASSERT_EQ(theme.GetDefaultSystemColorScheme(),
            ColorScheme::kPlatformHighContrast);

  theme.SetUsesHighContrastColors(false);
  ASSERT_EQ(theme.GetDefaultSystemColorScheme(), ColorScheme::kLight);
}

TEST(NativeThemeWinTest, GetPlatformHighContrastColor) {
  TestNativeThemeWin theme;

  // These specific colors don't matter, but should be unique.
  constexpr SkColor kWindowTextColor = SK_ColorGREEN;
  constexpr SkColor kHighlightColor = SK_ColorYELLOW;
  constexpr SkColor kHighlightTextColor = SK_ColorBLUE;

  theme.SetSystemColor(SystemThemeColor::kWindowText, kWindowTextColor);
  theme.SetSystemColor(SystemThemeColor::kHighlight, kHighlightColor);
  theme.SetSystemColor(SystemThemeColor::kHighlightText, kHighlightTextColor);

  // Test that we get regular colors when HC is off.
  theme.SetUsesHighContrastColors(false);
  ASSERT_NE(theme.GetSystemColor(ColorId::kColorId_LabelEnabledColor),
            kWindowTextColor);
  ASSERT_NE(theme.GetSystemColor(ColorId::kColorId_ProminentButtonColor),
            kHighlightColor);
  ASSERT_NE(theme.GetSystemColor(ColorId::kColorId_TextOnProminentButtonColor),
            kHighlightTextColor);

  // Test that we get HC colors when HC is on.
  theme.SetUsesHighContrastColors(true);
  ASSERT_EQ(theme.GetSystemColor(ColorId::kColorId_LabelEnabledColor),
            kWindowTextColor);
  ASSERT_EQ(theme.GetSystemColor(ColorId::kColorId_ProminentButtonColor),
            kHighlightColor);
  ASSERT_EQ(theme.GetSystemColor(ColorId::kColorId_TextOnProminentButtonColor),
            kHighlightTextColor);
}

}  // namespace ui
