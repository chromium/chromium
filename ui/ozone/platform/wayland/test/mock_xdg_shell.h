// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_SHELL_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_SHELL_H_

#include <xdg-shell-server-protocol.h>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

extern const struct xdg_wm_base_interface kMockXdgShellImpl;
extern const struct zxdg_shell_v6_interface kMockZxdgShellV6Impl;

// Manage xdg_shell object.
class MockXdgShell : public GlobalObject {
 public:
  MockXdgShell();

  MockXdgShell(const MockXdgShell&) = delete;
  MockXdgShell& operator=(const MockXdgShell&) = delete;

  ~MockXdgShell() override;

  MOCK_METHOD1(Pong, void(uint32_t serial));
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_SHELL_H_
