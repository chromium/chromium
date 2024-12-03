// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_CURSOR_MANAGER_WIN_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_CURSOR_MANAGER_WIN_H_

#include "base/win/registry.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"

namespace wm {
class NativeCursorManagerDelegate;
}

namespace views {

// A NativeCursorManager that performs the desktop-specific setting of cursor
// state. Similar to NativeCursorManagerAsh, it also communicates these changes
// to all root windows.
class VIEWS_EXPORT DesktopNativeCursorManagerWin
    : public DesktopNativeCursorManager {
 public:
  DesktopNativeCursorManagerWin();

  DesktopNativeCursorManagerWin(const DesktopNativeCursorManagerWin&) = delete;
  DesktopNativeCursorManagerWin& operator=(
      const DesktopNativeCursorManagerWin&) = delete;

  ~DesktopNativeCursorManagerWin() override;

  void InitCursorSizeObserver(
      wm::NativeCursorManagerDelegate* delegate) override;

 private:
  // Retrieve and report the cursor size to cursor manager.
  void SetSystemCursorSize(wm::NativeCursorManagerDelegate* delegate);
  void RegisterCursorRegkeyObserver(wm::NativeCursorManagerDelegate* delegate);

  base::win::RegKey hkcu_cursor_regkey_;
  gfx::Size system_cursor_size_;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_CURSOR_MANAGER_WIN_H_
