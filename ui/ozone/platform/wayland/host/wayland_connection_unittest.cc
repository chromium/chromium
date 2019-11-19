// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server-core.h>
#include <xdg-shell-unstable-v5-server-protocol.h>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"

namespace ui {

namespace {
constexpr uint32_t kXdgVersion5 = 5;
}

TEST(WaylandConnectionTest, UseUnstableVersion) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
  wl::TestWaylandServerThread server;
  EXPECT_CALL(*server.xdg_shell(),
              UseUnstableVersion(XDG_SHELL_VERSION_CURRENT));
  ASSERT_TRUE(server.Start(kXdgVersion5));
  WaylandConnection connection;
  ASSERT_TRUE(connection.Initialize());
  connection.StartProcessingEvents();

  base::RunLoop().RunUntilIdle();
  server.Pause();
}

TEST(WaylandConnectionTest, Ping) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
  wl::TestWaylandServerThread server;
  ASSERT_TRUE(server.Start(kXdgVersion5));
  WaylandConnection connection;
  ASSERT_TRUE(connection.Initialize());
  connection.StartProcessingEvents();

  base::RunLoop().RunUntilIdle();
  server.Pause();

  xdg_shell_send_ping(server.xdg_shell()->resource(), 1234);
  EXPECT_CALL(*server.xdg_shell(), Pong(1234));

  server.Resume();
  base::RunLoop().RunUntilIdle();
  server.Pause();
}

}  // namespace ui
