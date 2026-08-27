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
  EXPECT_TRUE(IsValidThemeName(ThemeProperty::kCursorThemeName, "Adwaita"));
  EXPECT_TRUE(IsValidThemeName(ThemeProperty::kKeyThemeName, ""));
  EXPECT_TRUE(IsValidThemeName(ThemeProperty::kKeyThemeName, nullptr));
  EXPECT_TRUE(IsValidThemeName(ThemeProperty::kCursorThemeName, ""));
  EXPECT_TRUE(IsValidThemeName(ThemeProperty::kCursorThemeName, nullptr));
  EXPECT_FALSE(IsValidThemeName(ThemeProperty::kThemeName, ""));
  EXPECT_FALSE(IsValidThemeName(ThemeProperty::kThemeName, nullptr));
  EXPECT_FALSE(IsValidThemeName(ThemeProperty::kIconThemeName, ""));
  EXPECT_FALSE(IsValidThemeName(ThemeProperty::kIconThemeName, nullptr));
  EXPECT_FALSE(IsValidThemeName(ThemeProperty::kThemeName, "../invalid"));
  EXPECT_FALSE(
      IsValidThemeName(ThemeProperty::kThemeName, "/absolute/invalid"));
  EXPECT_FALSE(IsValidThemeName(ThemeProperty::kThemeName, "."));
  EXPECT_FALSE(IsValidThemeName(ThemeProperty::kCursorThemeName, "../invalid"));
  EXPECT_FALSE(
      IsValidThemeName(ThemeProperty::kCursorThemeName, "/absolute/invalid"));
  EXPECT_FALSE(IsValidThemeName(ThemeProperty::kCursorThemeName, "."));
}

TEST(GtkUtilTest, GetThemeFallback) {
  EXPECT_STREQ(GetThemeFallback(ThemeProperty::kIconThemeName), "hicolor");
  EXPECT_STREQ(GetThemeFallback(ThemeProperty::kThemeName), "Adwaita");
  EXPECT_STREQ(GetThemeFallback(ThemeProperty::kCursorThemeName), "Adwaita");
  EXPECT_EQ(GetThemeFallback(ThemeProperty::kKeyThemeName), nullptr);
}

class GtkUtilInterceptorTest : public testing::Test {
 protected:
  void SetUp() override { InstallGtkSettingsInterceptor(); }
  void TearDown() override { UninstallGtkSettingsInterceptor(); }

  struct PropertyObserver {
    std::string value;
    void OnNotify(const char* prop, GtkSettings* settings, GParamSpec* pspec) {
      gchar* str = nullptr;
      g_object_get(settings, prop, &str, nullptr);
      if (str) {
        value = str;
        g_free(str);
      }
    }
    base::WeakPtrFactory<PropertyObserver> weak_factory{this};
  };
};

TEST_F(GtkUtilInterceptorTest, ThemeNamesSanitizedAtWriteTime) {
  GtkSettings* settings = GetDefaultGtkSettings();
  ASSERT_TRUE(settings);

  PropertyObserver observer;
  ScopedGSignal signal(settings, "notify::gtk-theme-name",
                       base::BindRepeating(&PropertyObserver::OnNotify,
                                           observer.weak_factory.GetWeakPtr(),
                                           "gtk-theme-name"));

  // Set to an invalid value (path traversal)
  g_object_set(settings, "gtk-theme-name", "../../../invalid-theme", nullptr);

  // The interceptor should have triggered and sanitized the theme name to
  // "Adwaita" before the notify callback ran!
  EXPECT_EQ(observer.value, "Adwaita");
}

TEST_F(GtkUtilInterceptorTest, IconThemeNamesSanitizedAtWriteTime) {
  GtkSettings* settings = GetDefaultGtkSettings();
  ASSERT_TRUE(settings);

  PropertyObserver observer;
  ScopedGSignal signal(settings, "notify::gtk-icon-theme-name",
                       base::BindRepeating(&PropertyObserver::OnNotify,
                                           observer.weak_factory.GetWeakPtr(),
                                           "gtk-icon-theme-name"));

  // Set to an invalid value (path traversal)
  g_object_set(settings, "gtk-icon-theme-name", "../../../invalid-theme",
               nullptr);

  // The interceptor should have triggered and sanitized the theme name to
  // "hicolor" before the notify callback ran!
  EXPECT_EQ(observer.value, "hicolor");
}

TEST_F(GtkUtilInterceptorTest, CursorThemeNamesSanitizedAtWriteTime) {
  GtkSettings* settings = GetDefaultGtkSettings();
  ASSERT_TRUE(settings);

  PropertyObserver observer;
  ScopedGSignal signal(settings, "notify::gtk-cursor-theme-name",
                       base::BindRepeating(&PropertyObserver::OnNotify,
                                           observer.weak_factory.GetWeakPtr(),
                                           "gtk-cursor-theme-name"));

  // Set to an invalid value (path traversal)
  g_object_set(settings, "gtk-cursor-theme-name",
               "../../../../tmp/w8_evil_cursor", nullptr);

  // The interceptor should have triggered and sanitized the cursor theme name
  // to "Adwaita" before the notify callback ran!
  EXPECT_EQ(observer.value, "Adwaita");
}

TEST_F(GtkUtilInterceptorTest, GtkModulesSanitizedAtWriteTime) {
  if (GtkCheckVersion(4)) {
    GTEST_SKIP();
  }
  GtkSettings* settings = GetDefaultGtkSettings();
  ASSERT_TRUE(settings);

  PropertyObserver observer;
  ScopedGSignal signal(
      settings, "notify::gtk-modules",
      base::BindRepeating(&PropertyObserver::OnNotify,
                          observer.weak_factory.GetWeakPtr(), "gtk-modules"));

  // Set to a module name
  g_object_set(settings, "gtk-modules", "canberra-gtk-module:pk-gtk-module",
               nullptr);

  // The interceptor should have triggered and sanitized the modules to ""
  EXPECT_EQ(observer.value, "");
}

TEST_F(GtkUtilInterceptorTest, CursorThemeNamesAllowsEmpty) {
  GtkSettings* settings = GetDefaultGtkSettings();
  ASSERT_TRUE(settings);

  std::string observed_theme_name = "initial";
  auto callback = base::BindRepeating(
      [](std::string* out_str, GtkSettings* settings, GParamSpec* pspec) {
        gchar* name = nullptr;
        g_object_get(settings, "gtk-cursor-theme-name", &name, nullptr);
        if (name) {
          *out_str = name;
          g_free(name);
        } else {
          out_str->clear();
        }
      },
      base::Unretained(&observed_theme_name));

  ScopedGSignal signal(settings, "notify::gtk-cursor-theme-name", callback);

  g_object_set(settings, "gtk-cursor-theme-name", "", nullptr);
  EXPECT_EQ(observed_theme_name, "");
}

}  // namespace gtk
