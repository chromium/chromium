// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/os_settings_provider.h"

#include <optional>

#include "base/containers/flat_map.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/native_theme/native_theme.h"

namespace ui {
namespace {

// A cut-down version of `MockOsSettingsProvider` that intentionally does not
// override the `PreferredColorScheme()`/`PreferredContrast()` methods, so the
// default implementations can be tested.
class SysColorsOsSettingsProvider : public OsSettingsProvider {
 public:
  SysColorsOsSettingsProvider() : OsSettingsProvider(PriorityLevel::kTesting) {}
  ~SysColorsOsSettingsProvider() override = default;

  // OsSettingsProvider:
  bool ForcedColorsActive() const override;
  std::optional<SkColor> Color(ColorId color_id) const override;

  // Setters for all the above settings.
  void SetForcedColorsActive(bool forced_colors_active);
  void SetColor(ColorId color_id, SkColor color);

 private:
  bool forced_colors_active_ = false;
  base::flat_map<ColorId, SkColor> colors_;
};

bool SysColorsOsSettingsProvider::ForcedColorsActive() const {
  return forced_colors_active_;
}

std::optional<SkColor> SysColorsOsSettingsProvider::Color(
    ColorId color_id) const {
  const auto it = colors_.find(color_id);
  return (it == colors_.end()) ? std::nullopt : std::make_optional(it->second);
}

void SysColorsOsSettingsProvider::SetForcedColorsActive(
    bool forced_colors_active) {
  forced_colors_active_ = forced_colors_active;
  NotifyOnSettingsChanged();
}

void SysColorsOsSettingsProvider::SetColor(ColorId color_id, SkColor color) {
  colors_[color_id] = color;
  NotifyOnSettingsChanged();
}

class OsSettingsProviderTest : public ::testing::Test {
 protected:
  OsSettingsProviderTest() = default;
  ~OsSettingsProviderTest() override = default;

  SysColorsOsSettingsProvider& os_settings_provider() {
    return os_settings_provider_;
  }

 private:
  SysColorsOsSettingsProvider os_settings_provider_;
};

TEST_F(OsSettingsProviderTest, PreferredColorScheme) {
  using enum NativeTheme::PreferredColorScheme;
  using enum OsSettingsProvider::ColorId;

  EXPECT_EQ(os_settings_provider().PreferredColorScheme(), kNoPreference);

  os_settings_provider().SetForcedColorsActive(true);
  os_settings_provider().SetColor(kWindow, SK_ColorBLACK);
  EXPECT_EQ(os_settings_provider().PreferredColorScheme(), kDark);

  os_settings_provider().SetColor(kWindow, SK_ColorWHITE);
  EXPECT_EQ(os_settings_provider().PreferredColorScheme(), kLight);

  os_settings_provider().SetColor(kWindow, SK_ColorBLUE);
  EXPECT_EQ(os_settings_provider().PreferredColorScheme(), kDark);

  os_settings_provider().SetColor(kWindow, SK_ColorYELLOW);
  EXPECT_EQ(os_settings_provider().PreferredColorScheme(), kLight);

  os_settings_provider().SetForcedColorsActive(false);
  EXPECT_EQ(os_settings_provider().PreferredColorScheme(), kNoPreference);
}

TEST_F(OsSettingsProviderTest, PreferredContrast) {
  using enum NativeTheme::PreferredContrast;
  using enum OsSettingsProvider::ColorId;

  EXPECT_EQ(os_settings_provider().PreferredContrast(), kNoPreference);

  os_settings_provider().SetForcedColorsActive(true);
  os_settings_provider().SetColor(kWindow, SK_ColorBLACK);
  os_settings_provider().SetColor(kWindowText, SK_ColorWHITE);
  EXPECT_EQ(os_settings_provider().PreferredContrast(), kMore);

  os_settings_provider().SetColor(kWindow, SK_ColorWHITE);
  os_settings_provider().SetColor(kWindowText, SK_ColorBLACK);
  EXPECT_EQ(os_settings_provider().PreferredContrast(), kMore);

  os_settings_provider().SetColor(kWindowText, SK_ColorRED);
  EXPECT_EQ(os_settings_provider().PreferredContrast(), kCustom);

  os_settings_provider().SetColor(kWindowText, SK_ColorYELLOW);
  EXPECT_EQ(os_settings_provider().PreferredContrast(), kLess);

  os_settings_provider().SetForcedColorsActive(false);
  EXPECT_EQ(os_settings_provider().PreferredContrast(), kNoPreference);
}

}  // namespace
}  // namespace ui
