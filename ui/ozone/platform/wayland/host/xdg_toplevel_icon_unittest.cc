// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/wayland_test.h"

namespace ui {

using XdgToplevelIconTest = WaylandTestSimple;

TEST_F(XdgToplevelIconTest, Basic) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(128, 128);
  gfx::ImageSkia icon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  window_->SetWindowIcons(gfx::ImageSkia(), icon);

  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* const manager = server->xdg_toplevel_icon_manager_v1();
    ASSERT_TRUE(manager);
    wl::XdgToplevelIconResources expected_resources = {
        wl::XdgToplevelIconResource(gfx::Size(128, 128), 1)};
    EXPECT_EQ(manager->resources(), expected_resources);
  });
}

TEST_F(XdgToplevelIconTest, AppIconTakesPrecedence) {
  SkBitmap app_bitmap;
  app_bitmap.allocN32Pixels(128, 128);
  gfx::ImageSkia app_icon = gfx::ImageSkia::CreateFrom1xBitmap(app_bitmap);

  SkBitmap window_bitmap;
  window_bitmap.allocN32Pixels(64, 64);
  gfx::ImageSkia window_icon =
      gfx::ImageSkia::CreateFrom1xBitmap(window_bitmap);

  window_->SetWindowIcons(window_icon, app_icon);

  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* const manager = server->xdg_toplevel_icon_manager_v1();
    ASSERT_TRUE(manager);
    wl::XdgToplevelIconResources expected_resources = {
        wl::XdgToplevelIconResource(gfx::Size(128, 128), 1)};
    EXPECT_EQ(manager->resources(), expected_resources);
  });
}

}  // namespace ui
