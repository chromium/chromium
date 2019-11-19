// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WINDOWS_WINDOWS_WINDOW_H_
#define UI_OZONE_PLATFORM_WINDOWS_WINDOWS_WINDOW_H_

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/win/win_window.h"

namespace ui {

class WindowsWindow : public WinWindow {
 public:
  WindowsWindow(PlatformWindowDelegate* delegate, const gfx::Rect& bounds);
  ~WindowsWindow() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowsWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WINDOWS_WINDOWS_WINDOW_H_
