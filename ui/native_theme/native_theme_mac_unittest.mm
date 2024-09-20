// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_mac.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cocoa/defaults_utils.h"

namespace ui {

class TestNativeThemeMac : public NativeThemeMac {
 public:
  TestNativeThemeMac() : NativeThemeMac(false, false) {}
  TestNativeThemeMac& operator=(const TestNativeThemeMac&) = delete;

  ~TestNativeThemeMac() override = default;
};

TEST(NativeThemeMacTest, GetPlatformHighContrastColorScheme) {
  using PrefScheme = NativeTheme::PreferredColorScheme;
  using PrefContrast = NativeTheme::PreferredContrast;

  constexpr NativeTheme::PlatformHighContrastColorScheme kNone =
      NativeTheme::PlatformHighContrastColorScheme::kNone;

  NativeTheme* native_theme = NativeTheme::GetInstanceForNativeUi();
  ASSERT_TRUE(native_theme);

  native_theme->set_forced_colors(false);
  native_theme->SetPreferredContrast(PrefContrast::kNoPreference);
  native_theme->set_preferred_color_scheme(PrefScheme::kDark);
  EXPECT_EQ(native_theme->GetPlatformHighContrastColorScheme(), kNone);

  native_theme->set_preferred_color_scheme(PrefScheme::kLight);
  EXPECT_EQ(native_theme->GetPlatformHighContrastColorScheme(), kNone);

  native_theme->set_forced_colors(true);
  native_theme->SetPreferredContrast(PrefContrast::kMore);
  native_theme->set_preferred_color_scheme(PrefScheme::kDark);
  EXPECT_EQ(native_theme->GetPlatformHighContrastColorScheme(), kNone);

  native_theme->set_preferred_color_scheme(PrefScheme::kLight);
  EXPECT_EQ(native_theme->GetPlatformHighContrastColorScheme(), kNone);

  native_theme->set_forced_colors(false);
  native_theme->SetPreferredContrast(PrefContrast::kNoPreference);
  EXPECT_EQ(native_theme->GetPlatformHighContrastColorScheme(), kNone);
}

TEST(NativeThemeMacTest, ThumbSize) {
  EXPECT_EQ(gfx::Size(6.0, 18.0), NativeThemeMac::GetThumbMinSize(true, 1.0));
  EXPECT_EQ(gfx::Size(18.0, 6.0), NativeThemeMac::GetThumbMinSize(false, 1.0));
  EXPECT_EQ(gfx::Size(12.0, 36.0), NativeThemeMac::GetThumbMinSize(true, 2.0));
  EXPECT_EQ(gfx::Size(36.0, 12.0), NativeThemeMac::GetThumbMinSize(false, 2.0));
}

TEST(NativeThemeMacTest, GetCaretBlinkInterval) {
  TestNativeThemeMac theme;
  std::optional<base::TimeDelta> system_value(
      ui::TextInsertionCaretBlinkPeriodFromDefaults());
  base::TimeDelta actual_interval = theme.GetCaretBlinkInterval();

  if (system_value.has_value()) {
    // Uses system value.
    EXPECT_EQ(system_value.value(), actual_interval);
  } else {
    // Uses default value.
    EXPECT_EQ(base::Milliseconds(500), actual_interval);
  }
  // The setter overrides the system value or the default value.
  base::TimeDelta new_interval = base::Milliseconds(42);
  theme.set_caret_blink_interval(new_interval);
  actual_interval = theme.GetCaretBlinkInterval();
  EXPECT_EQ(new_interval, actual_interval);
}

}  // namespace ui
