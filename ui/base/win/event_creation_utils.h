// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_EVENT_CREATION_UTILS_H_
#define UI_BASE_WIN_EVENT_CREATION_UTILS_H_

#include "base/component_export.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace ui {

// Values passed as `dwExtraInfo` on synthetic mouse events so hooks can
// identify them.
enum class MouseEventExtraInfo {
  kNone = 0,
  // Marks synthetic mouse moves injected by `QueueDragUnblockNudge()` so that
  // `InputDispatcher` ignores them when waiting for test-initiated moves.
  kDragUnblockNudge = 0x4E554447,  // 'NUDG'
};

// Send a mouse event to Windows input queue using ::SendInput, to screen
// point |point|. Returns true if the mouse event was sent, false if not.
// The coordinates will be translated to absolute screen coordinates and the
// MOUSEEVENTF_ABSOLUTE flag will be set on the events. `extra_info` is passed
// as the event's dwExtraInfo, which hooks can read to identify the event.
COMPONENT_EXPORT(UI_BASE)
bool SendMouseEvent(
    const gfx::Point& point,
    int flags,
    MouseEventExtraInfo extra_info = MouseEventExtraInfo::kNone);

}  // namespace ui

#endif  // UI_BASE_WIN_EVENT_CREATION_UTILS_H_
