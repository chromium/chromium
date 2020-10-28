// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_DESKTOP_WINDOW_MOVE_CLIENT_H_
#define UI_BASE_X_X11_DESKTOP_WINDOW_MOVE_CLIENT_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "ui/base/x/x11_move_loop_delegate.h"
#include "ui/base/x/x11_whole_screen_move_loop.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

class XWindow;

// When we're dragging tabs, we need to manually position our window.
class COMPONENT_EXPORT(UI_BASE_X) X11DesktopWindowMoveClient
    : public X11MoveLoopDelegate {
 public:
  explicit X11DesktopWindowMoveClient(ui::XWindow* window);
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
  ui::XWindow* const window_;

  // Our cursor offset from the top left window origin when the drag
  // started. Used to calculate the window's new bounds relative to the current
  // location of the cursor.
  gfx::Vector2d window_offset_;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_DESKTOP_WINDOW_MOVE_CLIENT_H_
