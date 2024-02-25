// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"

#include "base/version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zaura_shell.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

namespace ui {

using WaylandZAuraShellTest = WaylandTestSimpleWithAuraShell;

TEST_F(WaylandZAuraShellTest, CompositorVersion) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->zaura_shell()->SetCompositorVersion("INVALID.VERSION");
  });
  ASSERT_FALSE(connection_->GetServerVersion().IsValid());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->zaura_shell()->SetCompositorVersion("1.2.3.4");
  });

  base::Version received_version = connection_->GetServerVersion();
  ASSERT_TRUE(received_version.IsValid());
  ASSERT_EQ(received_version, base::Version("1.2.3.4"));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->zaura_shell()->SetCompositorVersion("1NV4L1D.2");
  });
  ASSERT_FALSE(connection_->GetServerVersion().IsValid());
}

}  // namespace ui
