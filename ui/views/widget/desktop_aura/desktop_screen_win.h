// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_WIN_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_WIN_H_

#include "base/memory/raw_ptr.h"
#include "ui/display/win/screen_win.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT DesktopScreenWin : public display::win::ScreenWin {
 public:
  DesktopScreenWin();
  DesktopScreenWin(const DesktopScreenWin&) = delete;
  DesktopScreenWin& operator=(const DesktopScreenWin&) = delete;
  ~DesktopScreenWin() override;

 private:
  // display::win::ScreenWin:
  HWND GetHWNDFromNativeWindow(gfx::NativeWindow window) const override;
  gfx::NativeWindow GetNativeWindowFromHWND(HWND hwnd) const override;
  bool IsNativeWindowOccluded(gfx::NativeWindow window) const override;
  std::optional<bool> IsWindowOnCurrentVirtualDesktop(
      gfx::NativeWindow window) const override;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_WIN_H_
