// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_win.h"

#include <cmath>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"

namespace ui {

using ColorMode = ColorProviderKey::ColorMode;
using PrefScheme = NativeTheme::PreferredColorScheme;
using SystemThemeColor = NativeTheme::SystemThemeColor;

class TestNativeThemeWin : public NativeThemeWin {
 public:
  TestNativeThemeWin() = default;
  TestNativeThemeWin& operator=(const TestNativeThemeWin&) = delete;

  ~TestNativeThemeWin() override = default;

  ColorMode GetColorMode() const {
    return GetColorProviderKey(/*custom_theme=*/nullptr).color_mode;
  }

  void SetForcedColors(bool forced_colors) {
    set_forced_colors(forced_colors);
    UpdateColorSchemeAndContrast();
  }

  void SetInDarkMode(bool in_dark_mode) {
    set_in_dark_mode_for_testing(in_dark_mode);
    UpdateColorSchemeAndContrast();
  }

  void SetSystemColor(SystemThemeColor system_color, SkColor color) {
    system_colors_[system_color] = color;
    UpdateColorSchemeAndContrast();
  }

 private:
  void UpdateColorSchemeAndContrast() {
    set_preferred_color_scheme(CalculatePreferredColorScheme());
    SetPreferredContrast(CalculatePreferredContrast());
  }
};

TEST(NativeThemeWinTest, CalculatePreferredColorScheme) {
  TestNativeThemeWin theme;

  theme.SetForcedColors(false);
  theme.SetInDarkMode(true);
  EXPECT_EQ(theme.preferred_color_scheme(), PrefScheme::kDark);

  theme.SetInDarkMode(false);
  EXPECT_EQ(theme.preferred_color_scheme(), PrefScheme::kLight);

  theme.SetForcedColors(true);
  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorBLACK);
  EXPECT_EQ(theme.preferred_color_scheme(), PrefScheme::kDark);

  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorWHITE);
  EXPECT_EQ(theme.preferred_color_scheme(), PrefScheme::kLight);

  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorBLUE);
  EXPECT_EQ(theme.preferred_color_scheme(), PrefScheme::kDark);

  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorYELLOW);
  EXPECT_EQ(theme.preferred_color_scheme(), PrefScheme::kLight);

  theme.SetForcedColors(false);
  EXPECT_EQ(theme.preferred_color_scheme(), PrefScheme::kLight);
}

TEST(NativeThemeWinTest, CalculatePreferredContrast) {
  using PrefContrast = NativeTheme::PreferredContrast;

  TestNativeThemeWin theme;

  theme.SetForcedColors(false);
  EXPECT_EQ(theme.preferred_contrast(), PrefContrast::kNoPreference);

  theme.SetForcedColors(true);
  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorBLACK);
  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorWHITE);
  EXPECT_EQ(theme.preferred_contrast(), PrefContrast::kMore);

  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorWHITE);
  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorBLACK);
  EXPECT_EQ(theme.preferred_contrast(), PrefContrast::kMore);

  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorRED);
  EXPECT_EQ(theme.preferred_contrast(), PrefContrast::kCustom);

  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorYELLOW);
  EXPECT_EQ(theme.preferred_contrast(), PrefContrast::kLess);

  theme.SetForcedColors(false);
  EXPECT_EQ(theme.preferred_contrast(), PrefContrast::kNoPreference);
}

TEST(NativeThemeWinTest, TestColorProviderKeyColorMode) {
  TestNativeThemeWin theme;

  theme.SetForcedColors(false);
  theme.SetInDarkMode(true);
  EXPECT_EQ(theme.GetColorMode(), ColorMode::kDark);

  theme.SetInDarkMode(false);
  EXPECT_EQ(theme.GetColorMode(), ColorMode::kLight);

  theme.SetForcedColors(true);
  theme.set_preferred_color_scheme(PrefScheme::kDark);
  EXPECT_EQ(theme.GetColorMode(), ColorMode::kDark);

  theme.set_preferred_color_scheme(PrefScheme::kLight);
  EXPECT_EQ(theme.GetColorMode(), ColorMode::kLight);
}

TEST(NativeThemeWinTest, GetCaretBlinkInterval) {
  TestNativeThemeWin theme;
  static const size_t system_value = ::GetCaretBlinkTime();
  base::TimeDelta actual_interval = theme.GetCaretBlinkInterval();

  if (system_value == 0) {
    // Uses default value when there is no system value.
    EXPECT_EQ(base::Milliseconds(500), actual_interval);
  } else if (system_value == INFINITY) {
    // 0 is the value meaning "don't blink" in Chromium, while Windows uses
    // INFINITY.
    EXPECT_EQ(base::Milliseconds(0), actual_interval);
  } else {
    // Uses system value without modification.
    EXPECT_EQ(base::Milliseconds(system_value), actual_interval);
  }

  // The setter overrides the system value or the default value.
  base::TimeDelta new_interval = base::Milliseconds(42);
  theme.set_caret_blink_interval(new_interval);
  actual_interval = theme.GetCaretBlinkInterval();
  EXPECT_EQ(new_interval, actual_interval);
}

}  // namespace ui
