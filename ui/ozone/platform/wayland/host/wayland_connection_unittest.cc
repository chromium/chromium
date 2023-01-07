// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server-core.h>
#include <xdg-shell-server-protocol.h>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/common/wayland.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/test/test_compositor.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"

namespace ui {

TEST(WaylandConnectionTest, Ping) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
  wl::TestWaylandServerThread server;
  constexpr uint32_t expected_compositor_version = 4;
  ASSERT_TRUE(server.Start({.shell_version = wl::ShellVersion::kStable,
                            .compositor_version = wl::CompositorVersion::kV4}));
  WaylandConnection connection;
  ASSERT_TRUE(connection.Initialize());
  connection.event_source()->StartProcessingEvents();

  base::RunLoop().RunUntilIdle();
  server.Pause();

  EXPECT_EQ(expected_compositor_version,
            wl::get_version_of_object(connection.compositor()));

  xdg_wm_base_send_ping(server.xdg_shell()->resource(), 1234);
  EXPECT_CALL(*server.xdg_shell(), Pong(1234));

  server.Resume();
  base::RunLoop().RunUntilIdle();
  server.Pause();
}

}  // namespace ui
