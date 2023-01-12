// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_DESKTOP_WINDOW_MOVE_CLIENT_H_
#define UI_BASE_X_X11_DESKTOP_WINDOW_MOVE_CLIENT_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/x/x11_move_loop_delegate.h"
#include "ui/base/x/x11_whole_screen_move_loop.h"
#include "ui/gfx/geometry/point.h"

namespace gfx {
class Rect;
}

namespace ui {

// When we're dragging tabs, we need to manually position our window.
class COMPONENT_EXPORT(UI_BASE_X) X11DesktopWindowMoveClient
    : public X11MoveLoopDelegate {
 public:
  // Connection point that the window being moved needs to implement.
  class Delegate {
   public:
    // Sets new window bounds.
    virtual void SetBoundsOnMove(const gfx::Rect& requested_bounds) = 0;
    // Returns the cursor that was used at the time the move started.
    virtual scoped_refptr<X11Cursor> GetLastCursor() = 0;
    // Returns the size part of the window bounds.
    virtual gfx::Size GetSize() = 0;

   protected:
    virtual ~Delegate();
  };

  explicit X11DesktopWindowMoveClient(Delegate* window);
  ~X11DesktopWindowMoveClient() override;

  // Overridden from X11WholeScreenMoveLoopDelegate:
  void OnMouseMovement(const gfx::Point& screen_point,
                       int flags,
                       base::TimeTicks event_time) override;
  void OnMouseReleased() override;
  void OnMoveLoopEnded() override;

  bool RunMoveLoop(bool can_grab_pointer, const gfx::Vector2d& drag_offset);
  void EndMoveLoop();

 private:
  X11WholeScreenMoveLoop move_loop_{this};

  // We need to keep track of this so we can actually move it when reacting to
  // mouse events.
  const raw_ptr<Delegate> window_;

  // Our cursor offset from the top left window origin when the drag
  // started. Used to calculate the window's new bounds relative to the current
  // location of the cursor.
  gfx::Vector2d window_offset_;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_DESKTOP_WINDOW_MOVE_CLIENT_H_
