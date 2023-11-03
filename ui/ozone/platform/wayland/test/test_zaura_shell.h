// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_SHELL_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_SHELL_H_

#include <aura-shell-server-protocol.h>

#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

class TestZAuraShell : public GlobalObject {
 public:
  TestZAuraShell();

  TestZAuraShell(const TestZAuraShell&) = delete;
  TestZAuraShell& operator=(const TestZAuraShell&) = delete;

  ~TestZAuraShell() override;

  void SetCompositorVersion(const std::string& version);

  // Sets bug fixes and sends them out if the object is bound.
  void SetBugFixes(std::vector<uint32_t> bug_fixes);

  // Sends `zaura_shell_send_all_bug_fixes_sent` event with the `size`. This
  // must be called after SetBugFixes call.
  void SendAllBugFixesSent();

 private:
  void OnBind() override;

  void MaybeSendCompositorVersion();
  void SendBugFixes();

  // Compostitor string version. For testing purposes, it is help in a string,
  // so that it can store either valid or invalid values.
  std::string compositor_version_string_ = "1.2.3.4";

  // Bug fixes that shall be sent to the client.
  std::vector<uint32_t> bug_fixes_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_SHELL_H_
