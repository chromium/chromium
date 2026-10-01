// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_util.h"

#include <glib.h>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gtk/gtk_compat.h"

namespace gtk {

class GtkUtilXftDpiTest : public testing::Test {
 protected:
  void SetUp() override {
    settings_ = GetDefaultGtkSettings();
    ASSERT_TRUE(settings_);
    g_object_get(settings_, "gtk-xft-dpi", &original_dpi_, nullptr);
  }
  void TearDown() override {
    g_object_set(settings_, "gtk-xft-dpi", original_dpi_, nullptr);
  }

  void SetXftDpi(int dpi) {
    g_object_set(settings_, "gtk-xft-dpi", dpi, nullptr);
  }

 private:
  raw_ptr<GtkSettings> settings_ = nullptr;
  int original_dpi_ = -1;
};

TEST_F(GtkUtilXftDpiTest, GetFontScaleFromXftDpi) {
  SetXftDpi(144 * 1024);
  EXPECT_EQ(GetXftDpi(), 144 * 1024);
  EXPECT_EQ(GetFontScale(), 1.5);
}

// Regression test for crbug.com/566241627: `gtk-xft-dpi` is unset on GTK4
// Wayland when the xdg-desktop-portal Settings interface is unavailable.
// GetFontScale() must not call GTK3-only GdkScreen APIs on GTK4, which are
// unresolved there.
TEST_F(GtkUtilXftDpiTest, GetFontScaleWithoutXftDpi) {
  SetXftDpi(-1);
  EXPECT_EQ(GetXftDpi(), 0);
  const double font_scale = GetFontScale();
  if (GtkCheckVersion(4)) {
    EXPECT_EQ(font_scale, 1.0);
  } else {
    EXPECT_GT(font_scale, 0.0);
  }
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

}  // namespace gtk
