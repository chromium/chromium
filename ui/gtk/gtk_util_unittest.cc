// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_util.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/gtk/gtk_compat.h"

namespace gtk {

TEST(GtkUtilTest, IsValidThemeName) {
  EXPECT_TRUE(IsValidThemeName(ThemeProperty::kThemeName, "Adwaita"));
  EXPECT_TRUE(IsValidThemeName(ThemeProperty::kIconThemeName, "hicolor"));
  EXPECT_TRUE(IsValidThemeName(ThemeProperty::kKeyThemeName, ""));
  EXPECT_FALSE(IsValidThemeName(ThemeProperty::kThemeName, ""));
  EXPECT_FALSE(IsValidThemeName(ThemeProperty::kThemeName, "../invalid"));
  EXPECT_FALSE(
      IsValidThemeName(ThemeProperty::kThemeName, "/absolute/invalid"));
  EXPECT_FALSE(IsValidThemeName(ThemeProperty::kThemeName, "."));
}

TEST(GtkUtilTest, GetThemeFallback) {
  EXPECT_STREQ(GetThemeFallback(ThemeProperty::kIconThemeName), "hicolor");
  EXPECT_STREQ(GetThemeFallback(ThemeProperty::kThemeName), "Adwaita");
  EXPECT_EQ(GetThemeFallback(ThemeProperty::kKeyThemeName), nullptr);
}

class GtkUtilInterceptorTest : public testing::Test {
 protected:
  void SetUp() override { InstallGtkSettingsInterceptor(); }
  void TearDown() override { UninstallGtkSettingsInterceptor(); }
};

TEST_F(GtkUtilInterceptorTest, ThemeNamesSanitizedAtWriteTime) {
  GtkSettings* settings = GetDefaultGtkSettings();
  ASSERT_TRUE(settings);

  std::string observed_theme_name;
  auto callback = base::BindRepeating(
      [](std::string* out_str, GtkSettings* settings, GParamSpec* pspec) {
        gchar* name = nullptr;
        g_object_get(settings, "gtk-theme-name", &name, nullptr);
        if (name) {
          *out_str = name;
          g_free(name);
        }
      },
      base::Unretained(&observed_theme_name));

  ScopedGSignal signal(settings, "notify::gtk-theme-name", callback);

  // Set to an invalid value (path traversal)
  g_object_set(settings, "gtk-theme-name", "../../../invalid-theme", nullptr);

  // The interceptor should have triggered and sanitized the theme name to
  // "Adwaita" before the notify callback ran!
  EXPECT_EQ(observed_theme_name, "Adwaita");
}

TEST_F(GtkUtilInterceptorTest, IconThemeNamesSanitizedAtWriteTime) {
  GtkSettings* settings = GetDefaultGtkSettings();
  ASSERT_TRUE(settings);

  std::string observed_theme_name;
  auto callback = base::BindRepeating(
      [](std::string* out_str, GtkSettings* settings, GParamSpec* pspec) {
        gchar* name = nullptr;
        g_object_get(settings, "gtk-icon-theme-name", &name, nullptr);
        if (name) {
          *out_str = name;
          g_free(name);
        }
      },
      base::Unretained(&observed_theme_name));

  ScopedGSignal signal(settings, "notify::gtk-icon-theme-name", callback);

  // Set to an invalid value (path traversal)
  g_object_set(settings, "gtk-icon-theme-name", "../../../invalid-theme",
               nullptr);

  // The interceptor should have triggered and sanitized the theme name to
  // "hicolor" before the notify callback ran!
  EXPECT_EQ(observed_theme_name, "hicolor");
}

}  // namespace gtk
