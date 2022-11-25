// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <aura-shell-server-protocol.h>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"
#include "ui/ozone/platform/wayland/test/mock_zaura_shell.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"

namespace ui {

TEST(WaylandZAuraShellTest, BugFix) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
  wl::TestWaylandServerThread server;
  ASSERT_TRUE(server.Start(
      {.enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled}));
  wl::MockZAuraShell* zaura_shell = server.zaura_shell();

  WaylandConnection connection;
  ASSERT_TRUE(connection.Initialize());
  connection.event_source()->StartProcessingEvents();

  base::RunLoop().RunUntilIdle();
  server.Pause();

  ASSERT_TRUE(server.zaura_shell()->resource());
  zaura_shell_send_bug_fix(zaura_shell->resource(), 1);
  zaura_shell_send_bug_fix(zaura_shell->resource(), 3);

  server.Resume();
  base::RunLoop().RunUntilIdle();
  server.Pause();

  ASSERT_TRUE(connection.zaura_shell()->HasBugFix(1));
  ASSERT_TRUE(connection.zaura_shell()->HasBugFix(3));
  ASSERT_FALSE(connection.zaura_shell()->HasBugFix(2));
}

}  // namespace ui
