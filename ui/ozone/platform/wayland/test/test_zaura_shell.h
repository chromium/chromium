// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_SHELL_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_SHELL_H_

#include <aura-shell-server-protocol.h>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

class TestZAuraShell : public GlobalObject {
 public:
  TestZAuraShell();

  TestZAuraShell(const TestZAuraShell&) = delete;
  TestZAuraShell& operator=(const TestZAuraShell&) = delete;

  ~TestZAuraShell() override;

  // Sets bug fixes and sends them out if the object is bound.
  void SetBugFixes(std::vector<uint32_t> bug_fixes);

 private:
  void OnBind() override;

  void MaybeSendBugFixes();

  // Bug fixes that shall be sent to the client.
  std::vector<uint32_t> bug_fixes_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_SHELL_H_
