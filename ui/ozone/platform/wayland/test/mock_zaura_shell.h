// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZAURA_SHELL_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZAURA_SHELL_H_

#include <aura-shell-server-protocol.h>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

class MockZAuraShell : public GlobalObject {
 public:
  MockZAuraShell();

  MockZAuraShell(const MockZAuraShell&) = delete;
  MockZAuraShell& operator=(const MockZAuraShell&) = delete;

  ~MockZAuraShell() override;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZAURA_SHELL_H_
