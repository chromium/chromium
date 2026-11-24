// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/x/gtk_ui_platform_x11.h"

#include <dlfcn.h>
#include <glib-object.h>

#include "base/environment.h"
#include "base/process/launch.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gtk/gtk_compat.h"

namespace gtk {

namespace {

x11::VisualId FindArgbVisual(x11::Connection* connection) {
  for (const auto& depth : connection->default_screen().allowed_depths) {
    if (depth.depth != 32) {
      continue;
    }
    for (const auto& visual : depth.visuals) {
      if (visual.red_mask == 0xff0000 && visual.green_mask == 0x00ff00 &&
          visual.blue_mask == 0x0000ff) {
        return visual.visual_id;
      }
    }
  }
  return x11::VisualId{};
}

}  // namespace

MULTIPROCESS_TEST_MAIN(GetGdkWindowWithSkipArgbVisualsChild) {
  // GtkInitCheck() has already run in main(); verify XLIB_SKIP_ARGB_VISUALS is
  // preserved in the environment for child processes.
  auto env = base::Environment::Create();
  if (env->GetVar("XLIB_SKIP_ARGB_VISUALS") != "1") {
    return 1;
  }

  auto* connection = x11::Connection::Get();
  x11::VisualId argb_visual = FindArgbVisual(connection);
  if (argb_visual == x11::VisualId{}) {
    return 2;
  }

  x11::ColorMap colormap = connection->GenerateId<x11::ColorMap>();
  connection->CreateColormap({x11::ColormapAlloc::None, colormap,
                              connection->default_root(), argb_visual});
  x11::Window window = connection->GenerateId<x11::Window>();
  connection->CreateWindow({
      .depth = 32,
      .wid = window,
      .parent = connection->default_root(),
      .width = 10,
      .height = 10,
      .c_class = x11::WindowClass::InputOutput,
      .visual = argb_visual,
      .border_pixel = 0,
      .colormap = colormap,
  });
  connection->Sync();

  absl::Cleanup cleanup = [&] {
    connection->DestroyWindow({window});
    connection->FreeColormap({colormap});
    connection->Sync();
  };

  GtkUiPlatformX11 platform;
  GdkWindow* gdk_window =
      platform.GetGdkWindow(static_cast<gfx::AcceleratedWidget>(window));
  if (!gdk_window) {
    return 3;
  }
  auto get_xid = reinterpret_cast<unsigned long (*)(GdkWindow*)>(
      dlsym(RTLD_DEFAULT, "gdk_x11_window_get_xid"));
  if (!get_xid) {
    g_object_unref(gdk_window);
    return 4;
  }
  const unsigned long xid = get_xid(gdk_window);
  g_object_unref(gdk_window);
  return xid == static_cast<unsigned long>(window) ? 0 : 5;
}

using GtkUiPlatformX11Test = base::MultiProcessTest;

// Regression test for https://crbug.com/40850888 and https://crbug.com/1272182:
// when XLIB_SKIP_ARGB_VISUALS is set at startup, GetGdkWindow() must still
// return a valid foreign GdkWindow for 32-bit ARGB X11 windows without crashing
// in XVisualIDFromVisual().
TEST_F(GtkUiPlatformX11Test, GetGdkWindowWithSkipArgbVisuals) {
  if (GtkCheckVersion(4)) {
    GTEST_SKIP() << "GetGdkWindow() is only used with GTK3.";
  }

  base::LaunchOptions options;
  options.environment["XLIB_SKIP_ARGB_VISUALS"] = "1";
  base::Process child =
      SpawnChildWithOptions("GetGdkWindowWithSkipArgbVisualsChild", options);
  ASSERT_TRUE(child.IsValid());

  int exit_code = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(exit_code, 0);
}

}  // namespace gtk
