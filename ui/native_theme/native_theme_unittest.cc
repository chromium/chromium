// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_provider_key.h"
#include "ui/native_theme/mock_os_settings_provider.h"

namespace ui {

namespace {

class TestNativeTheme : public NativeTheme {
 public:
  TestNativeTheme() = default;
  TestNativeTheme(const TestNativeTheme&) = delete;
  TestNativeTheme& operator=(const TestNativeTheme&) = delete;
  ~TestNativeTheme() override = default;

  ColorProviderKey::ForcedColors GetForcedColorsKey() const {
    return GetColorProviderKey(/*custom_theme=*/nullptr).forced_colors;
  }
};

}  // namespace

class NativeThemeTest : public ::testing::Test {
 protected:
  NativeThemeTest() = default;
  ~NativeThemeTest() override = default;

  MockOsSettingsProvider& os_settings_provider() {
    return os_settings_provider_;
  }

 private:
  MockOsSettingsProvider os_settings_provider_;
};

TEST_F(NativeThemeTest, PreferredContrast) {
  using enum NativeTheme::PreferredContrast;
  const auto* const native_theme = NativeTheme::GetInstanceForNativeUi();

  EXPECT_EQ(native_theme->preferred_contrast(), kNoPreference);

  os_settings_provider().SetPreferredContrast(kMore);
  EXPECT_EQ(native_theme->preferred_contrast(), kMore);

  os_settings_provider().SetPreferredContrast(kCustom);
  EXPECT_EQ(native_theme->preferred_contrast(), kCustom);

  os_settings_provider().SetPreferredContrast(kLess);
  EXPECT_EQ(native_theme->preferred_contrast(), kLess);
}

TEST_F(NativeThemeTest, UserColor) {
  static constexpr auto kAccentColor = SkColorSetRGB(135, 115, 10);
  os_settings_provider().SetAccentColor(kAccentColor);
  EXPECT_EQ(kAccentColor, NativeTheme::GetInstanceForNativeUi()->user_color());
}

TEST_F(NativeThemeTest, CaretBlinkInterval) {
  auto* const native_theme = NativeTheme::GetInstanceForNativeUi();
  static constexpr auto kNewInterval = base::Milliseconds(42);
  native_theme->set_caret_blink_interval(kNewInterval);
  EXPECT_EQ(native_theme->caret_blink_interval(), kNewInterval);

  native_theme->set_caret_blink_interval(base::TimeDelta());
  EXPECT_EQ(native_theme->caret_blink_interval(), base::TimeDelta());
}

TEST_F(NativeThemeTest, MetricsEmitted) {
  auto* const native_theme = NativeTheme::GetInstanceForNativeUi();
  base::HistogramTester histogram_tester;

  native_theme->NotifyOnNativeThemeUpdated();
  histogram_tester.ExpectTotalCount(
      "Views.Browser.TimeSpentProcessingOnNativeThemeUpdatedEvent", 1);
  histogram_tester.ExpectUniqueSample(
      "Views.Browser.NumColorProvidersInitializedDuringOnNativeThemeUpdated", 0,
      1);

  native_theme->NotifyOnNativeThemeUpdated();
  histogram_tester.ExpectTotalCount(
      "Views.Browser.TimeSpentProcessingOnNativeThemeUpdatedEvent", 2);
  histogram_tester.ExpectUniqueSample(
      "Views.Browser.NumColorProvidersInitializedDuringOnNativeThemeUpdated", 0,
      2);
}

}  // namespace ui
