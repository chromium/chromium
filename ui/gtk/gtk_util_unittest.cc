// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_util.h"

#include <glib.h>

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
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

TEST(GtkUtilTest, IsGdkFatalErrorMessage) {
  // Fatal Wayland error messages.
  EXPECT_TRUE(
      IsGdkFatalErrorMessage("Gdk", "Lost connection to Wayland compositor."));
  EXPECT_TRUE(IsGdkFatalErrorMessage(
      "Gdk", "Error 32 (Broken pipe) dispatching to Wayland display."));
  EXPECT_TRUE(IsGdkFatalErrorMessage(
      "Gdk", "Error reading events from display: Broken pipe"));
  EXPECT_TRUE(
      IsGdkFatalErrorMessage("Gdk", "Error flushing display: Broken pipe"));

  // Fatal X11 error messages.
  EXPECT_TRUE(IsGdkFatalErrorMessage(
      "Gdk",
      "Fatal IO error 11 (Resource temporarily unavailable) on X server :0."));

  // Non-fatal GDK messages.
  EXPECT_FALSE(IsGdkFatalErrorMessage("Gdk", "Window 0x1234 is not mapped"));
  EXPECT_FALSE(IsGdkFatalErrorMessage("Gdk", ""));

  // Fatal message strings from other domains must not trigger.
  EXPECT_FALSE(
      IsGdkFatalErrorMessage("Gtk", "Lost connection to Wayland compositor."));
  EXPECT_FALSE(
      IsGdkFatalErrorMessage("GLib", "Lost connection to Wayland compositor."));
  EXPECT_FALSE(
      IsGdkFatalErrorMessage("", "Lost connection to Wayland compositor."));
}

TEST(GtkUtilTest, GtkLogWriterFatalDisconnectIntercepted) {
  InstallGtkLogWriter();

  bool shutdown_called = false;
  SetGtkShutdownCb(base::BindLambdaForTesting([&] { shutdown_called = true; }));

  // Non-fatal GDK message should not trigger the callback.
  g_log_structured("Gdk", G_LOG_LEVEL_MESSAGE, "MESSAGE",
                   "Normal non-fatal informational message");
  EXPECT_FALSE(shutdown_called);

  // Message from another domain should not trigger the callback.
  g_log_structured("Gtk", G_LOG_LEVEL_MESSAGE, "MESSAGE",
                   "Lost connection to Wayland compositor.");
  EXPECT_FALSE(shutdown_called);

  // Fatal GDK message must trigger the shutdown callback.
  g_log_structured("Gdk", G_LOG_LEVEL_MESSAGE, "MESSAGE",
                   "Lost connection to Wayland compositor.");
  EXPECT_TRUE(shutdown_called);

  // Clean up.
  SetGtkShutdownCb(base::NullCallback());
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
