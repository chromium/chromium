// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib-object.h>
#include <glib.h>

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_ui_factory.h"
#include "ui/gtk/gtk_util.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_delegate.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

namespace gtk {

namespace {

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

 private:
  int count_ = 0;
};

class GtkUiTest : public testing::Test {
 protected:
  void SetUp() override {
    settings_ = GetDefaultGtkSettings();
    ASSERT_TRUE(settings_);
    gchar* font_name = nullptr;
    g_object_get(settings_, "gtk-font-name", &font_name, nullptr);
    original_font_name_ = font_name ? font_name : "";
    g_free(font_name);
  }

  void TearDown() override {
    g_object_set(settings_, "gtk-font-name", original_font_name_.c_str(),
                 nullptr);
    ui::ColorProviderManager::ResetForTesting();
  }

  void SetGtkFontName(const char* font_name) {
    g_object_set(settings_, "gtk-font-name", font_name, nullptr);
  }

  raw_ptr<GtkSettings> settings_ = nullptr;

 private:
  std::string original_font_name_;
};

}  // namespace

// Changing the GTK font (e.g. via the XSETTINGS Gtk/FontName setting) should
// update the default UI font and notify about the change.
TEST_F(GtkUiTest, DefaultFontFollowsGtkFontName) {
  SetGtkFontName("Sans 10");

  StubLinuxUiDelegate delegate;
  std::unique_ptr<ui::LinuxUiAndTheme> gtk_ui = BuildGtkUi();
  ASSERT_TRUE(gtk_ui->Initialize());

  int font_changed_count = 0;
  base::CallbackListSubscription subscription =
      gtk_ui->RegisterDefaultFontChangedCallback(
          base::BindLambdaForTesting([&] { ++font_changed_count; }));
  CountingNativeThemeObserver theme_observer;
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver> observation(
      &theme_observer);
  observation.Observe(gtk_ui->GetNativeTheme());

  const int small_size = gtk_ui->GetDefaultFontDescription().size_pixels;
  ASSERT_GT(small_size, 0);

  // Setting the same font must not notify.
  SetGtkFontName("Sans 10");
  EXPECT_EQ(font_changed_count, 0);
  EXPECT_EQ(theme_observer.count(), 0);
  EXPECT_EQ(gtk_ui->GetDefaultFontDescription().size_pixels, small_size);

  SetGtkFontName("Sans 20");
  EXPECT_EQ(font_changed_count, 1);
  EXPECT_GT(theme_observer.count(), 0);
  EXPECT_GT(gtk_ui->GetDefaultFontDescription().size_pixels, small_size);
}

// When the font scale isn't part of the device scale factor (as with the stub
// platform, or Wayland with kWaylandTextScaleTextOnly), changing the text
// scaling factor should scale the default UI font, not the display.
TEST_F(GtkUiTest, DefaultFontFollowsXftDpi) {
  SetGtkFontName("Sans 10");
  int original_dpi = -1;
  g_object_get(settings_, "gtk-xft-dpi", &original_dpi, nullptr);
  g_object_set(settings_, "gtk-xft-dpi", 96 * 1024, nullptr);
  absl::Cleanup restore_dpi = [&] {
    g_object_set(settings_, "gtk-xft-dpi", original_dpi, nullptr);
  };

  StubLinuxUiDelegate delegate;
  std::unique_ptr<ui::LinuxUiAndTheme> gtk_ui = BuildGtkUi();
  ASSERT_TRUE(gtk_ui->Initialize());

  int font_changed_count = 0;
  base::CallbackListSubscription subscription =
      gtk_ui->RegisterDefaultFontChangedCallback(
          base::BindLambdaForTesting([&] { ++font_changed_count; }));
  const int size = gtk_ui->GetDefaultFontDescription().size_pixels;
  ASSERT_GT(size, 0);

  g_object_set(settings_, "gtk-xft-dpi", 144 * 1024, nullptr);
  EXPECT_EQ(font_changed_count, 1);
  EXPECT_NEAR(gtk_ui->GetDefaultFontDescription().size_pixels, size * 1.5, 1);
  const ui::LinuxUi& linux_ui = *gtk_ui;
  EXPECT_EQ(linux_ui.display_config().font_scale, 1.0f);
}

}  // namespace gtk
