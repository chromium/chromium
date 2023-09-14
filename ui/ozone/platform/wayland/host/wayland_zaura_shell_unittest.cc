// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"

#include <vector>

#include "base/test/test_future.h"
#include "base/version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zaura_shell.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

namespace ui {

using WaylandZAuraShellTest = WaylandTestSimpleWithAuraShell;

TEST_F(WaylandZAuraShellTest, BugFix) {
  connection_->zaura_shell()->ResetBugFixesStatusForTesting();

  ASSERT_FALSE(connection_->zaura_shell()->HasBugFix(1));
  ASSERT_FALSE(connection_->zaura_shell()->HasBugFix(3));
  ASSERT_FALSE(connection_->zaura_shell()->HasBugFix(2));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->zaura_shell()->SetBugFixes({1, 3});
  });

  ASSERT_TRUE(connection_->zaura_shell()->HasBugFix(1));
  ASSERT_TRUE(connection_->zaura_shell()->HasBugFix(3));
  ASSERT_FALSE(connection_->zaura_shell()->HasBugFix(2));
}

TEST_F(WaylandZAuraShellTest, CompositorVersion) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->zaura_shell()->SetCompositorVersion("INVALID.VERSION");
  });
  ASSERT_FALSE(connection_->zaura_shell()->compositor_version().IsValid());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->zaura_shell()->SetCompositorVersion("1.2.3.4");
  });

  const base::Version& received_version =
      connection_->zaura_shell()->compositor_version();
  ASSERT_TRUE(received_version.IsValid());
  ASSERT_EQ(received_version, base::Version("1.2.3.4"));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->zaura_shell()->SetCompositorVersion("1NV4L1D.2");
  });
  ASSERT_FALSE(connection_->zaura_shell()->compositor_version().IsValid());
}

TEST_F(WaylandZAuraShellTest, AllBugFixesSent) {
  connection_->zaura_shell()->ResetBugFixesStatusForTesting();

  ASSERT_FALSE(connection_->zaura_shell()->HasBugFix(1));
  ASSERT_FALSE(connection_->zaura_shell()->HasBugFix(2));
  ASSERT_FALSE(connection_->zaura_shell()->HasBugFix(3));

  base::test::TestFuture<const std::vector<uint32_t>&> future;
  connection_->buffer_manager_host()->WaitForAllBugFixIds(future.GetCallback());

  const std::vector<uint32_t>& kBugFixIds = {1, 3};
  PostToServerAndWait([&kBugFixIds](wl::TestWaylandServerThread* server) {
    server->zaura_shell()->SetBugFixes(kBugFixIds);
  });
  // Bug fix ids are not ready yet since all_bug_fixes_sent event is not yet
  // received.
  EXPECT_EQ(connection_->zaura_shell()->MaybeGetBugFixIds(), absl::nullopt);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->zaura_shell()->SendAllBugFixesSent();
  });
  // Make sure zaura_shell bug fix ids cache is filled by kBugFixIds.
  auto ids = connection_->zaura_shell()->MaybeGetBugFixIds();
  ASSERT_TRUE(ids.has_value());
  EXPECT_EQ(*ids, kBugFixIds);
  // Make sure kBugFixIds are not passed to BufferManagerHost.
  EXPECT_EQ(future.Get<std::vector<uint32_t>>(), kBugFixIds);
}

}  // namespace ui
