// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_NATIVE_CURSOR_MANAGER_DELEGATE_H_
#define UI_WM_CORE_NATIVE_CURSOR_MANAGER_DELEGATE_H_

#include "base/component_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Size;
}

namespace ui {
enum class CursorSize;
}

namespace wm {

// The non-public interface that CursorManager exposes to its users. This
// gives accessors to all the current state, and mutators to all the current
// state.
class COMPONENT_EXPORT(UI_WM) NativeCursorManagerDelegate {
 public:
  virtual ~NativeCursorManagerDelegate() {}

  // TODO(tdanderson): Possibly remove this interface.
  virtual gfx::NativeCursor GetCursor() const = 0;
  virtual bool IsCursorVisible() const = 0;

  virtual void CommitCursor(gfx::NativeCursor cursor) = 0;
  virtual void CommitVisibility(bool visible) = 0;
  virtual void CommitCursorSize(ui::CursorSize cursor_size) = 0;
  virtual void CommitMouseEventsEnabled(bool enabled) = 0;
  virtual void CommitSystemCursorSize(const gfx::Size& cursor_size) = 0;
};

}  // namespace wm

#endif  // UI_WM_CORE_NATIVE_CURSOR_MANAGER_DELEGATE_H_
