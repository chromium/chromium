// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include <optional>
#include <utility>

#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_provider_key.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

namespace ui {
namespace {

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

TEST_F(NativeThemeTest, PreferredColorScheme) {
  using enum NativeTheme::PreferredColorScheme;
  const auto* const native_theme = NativeTheme::GetInstanceForNativeUi();

  EXPECT_EQ(native_theme->preferred_color_scheme(), kLight);

  os_settings_provider().SetPreferredColorScheme(kDark);
  EXPECT_EQ(native_theme->preferred_color_scheme(), kDark);

  os_settings_provider().SetPreferredColorScheme(kNoPreference);
  EXPECT_EQ(native_theme->preferred_color_scheme(), kNoPreference);
}

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

TEST_F(NativeThemeTest, ColorMode) {
  using enum NativeTheme::PreferredColorScheme;
  const auto* const native_theme = NativeTheme::GetInstanceForNativeUi();

  os_settings_provider().SetPreferredColorScheme(kDark);
  EXPECT_EQ(native_theme->GetColorProviderKey(nullptr).color_mode,
            ColorProviderKey::ColorMode::kDark);

  os_settings_provider().SetPreferredColorScheme(kLight);
  EXPECT_EQ(native_theme->GetColorProviderKey(nullptr).color_mode,
            ColorProviderKey::ColorMode::kLight);

  os_settings_provider().SetForcedColorsActive(true);
  os_settings_provider().SetPreferredColorScheme(kDark);
  EXPECT_EQ(native_theme->GetColorProviderKey(nullptr).color_mode,
            ColorProviderKey::ColorMode::kDark);

  os_settings_provider().SetPreferredColorScheme(kLight);
  EXPECT_EQ(native_theme->GetColorProviderKey(nullptr).color_mode,
            ColorProviderKey::ColorMode::kLight);
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

TEST_F(NativeThemeTest, DelayScoper) {
  // Monitor calls to `OnNativeThemeUpdated()`.
  struct MockObserver : NativeThemeObserver {
    void OnNativeThemeUpdated(NativeTheme* observed_theme) override {
      ++call_count;
    }

    int call_count = 0;
  } observer;
  base::ScopedObservation<NativeTheme, NativeThemeObserver> observation(
      &observer);
  observation.Observe(NativeTheme::GetInstanceForNativeUi());

  const auto expect_notification_count = [&](int n) {
    EXPECT_EQ(std::exchange(observer.call_count, 0), n);
  };

  // Sanity check: setting the color should normally notify.
  os_settings_provider().SetAccentColor(SK_ColorRED);
  expect_notification_count(1);

  // When there are scopers alive, there should be no notifications.
  std::optional<NativeTheme::UpdateNotificationDelayScoper> scoper_1, scoper_2;
  scoper_1.emplace();
  scoper_2.emplace();
  os_settings_provider().SetAccentColor(SK_ColorGREEN);
  expect_notification_count(0);

  // Destroying some, but not all scopers should still not notify.
  scoper_2.reset();
  expect_notification_count(0);

  // Since there are still scopers, further changes should still not notify.
  os_settings_provider().SetAccentColor(SK_ColorBLUE);
  expect_notification_count(0);

  // When the last scoper is destroyed, there should only be one notification,
  // even though there were multiple changes above.
  scoper_1.reset();
  expect_notification_count(1);
}

}  // namespace
}  // namespace ui
