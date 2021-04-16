// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_DESKTOP_WINDOW_TREE_HOST_WIN_TEST_API_H_
#define UI_VIEWS_TEST_DESKTOP_WINDOW_TREE_HOST_WIN_TEST_API_H_

#include "base/macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class AXSystemCaretWin;
}

namespace views {

class DesktopWindowTreeHostWin;
class HWNDMessageHandler;

namespace test {

// A wrapper of DesktopWindowTreeHostWin to access private members for testing.
class DesktopWindowTreeHostWinTestApi {
 public:
  explicit DesktopWindowTreeHostWinTestApi(DesktopWindowTreeHostWin* host);

  void EnsureAXSystemCaretCreated();
  ui::AXSystemCaretWin* GetAXSystemCaret();
  gfx::NativeViewAccessible GetNativeViewAccessible();

  HWNDMessageHandler* GetHwndMessageHandler();

  void SetMockCursorPositionForTesting(const gfx::Point& position);

 private:
  DesktopWindowTreeHostWin* host_;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostWinTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_DESKTOP_WINDOW_TREE_HOST_WIN_TEST_API_H_
