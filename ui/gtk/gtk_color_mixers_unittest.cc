// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_color_mixers.h"

#include <glib-object.h>
#include <glib.h>

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixers.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/system_theme.h"
#include "ui/gfx/color_utils.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_util.h"

namespace gtk {

namespace {

using ColorMode = ui::ColorProviderKey::ColorMode;

class GtkColorMixersTest : public testing::Test {
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
    g_object_set(settings_, "gtk-theme-name", original_theme_name_.c_str(),
                 "gtk-application-prefer-dark-theme", original_prefer_dark_,
                 nullptr);
    ClearStyleColorCache();
  }

  // Selects GTK's built-in Adwaita theme, in its dark or light variant.
  void UseAdwaita(bool dark) {
    g_object_set(settings_, "gtk-theme-name", "Adwaita",
                 "gtk-application-prefer-dark-theme", dark, nullptr);
    ClearStyleColorCache();
  }

  // Builds a provider like the production pipeline: the core UI mixers
  // followed by the GTK mixer.
  std::unique_ptr<ui::ColorProvider> CreateProvider(ColorMode color_mode) {
    ui::ColorProviderKey key;
    key.color_mode = color_mode;
    key.system_theme = ui::SystemTheme::kGtk;
    auto provider = std::make_unique<ui::ColorProvider>();
    ui::AddColorMixers(provider.get(), key);
    AddGtkNativeColorMixer(provider.get(), key, std::nullopt);
    return provider;
  }

  void ExpectReadable(const ui::ColorProvider& provider,
                      ui::ColorId foreground,
                      ui::ColorId background) {
    const SkColor bg = provider.GetColor(background);
    const SkColor fg =
        color_utils::GetResultingPaintColor(provider.GetColor(foreground), bg);
    EXPECT_GE(color_utils::GetContrastRatio(fg, bg),
              color_utils::kMinimumReadableContrastRatio)
        << ui::ColorIdName(foreground) << " " << ui::SkColorName(fg) << " on "
        << ui::ColorIdName(background) << " " << ui::SkColorName(bg);
  }

  // Content text is painted on Material surfaces (WebUI cards, Views dialogs),
  // which follow the color mode rather than the GTK theme.
  void ExpectContentReadable(const ui::ColorProvider& provider) {
    for (ui::ColorId surface :
         {ui::kColorSysSurface, ui::kColorDialogBackground}) {
      ExpectReadable(provider, ui::kColorPrimaryForeground, surface);
      ExpectReadable(provider, ui::kColorSecondaryForeground, surface);
      ExpectReadable(provider, ui::kColorButtonForeground, surface);
      ExpectReadable(provider, ui::kColorLabelForeground, surface);
    }
    ExpectReadable(provider, ui::kColorDialogForeground,
                   ui::kColorDialogBackground);
  }

  // Tabs and toolbar text are painted on GTK surfaces and must stay readable.
  void ExpectToolbarReadable(const ui::ColorProvider& provider) {
    ExpectReadable(provider, ui::kColorTabForegroundSelected,
                   ui::kColorNativeToolbarBackground);
    ExpectReadable(provider, ui::kColorNativeLabelForeground,
                   ui::kColorNativeToolbarBackground);
  }

 private:
  raw_ptr<GtkSettings> settings_ = nullptr;
  std::string original_theme_name_;
  gboolean original_prefer_dark_ = FALSE;
};

}  // namespace

// Regression test for crbug.com/565897998: a dark GTK theme with a light color
// mode (e.g. portal color-scheme "no preference") made chrome://settings text
// light-on-light.
TEST_F(GtkColorMixersTest, DarkThemeLightModeReadable) {
  UseAdwaita(/*dark=*/true);
  auto provider = CreateProvider(ColorMode::kLight);
  ASSERT_TRUE(color_utils::IsDark(
      provider->GetColor(ui::kColorNativeToolbarBackground)));
  ExpectContentReadable(*provider);
  ExpectToolbarReadable(*provider);
}

TEST_F(GtkColorMixersTest, LightThemeDarkModeReadable) {
  UseAdwaita(/*dark=*/false);
  auto provider = CreateProvider(ColorMode::kDark);
  ASSERT_FALSE(color_utils::IsDark(
      provider->GetColor(ui::kColorNativeToolbarBackground)));
  ExpectContentReadable(*provider);
  ExpectToolbarReadable(*provider);
}

// When the GTK theme matches the color mode, the GTK colors are still used.
TEST_F(GtkColorMixersTest, MatchingPaletteUsesGtkColors) {
  for (bool dark : {false, true}) {
    SCOPED_TRACE(dark ? "dark" : "light");
    UseAdwaita(dark);
    auto provider = CreateProvider(dark ? ColorMode::kDark : ColorMode::kLight);
    EXPECT_EQ(provider->GetColor(ui::kColorPrimaryForeground),
              provider->GetColor(ui::kColorNativeLabelForeground));
    EXPECT_EQ(provider->GetColor(ui::kColorTabForegroundSelected),
              provider->GetColor(ui::kColorNativeLabelForeground));
    // Secondary text contrast is up to the theme here (Adwaita-dark's
    // label:disabled color is just under 4.5:1), so only check primary text.
    ExpectReadable(*provider, ui::kColorPrimaryForeground,
                   ui::kColorSysSurface);
    ExpectReadable(*provider, ui::kColorPrimaryForeground,
                   ui::kColorPrimaryBackground);
    ExpectToolbarReadable(*provider);
  }
}

}  // namespace gtk
