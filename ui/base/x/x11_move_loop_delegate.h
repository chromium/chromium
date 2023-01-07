// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_MOVE_LOOP_DELEGATE_H_
#define UI_BASE_X_X11_MOVE_LOOP_DELEGATE_H_

namespace base {
class TimeTicks;
}

namespace gfx {
class Point;
}

namespace ui {

// Receives mouse events while the X11MoveLoop is tracking a drag.
class X11MoveLoopDelegate {
 public:
  virtual ~X11MoveLoopDelegate() = default;

  // Called when we receive a mouse move event.
  virtual void OnMouseMovement(const gfx::Point& screen_point,
                               int flags,
                               base::TimeTicks event_time) = 0;

  // Called when the mouse button is released.
  virtual void OnMouseReleased() = 0;

  // Called when the user has released the mouse button. The move loop will
  // release the grab after this has been called.
  virtual void OnMoveLoopEnded() = 0;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_MOVE_LOOP_DELEGATE_H_
