// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_foreign_wrapper.h"

#include <string>

#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_foreign_v2.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::_;

namespace ui {
namespace {

class XdgForeignWrapperTest : public WaylandTestSimple {};

TEST_F(XdgForeignWrapperTest, ExportsVisibleToplevel) {
  ASSERT_TRUE(connection_->xdg_foreign());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->xdg_exporter_v2(), ExportToplevel(_)).Times(1);
  });

  std::string handle;
  connection_->xdg_foreign()->ExportSurfaceToForeign(
      window_->AsWaylandToplevelWindow(),
      base::BindLambdaForTesting(
          [&](const std::string& value) { handle = value; }));

  SyncDisplay();

  EXPECT_EQ(server_.xdg_exporter_v2()->handle(), handle);
}

TEST_F(XdgForeignWrapperTest, DoesNotExportToplevelWithoutRole) {
  ASSERT_TRUE(connection_->xdg_foreign());

  // Hiding destroys the xdg_toplevel role, like a never-shown window.
  window_->Hide();
  SyncDisplay();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->xdg_exporter_v2(), ExportToplevel(_)).Times(0);
  });

  bool callback_ran = false;
  std::string handle = "not-empty";
  connection_->xdg_foreign()->ExportSurfaceToForeign(
      window_->AsWaylandToplevelWindow(),
      base::BindLambdaForTesting([&](const std::string& value) {
        callback_ran = true;
        handle = value;
      }));

  SyncDisplay();

  EXPECT_TRUE(callback_ran);
  EXPECT_TRUE(handle.empty());
}

}  // namespace
}  // namespace ui
