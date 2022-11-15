// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/test/mock_zaura_shell.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using testing::Values;

namespace ui {

// TODO(crbug.com/1365887): change this to
// `using WaylandZAuraShellTest = WaylandTest`
// once the default mode becomes asynchronous.
class WaylandZAuraShellTest : public WaylandTest {
 public:
  WaylandZAuraShellTest() : WaylandTest(TestServerMode::kAsync) {}
  WaylandZAuraShellTest(const WaylandZAuraShellTest&) = delete;
  WaylandZAuraShellTest& operator=(const WaylandZAuraShellTest&) = delete;
  ~WaylandZAuraShellTest() override = default;
};

TEST_P(WaylandZAuraShellTest, BugFix) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const zaura_shell = server->zaura_shell()->resource();
    zaura_shell_send_bug_fix(zaura_shell, 1);
    zaura_shell_send_bug_fix(zaura_shell, 3);
  });

  ASSERT_TRUE(connection_->zaura_shell()->HasBugFix(1));
  ASSERT_TRUE(connection_->zaura_shell()->HasBugFix(3));
  ASSERT_FALSE(connection_->zaura_shell()->HasBugFix(2));
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandZAuraShellTest,
                         Values(wl::ServerConfig{}));

}  // namespace ui
