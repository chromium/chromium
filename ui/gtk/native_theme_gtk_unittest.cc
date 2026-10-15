// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/native_theme_gtk.h"

#include <glib-object.h>
#include <glib.h>

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_util.h"
#include "ui/gtk/os_settings_provider_gtk.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/os_settings_provider.h"

namespace gtk {

namespace {

using PreferredColorScheme = ui::NativeTheme::PreferredColorScheme;

class NativeThemeGtkTest : public testing::Test {
 protected:
  void SetUp() override {
    settings_ = GetDefaultGtkSettings();
    ASSERT_TRUE(settings_);
    gchar* theme_name = nullptr;
    g_object_get(settings_, "gtk-theme-name", &theme_name,
                 "gtk-application-prefer-dark-theme", &original_prefer_dark_,
                 nullptr);
    original_theme_name_ = theme_name ? theme_name : "";
    g_free(theme_name);
  }

  void TearDown() override {
    ui::NativeTheme::SetPreferredColorSchemeOverride(std::nullopt);
    g_object_set(settings_, "gtk-theme-name", original_theme_name_.c_str(),
                 "gtk-application-prefer-dark-theme", original_prefer_dark_,
                 nullptr);
    ClearStyleColorCache();
    provider_.OnThemeChanged();
  }

  // Selects GTK's built-in Adwaita theme, in its dark or light variant, and
  // notifies observers the way GtkUi does after a theme change.
  void UseAdwaita(bool dark) {
    g_object_set(settings_, "gtk-theme-name", "Adwaita",
                 "gtk-application-prefer-dark-theme", dark, nullptr);
    ClearStyleColorCache();
    provider_.OnThemeChanged();
  }

  // Simulates the xdg-desktop-portal color-scheme preference.
  void SetPortalPrefersDark(bool dark) { provider_.SetColorScheme(dark); }

  PreferredColorScheme GtkThemeScheme() const {
    return NativeThemeGtk::instance()->preferred_color_scheme();
  }

  OsSettingsProviderGtk provider_;

 private:
  raw_ptr<GtkSettings> settings_ = nullptr;
  std::string original_theme_name_;
  gboolean original_prefer_dark_ = false;
};

// A dark-only GTK theme with a light desktop preference must keep the GTK
// theme's scheme dark, so Chrome UI painted with GTK colors is consistent.
// The desktop preference still applies to everything else.
TEST_F(NativeThemeGtkTest, DarkThemeLightPreferenceStaysDark) {
  SetPortalPrefersDark(false);
  UseAdwaita(/*dark=*/true);
  EXPECT_EQ(GtkThemeScheme(), PreferredColorScheme::kDark);
  EXPECT_EQ(ui::OsSettingsProvider::Get().PreferredColorScheme(),
            PreferredColorScheme::kLight);
}

TEST_F(NativeThemeGtkTest, LightThemeDarkPreferenceStaysLight) {
  SetPortalPrefersDark(true);
  UseAdwaita(/*dark=*/false);
  EXPECT_EQ(GtkThemeScheme(), PreferredColorScheme::kLight);
  EXPECT_EQ(ui::OsSettingsProvider::Get().PreferredColorScheme(),
            PreferredColorScheme::kDark);
}

// Switching the GTK theme variant updates the scheme.
TEST_F(NativeThemeGtkTest, FollowsThemeVariant) {
  UseAdwaita(/*dark=*/true);
  EXPECT_EQ(GtkThemeScheme(), PreferredColorScheme::kDark);
  UseAdwaita(/*dark=*/false);
  EXPECT_EQ(GtkThemeScheme(), PreferredColorScheme::kLight);
  UseAdwaita(/*dark=*/true);
  EXPECT_EQ(GtkThemeScheme(), PreferredColorScheme::kDark);
}

// The process-wide override (e.g. --force-dark-mode) still wins.
TEST_F(NativeThemeGtkTest, OverrideTakesPrecedence) {
  UseAdwaita(/*dark=*/false);
  ui::NativeTheme::SetPreferredColorSchemeOverride(PreferredColorScheme::kDark);
  EXPECT_EQ(GtkThemeScheme(), PreferredColorScheme::kDark);
}

}  // namespace

}  // namespace gtk
