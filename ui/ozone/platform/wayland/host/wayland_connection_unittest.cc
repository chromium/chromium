// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/common/wayland.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_shell.h"
#include "ui/ozone/platform/wayland/test/test_compositor.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::Values;

namespace ui {

using WaylandConnectionTest = WaylandTest;

TEST_P(WaylandConnectionTest, Ping) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    constexpr uint32_t kSerial = 1234;
    xdg_wm_base_send_ping(server->xdg_shell()->resource(), kSerial);
    EXPECT_CALL(*server->xdg_shell(), Pong(1234));
  });
}

TEST_P(WaylandConnectionTest, CompositorVersionTest) {
  wl::ServerConfig config = GetParam();
  uint32_t expected_compositor_version = 0;
  switch (config.compositor_version) {
    case wl::CompositorVersion::kV3:
      expected_compositor_version = 3;
      break;
    case wl::CompositorVersion::kV4:
      expected_compositor_version = 4;
      break;
  }

  EXPECT_EQ(expected_compositor_version,
            wl::get_version_of_object(connection_->compositor()));
}

INSTANTIATE_TEST_SUITE_P(
    XdgVersionStableTest,
    WaylandConnectionTest,
    Values(wl::ServerConfig{.compositor_version = wl::CompositorVersion::kV3}));
INSTANTIATE_TEST_SUITE_P(
    XdgVersionStableTestCompositorV4,
    WaylandConnectionTest,
    Values(wl::ServerConfig{.compositor_version = wl::CompositorVersion::kV4}));

}  // namespace ui
