// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/os_settings_provider_android.h"

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/os_settings_provider.h"

namespace ui {
namespace {

class OsSettingsProviderAndroidTest : public testing::Test {
 protected:
  OsSettingsProviderAndroidTest() = default;
  ~OsSettingsProviderAndroidTest() override = default;
};

TEST_F(OsSettingsProviderAndroidTest,
       SetPreferredColorSchemeNotifiesObservers) {
  // Ensure the production provider is instantiated.
  OsSettingsProvider::Get();
  auto& provider =
      static_cast<OsSettingsProviderAndroid&>(OsSettingsProvider::Get());

  // Save original state to restore at the end of the test
  auto original_scheme = provider.PreferredColorScheme();

  bool notified = false;
  auto subscription =
      provider.RegisterOsSettingsChangedCallback(base::BindRepeating(
          [](bool* notified, bool force_notify) { *notified = true; },
          &notified));

  // Test transitions
  provider.SetPreferredColorScheme(NativeTheme::PreferredColorScheme::kDark);
  EXPECT_EQ(provider.PreferredColorScheme(),
            NativeTheme::PreferredColorScheme::kDark);
  EXPECT_TRUE(notified);

  notified = false;
  provider.SetPreferredColorScheme(NativeTheme::PreferredColorScheme::kLight);
  EXPECT_EQ(provider.PreferredColorScheme(),
            NativeTheme::PreferredColorScheme::kLight);
  EXPECT_TRUE(notified);

  // Redundant transitions must not trigger notifications
  notified = false;
  provider.SetPreferredColorScheme(NativeTheme::PreferredColorScheme::kLight);
  EXPECT_FALSE(notified);

  // Restore original state
  provider.SetPreferredColorScheme(original_scheme);
}

}  // namespace
}  // namespace ui
