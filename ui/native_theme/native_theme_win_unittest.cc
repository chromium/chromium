// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_win.h"

#include <cmath>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"
#include "ui/native_theme/mock_os_settings_provider.h"

namespace ui {

using ColorMode = ColorProviderKey::ColorMode;
using PrefScheme = NativeTheme::PreferredColorScheme;

class TestNativeThemeWin : public NativeThemeWin {
 public:
  TestNativeThemeWin() { BeginObservingOsSettingChanges(); }

  ColorMode GetColorMode() const {
    return GetColorProviderKey(/*custom_theme=*/nullptr).color_mode;
  }

  void SetForcedColors(ColorProviderKey::ForcedColors forced_colors) {
    set_forced_colors(forced_colors);
    UpdateColorSchemeAndContrast();
  }

  void SetInDarkMode(bool in_dark_mode) {
    set_in_dark_mode_for_testing(in_dark_mode);
    UpdateColorSchemeAndContrast();
  }

  void SetSystemColor(OsSettingsProvider::ColorId color_id, SkColor color) {
    os_settings_provider_.SetColor(color_id, color);
    UpdateColorSchemeAndContrast();
  }

 private:
  void UpdateColorSchemeAndContrast() {
    set_preferred_color_scheme(CalculatePreferredColorScheme());
    SetPreferredContrast(CalculatePreferredContrast());
  }

  MockOsSettingsProvider os_settings_provider_;
};

TEST(NativeThemeWinTest, CalculatePreferredColorScheme) {
  using enum OsSettingsProvider::ColorId;

  TestNativeThemeWin theme;

  theme.SetForcedColors(ColorProviderKey::ForcedColors::kNone);
  theme.SetInDarkMode(true);
  EXPECT_EQ(theme.preferred_color_scheme(), PrefScheme::kDark);

  theme.SetInDarkMode(false);
  EXPECT_EQ(theme.preferred_color_scheme(), PrefScheme::kLight);

  theme.SetForcedColors(ColorProviderKey::ForcedColors::kSystem);
  theme.SetSystemColor(kWindow, SK_ColorBLACK);
  EXPECT_EQ(theme.preferred_color_scheme(), PrefScheme::kDark);

  theme.SetSystemColor(kWindow, SK_ColorWHITE);
  EXPECT_EQ(theme.preferred_color_scheme(), PrefScheme::kLight);

  theme.SetSystemColor(kWindow, SK_ColorBLUE);
  EXPECT_EQ(theme.preferred_color_scheme(), PrefScheme::kDark);

  theme.SetSystemColor(kWindow, SK_ColorYELLOW);
  EXPECT_EQ(theme.preferred_color_scheme(), PrefScheme::kLight);

  theme.SetForcedColors(ColorProviderKey::ForcedColors::kNone);
  EXPECT_EQ(theme.preferred_color_scheme(), PrefScheme::kLight);
}

TEST(NativeThemeWinTest, CalculatePreferredContrast) {
  using PrefContrast = NativeTheme::PreferredContrast;
  using enum OsSettingsProvider::ColorId;

  TestNativeThemeWin theme;

  theme.SetForcedColors(ColorProviderKey::ForcedColors::kNone);
  EXPECT_EQ(theme.preferred_contrast(), PrefContrast::kNoPreference);

  theme.SetForcedColors(ColorProviderKey::ForcedColors::kSystem);
  theme.SetSystemColor(kWindow, SK_ColorBLACK);
  theme.SetSystemColor(kWindowText, SK_ColorWHITE);
  EXPECT_EQ(theme.preferred_contrast(), PrefContrast::kMore);

  theme.SetSystemColor(kWindow, SK_ColorWHITE);
  theme.SetSystemColor(kWindowText, SK_ColorBLACK);
  EXPECT_EQ(theme.preferred_contrast(), PrefContrast::kMore);

  theme.SetSystemColor(kWindowText, SK_ColorRED);
  EXPECT_EQ(theme.preferred_contrast(), PrefContrast::kCustom);

  theme.SetSystemColor(kWindowText, SK_ColorYELLOW);
  EXPECT_EQ(theme.preferred_contrast(), PrefContrast::kLess);

  theme.SetForcedColors(ColorProviderKey::ForcedColors::kNone);
  EXPECT_EQ(theme.preferred_contrast(), PrefContrast::kNoPreference);
}

TEST(NativeThemeWinTest, TestColorProviderKeyColorMode) {
  TestNativeThemeWin theme;

  theme.SetForcedColors(ColorProviderKey::ForcedColors::kNone);
  theme.SetInDarkMode(true);
  EXPECT_EQ(theme.GetColorMode(), ColorMode::kDark);

  theme.SetInDarkMode(false);
  EXPECT_EQ(theme.GetColorMode(), ColorMode::kLight);

  theme.SetForcedColors(ColorProviderKey::ForcedColors::kSystem);
  theme.set_preferred_color_scheme(PrefScheme::kDark);
  EXPECT_EQ(theme.GetColorMode(), ColorMode::kDark);

  theme.set_preferred_color_scheme(PrefScheme::kLight);
  EXPECT_EQ(theme.GetColorMode(), ColorMode::kLight);
}

}  // namespace ui
