// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zaura_shell.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using testing::Values;

namespace ui {

using WaylandZAuraShellTest = WaylandTestSimpleWithAuraShell;

TEST_F(WaylandZAuraShellTest, BugFix) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const zaura_shell = server->zaura_shell()->resource();
    zaura_shell_send_bug_fix(zaura_shell, 1);
    zaura_shell_send_bug_fix(zaura_shell, 3);
  });

  ASSERT_TRUE(connection_->zaura_shell()->HasBugFix(1));
  ASSERT_TRUE(connection_->zaura_shell()->HasBugFix(3));
  ASSERT_FALSE(connection_->zaura_shell()->HasBugFix(2));
}

}  // namespace ui
