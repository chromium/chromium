// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_surface.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

namespace ui {
namespace {

using ::testing::ElementsAre;

using WaylandSurfaceTest = WaylandTest;

TEST_P(WaylandSurfaceTest, SurfaceReenterOutput) {
  WaylandSurface* wayland_surface = window_->root_surface();
  EXPECT_TRUE(wayland_surface->entered_outputs().empty());

  // Client side WaylandOutput id.
  const uint32_t output_id =
      screen_->GetOutputIdForDisplayId(screen_->GetPrimaryDisplay().id());

  const uint32_t surface_id_ = window_->root_surface()->get_surface_id();

  PostToServerAndWait([surface_id_](wl::TestWaylandServerThread* server) {
    wl_surface_send_enter(
        server->GetObject<wl::MockSurface>(surface_id_)->resource(),
        server->output()->resource());
  });
  EXPECT_THAT(wayland_surface->entered_outputs(), ElementsAre(output_id));

  // Send enter again, but entered outputs should not have duplicate values.
  PostToServerAndWait([surface_id_](wl::TestWaylandServerThread* server) {
    wl_surface_send_enter(
        server->GetObject<wl::MockSurface>(surface_id_)->resource(),
        server->output()->resource());
  });
  EXPECT_THAT(wayland_surface->entered_outputs(), ElementsAre(output_id));
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandSurfaceTest,
                         ::testing::Values(wl::ServerConfig{}));
#else
INSTANTIATE_TEST_SUITE_P(
    XdgVersionStableTestWithAuraShell,
    WaylandSurfaceTest,
    ::testing::Values(
        wl::ServerConfig{
            .enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled},
        wl::ServerConfig{
            .enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled}));
#endif

}  // namespace
}  // namespace ui
