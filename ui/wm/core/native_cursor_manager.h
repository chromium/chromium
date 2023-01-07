// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_NATIVE_CURSOR_MANAGER_H_
#define UI_WM_CORE_NATIVE_CURSOR_MANAGER_H_

#include "base/component_export.h"
#include "ui/wm/core/native_cursor_manager_delegate.h"

namespace display {
class Display;
}

namespace ui {
enum class CursorSize;
}

namespace wm {

// Interface where platforms such as Ash or Desktop aura are notified of
// requested changes to cursor state. When requested, implementer should tell
// the CursorManager of any actual state changes performed through the
// delegate.
class COMPONENT_EXPORT(UI_WM) NativeCursorManager {
 public:
  virtual ~NativeCursorManager() {}

  // A request to set the screen DPI. Can cause changes in the current cursor.
  virtual void SetDisplay(const display::Display& display,
                          NativeCursorManagerDelegate* delegate) = 0;

  // A request to set the cursor to |cursor|. At minimum, implementer should
  // call NativeCursorManagerDelegate::CommitCursor() with whatever cursor is
  // actually used.
  virtual void SetCursor(
      gfx::NativeCursor cursor,
      NativeCursorManagerDelegate* delegate) = 0;

  // A request to set the visibility of the cursor. At minimum, implementer
  // should call NativeCursorManagerDelegate::CommitVisibility() with whatever
  // the visibility is.
  virtual void SetVisibility(
    bool visible,
    NativeCursorManagerDelegate* delegate) = 0;

  // A request to set the cursor set.
  virtual void SetCursorSize(ui::CursorSize cursor_size,
                             NativeCursorManagerDelegate* delegate) = 0;

  // A request to set whether mouse events are disabled. At minimum,
  // implementer should call NativeCursorManagerDelegate::
  // CommitMouseEventsEnabled() with whether mouse events are actually enabled.
  virtual void SetMouseEventsEnabled(
      bool enabled,
      NativeCursorManagerDelegate* delegate) = 0;
};

}  // namespace wm

#endif  // UI_WM_CORE_NATIVE_CURSOR_MANAGER_H_
