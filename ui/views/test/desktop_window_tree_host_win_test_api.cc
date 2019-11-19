// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/desktop_window_tree_host_win_test_api.h"

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"
#include "ui/views/win/hwnd_message_handler.h"

namespace views {
namespace test {

DesktopWindowTreeHostWinTestApi::DesktopWindowTreeHostWinTestApi(
    DesktopWindowTreeHostWin* host)
    : host_(host) {}

void DesktopWindowTreeHostWinTestApi::EnsureAXSystemCaretCreated() {
  host_->message_handler_->OnCaretBoundsChanged(nullptr);
}

ui::AXSystemCaretWin* DesktopWindowTreeHostWinTestApi::GetAXSystemCaret() {
  return host_->message_handler_->ax_system_caret_.get();
}

gfx::NativeViewAccessible
DesktopWindowTreeHostWinTestApi::GetNativeViewAccessible() {
  return host_->GetNativeViewAccessible();
}

HWNDMessageHandler* DesktopWindowTreeHostWinTestApi::GetHwndMessageHandler() {
  return host_->message_handler_.get();
}

void DesktopWindowTreeHostWinTestApi::SetMockCursorPositionForTesting(
    const gfx::Point& position) {
  GetHwndMessageHandler()->mock_cursor_position_ = position;
}

}  // namespace test
}  // namespace views
