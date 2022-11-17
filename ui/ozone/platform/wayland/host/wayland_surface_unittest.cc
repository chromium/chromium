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

class WaylandSurfaceTest : public WaylandTest {
 public:
  WaylandSurfaceTest() : WaylandTest(WaylandTest::TestServerMode::kAsync) {}
  WaylandSurfaceTest(const WaylandSurfaceTest&) = delete;
  WaylandSurfaceTest& operator=(const WaylandSurfaceTest&) = delete;
  ~WaylandSurfaceTest() override = default;
};

TEST_P(WaylandSurfaceTest, SurfaceReenterOutput) {
  WaylandSurface* wayland_surface = window_->root_surface();
  EXPECT_TRUE(wayland_surface->entered_outputs().empty());

  // Client side WaylandOutput id.
  const uint32_t output_id =
      screen_->GetOutputIdForDisplayId(screen_->GetPrimaryDisplay().id());

  // Shared wl_resource ids.
  const uint32_t wl_surface_id = wl_resource_get_id(surface_->resource());
  const uint32_t wl_output_id =
      wl_resource_get_id(server_.output()->resource());

  PostToServerAndWait(
      [wl_surface_id, wl_output_id](wl::TestWaylandServerThread* server) {
        wl_surface_send_enter(
            server->GetObject<wl::MockSurface>(wl_surface_id)->resource(),
            server->GetObject<wl::TestOutput>(wl_output_id)->resource());
      });
  EXPECT_THAT(wayland_surface->entered_outputs(), ElementsAre(output_id));

  // Send enter again, but entered outputs should not have duplicate values.
  PostToServerAndWait(
      [wl_surface_id, wl_output_id](wl::TestWaylandServerThread* server) {
        wl_surface_send_enter(
            server->GetObject<wl::MockSurface>(wl_surface_id)->resource(),
            server->GetObject<wl::TestOutput>(wl_output_id)->resource());
      });
  EXPECT_THAT(wayland_surface->entered_outputs(), ElementsAre(output_id));
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandSurfaceTest,
                         ::testing::Values(wl::ServerConfig{}));

}  // namespace
}  // namespace ui
