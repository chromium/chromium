// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/native_theme_gtk.h"

#include <glib-object.h>
#include <glib.h>

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_ui_factory.h"
#include "ui/gtk/gtk_util.h"
#include "ui/gtk/os_settings_provider_gtk.h"
#include "ui/gtk/select_file_dialog_linux_gtk.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_delegate.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
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
    SelectFileDialogLinuxGtk::ResetForTesting();
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
  raw_ptr<GtkSettings> settings_ = nullptr;

 private:
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

// Regression test for crbug.com/571161438: the Material palettes (focus rings,
// tab hover, ...) must be seeded from the desktop accent color.
TEST_F(NativeThemeGtkTest, UserColorFollowsDesktopAccent) {
  constexpr SkColor kAccent = SkColorSetRGB(0xE9, 0x54, 0x20);
  UseAdwaita(/*dark=*/false);
  provider_.SetAccentColor(kAccent);
  EXPECT_EQ(provider_.AccentColor(), kAccent);
  auto* const native_theme = NativeThemeGtk::instance();
  EXPECT_EQ(native_theme->user_color(), kAccent);
  const ui::ColorProviderKey key = native_theme->GetColorProviderKey(nullptr);
  EXPECT_EQ(key.user_color, kAccent);
  EXPECT_EQ(key.user_color_source,
            ui::ColorProviderKey::UserColorSource::kAccent);
}

// Without a desktop accent color, the GTK theme's own accent is used.
TEST_F(NativeThemeGtkTest, UserColorFallsBackToThemeAccent) {
  provider_.SetAccentColor(std::nullopt);
  for (bool dark : {false, true}) {
    SCOPED_TRACE(dark ? "dark" : "light");
    UseAdwaita(dark);
    const SkColor theme_accent =
        SkColorSetA(GetThemeAccentColor(), SK_AlphaOPAQUE);
    EXPECT_EQ(provider_.AccentColor(), theme_accent);
    EXPECT_EQ(NativeThemeGtk::instance()->user_color(), theme_accent);
  }
}

// `g_object_set()` resets GTK CSS values and emits
// `notify::gtk-application-prefer-dark-theme` even when setting the same value.
// `GtkUi::SetDarkTheme()` should skip redundant updates while still applying
// genuine dark/light changes.
TEST_F(NativeThemeGtkTest, SetDarkThemeSkipsRedundantUpdates) {
  class StubLinuxUiDelegate : public ui::LinuxUiDelegate {
   public:
    // ui::LinuxUiDelegate:
    ui::LinuxUiBackend GetBackend() const override {
      return ui::LinuxUiBackend::kStub;
    }
    void ExportWindowHandle(
        gfx::AcceleratedWidget window_id,
        base::OnceCallback<void(std::string)> callback) override {
      std::move(callback).Run({});
    }
  };

  class CountingNativeThemeObserver : public ui::NativeThemeObserver {
   public:
    // ui::NativeThemeObserver:
    void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override {
      ++count_;
    }

    int count() const { return count_; }
    void Reset() { count_ = 0; }

   private:
    int count_ = 0;
  };

  struct ScopedColorProviderManagerReset {
    ~ScopedColorProviderManagerReset() {
      ui::ColorProviderManager::ResetForTesting();
    }
  };

  UseAdwaita(/*dark=*/false);

  StubLinuxUiDelegate delegate;
  std::unique_ptr<ui::LinuxUiAndTheme> gtk_ui = BuildGtkUi();
  ScopedColorProviderManagerReset reset_color_provider_manager;
  ASSERT_TRUE(gtk_ui->Initialize());
  ASSERT_FALSE(gtk_ui->PreferDarkTheme());
  ASSERT_EQ(GtkThemeScheme(), PreferredColorScheme::kLight);

  int gtk_notify_count = 0;
  ScopedGSignal notify_signal(
      settings_.get(), "notify::gtk-application-prefer-dark-theme",
      base::BindRepeating(
          [](int* count, GtkSettings*, GParamSpec*) { ++(*count); },
          &gtk_notify_count));
  ASSERT_TRUE(notify_signal.Connected());

  CountingNativeThemeObserver theme_observer;
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver> observation(
      &theme_observer);
  observation.Observe(gtk_ui->GetNativeTheme());

  // Redundant light -> light update must not re-parse GTK CSS or notify
  // theme observers.
  gtk_ui->SetDarkTheme(false);
  EXPECT_FALSE(gtk_ui->PreferDarkTheme());
  EXPECT_EQ(gtk_notify_count, 0);
  EXPECT_EQ(theme_observer.count(), 0);
  EXPECT_EQ(GtkThemeScheme(), PreferredColorScheme::kLight);

  // Genuine light -> dark update must apply and notify observers.
  gtk_ui->SetDarkTheme(true);
  EXPECT_TRUE(gtk_ui->PreferDarkTheme());
  EXPECT_EQ(gtk_notify_count, 1);
  EXPECT_GT(theme_observer.count(), 0);
  EXPECT_EQ(GtkThemeScheme(), PreferredColorScheme::kDark);

  // Redundant dark -> dark update must also be skipped.
  theme_observer.Reset();
  gtk_ui->SetDarkTheme(true);
  EXPECT_TRUE(gtk_ui->PreferDarkTheme());
  EXPECT_EQ(gtk_notify_count, 1);
  EXPECT_EQ(theme_observer.count(), 0);
  EXPECT_EQ(GtkThemeScheme(), PreferredColorScheme::kDark);

  // Genuine dark -> light update must apply and notify observers.
  gtk_ui->SetDarkTheme(false);
  EXPECT_FALSE(gtk_ui->PreferDarkTheme());
  EXPECT_EQ(gtk_notify_count, 2);
  EXPECT_GT(theme_observer.count(), 0);
  EXPECT_EQ(GtkThemeScheme(), PreferredColorScheme::kLight);
}

}  // namespace

}  // namespace gtk
