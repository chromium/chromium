// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/linux_ui_delegate_wayland.h"

#include <string>
#include <utility>

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_foreign_v2.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

namespace ui {
namespace {

class LinuxUiDelegateWaylandTest : public WaylandTestSimple {};

TEST_F(LinuxUiDelegateWaylandTest, ExportsVisibleToplevel) {
  LinuxUiDelegateWayland delegate(connection_.get());

  std::string handle;
  delegate.ExportWindowHandle(
      widget_, base::BindLambdaForTesting(
                   [&](std::string value) { handle = std::move(value); }));

  SyncDisplay();

  EXPECT_EQ("wayland:" + server_.xdg_exporter_v2()->handle(), handle);
}

TEST_F(LinuxUiDelegateWaylandTest, HiddenToplevelYieldsEmptyHandle) {
  LinuxUiDelegateWayland delegate(connection_.get());

  // Hiding destroys the xdg_toplevel role, like a never-shown window.
  window_->Hide();
  SyncDisplay();

  bool callback_ran = false;
  std::string handle = "not-empty";
  delegate.ExportWindowHandle(
      widget_, base::BindLambdaForTesting([&](std::string value) {
        callback_ran = true;
        handle = std::move(value);
      }));

  SyncDisplay();

  // Callers and the XDG Desktop Portal treat an empty string as "no parent".
  EXPECT_TRUE(callback_ran);
  EXPECT_TRUE(handle.empty());
}

}  // namespace
}  // namespace ui
