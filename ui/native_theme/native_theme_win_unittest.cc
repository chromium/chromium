// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_win.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"

namespace ui {

using PrefScheme = NativeTheme::PreferredColorScheme;
using SystemThemeColor = NativeTheme::SystemThemeColor;

class TestNativeThemeWin : public NativeThemeWin {
 public:
  TestNativeThemeWin() : NativeThemeWin(false, false) {}
  TestNativeThemeWin& operator=(const TestNativeThemeWin&) = delete;

  ~TestNativeThemeWin() override = default;

  // NativeTheme:
  void SetSystemColor(SystemThemeColor system_color, SkColor color) {
    system_colors_[system_color] = color;
  }
};

TEST(NativeThemeWinTest, CalculatePreferredColorScheme) {
  TestNativeThemeWin theme;

  theme.set_forced_colors(false);
  theme.set_use_dark_colors(true);
  EXPECT_EQ(theme.CalculatePreferredColorScheme(), PrefScheme::kDark);

  theme.set_use_dark_colors(false);
  EXPECT_EQ(theme.CalculatePreferredColorScheme(), PrefScheme::kLight);

  theme.set_forced_colors(true);
  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorBLACK);
  EXPECT_EQ(theme.CalculatePreferredColorScheme(), PrefScheme::kDark);

  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorWHITE);
  EXPECT_EQ(theme.CalculatePreferredColorScheme(), PrefScheme::kLight);

  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorBLUE);
  EXPECT_EQ(theme.CalculatePreferredColorScheme(), PrefScheme::kDark);

  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorYELLOW);
  EXPECT_EQ(theme.CalculatePreferredColorScheme(), PrefScheme::kLight);

  theme.set_forced_colors(false);
  EXPECT_EQ(theme.CalculatePreferredColorScheme(), PrefScheme::kLight);
}

TEST(NativeThemeWinTest, CalculatePreferredContrast) {
  using PrefContrast = NativeTheme::PreferredContrast;

  TestNativeThemeWin theme;

  theme.set_forced_colors(false);
  EXPECT_EQ(theme.CalculatePreferredContrast(), PrefContrast::kNoPreference);

  theme.set_forced_colors(true);
  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorBLACK);
  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorWHITE);
  EXPECT_EQ(theme.CalculatePreferredContrast(), PrefContrast::kMore);

  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorWHITE);
  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorBLACK);
  EXPECT_EQ(theme.CalculatePreferredContrast(), PrefContrast::kMore);

  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorRED);
  EXPECT_EQ(theme.CalculatePreferredContrast(), PrefContrast::kCustom);

  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorYELLOW);
  EXPECT_EQ(theme.CalculatePreferredContrast(), PrefContrast::kLess);

  theme.set_forced_colors(false);
  EXPECT_EQ(theme.CalculatePreferredContrast(), PrefContrast::kNoPreference);
}

TEST(NativeThemeWinTest, GetDefaultSystemColorScheme) {
  using ColorScheme = NativeTheme::ColorScheme;

  TestNativeThemeWin theme;
  theme.set_forced_colors(false);
  theme.set_use_dark_colors(true);
  EXPECT_EQ(theme.GetDefaultSystemColorScheme(), ColorScheme::kDark);

  theme.set_use_dark_colors(false);
  EXPECT_EQ(theme.GetDefaultSystemColorScheme(), ColorScheme::kLight);

  theme.set_forced_colors(true);
  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorBLACK);
  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorWHITE);
  EXPECT_EQ(theme.GetDefaultSystemColorScheme(),
            ColorScheme::kPlatformHighContrast);

  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorWHITE);
  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorBLACK);
  EXPECT_EQ(theme.GetDefaultSystemColorScheme(),
            ColorScheme::kPlatformHighContrast);

  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorBLUE);
  EXPECT_EQ(theme.GetDefaultSystemColorScheme(),
            ColorScheme::kPlatformHighContrast);

  theme.set_forced_colors(false);
  EXPECT_EQ(theme.GetDefaultSystemColorScheme(), ColorScheme::kLight);
}

TEST(NativeThemeWinTest, GetPlatformHighContrastColorScheme) {
  using HCColorScheme = NativeTheme::PlatformHighContrastColorScheme;

  TestNativeThemeWin theme;
  theme.set_forced_colors(false);
  theme.set_preferred_color_scheme(PrefScheme::kDark);
  EXPECT_EQ(theme.GetPlatformHighContrastColorScheme(), HCColorScheme::kNone);

  theme.set_preferred_color_scheme(PrefScheme::kLight);
  EXPECT_EQ(theme.GetPlatformHighContrastColorScheme(), HCColorScheme::kNone);

  theme.set_forced_colors(true);
  theme.set_preferred_color_scheme(PrefScheme::kDark);
  EXPECT_EQ(theme.GetPlatformHighContrastColorScheme(), HCColorScheme::kDark);

  theme.set_preferred_color_scheme(PrefScheme::kLight);
  EXPECT_EQ(theme.GetPlatformHighContrastColorScheme(), HCColorScheme::kLight);

  theme.set_forced_colors(false);
  EXPECT_EQ(theme.GetPlatformHighContrastColorScheme(), HCColorScheme::kNone);
}

}  // namespace ui
