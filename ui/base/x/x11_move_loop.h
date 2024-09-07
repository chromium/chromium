// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_MOVE_LOOP_H_
#define UI_BASE_X_X11_MOVE_LOOP_H_

#include "base/functional/callback_forward.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class X11Cursor;

// Runs a nested run loop and grabs the mouse. This is used to implement
// dragging.
class X11MoveLoop {
 public:
  virtual ~X11MoveLoop() = default;

  // Runs the nested run loop. While the mouse is grabbed, use |cursor| as
  // the mouse cursor. Returns true if the move-loop is completed successfully.
  // If the pointer-grab fails, or the move-loop is canceled by the user (e.g.
  // by pressing escape), then returns false.
  virtual bool RunMoveLoop(bool can_grab_pointer,
                           scoped_refptr<ui::X11Cursor> old_cursor,
                           scoped_refptr<ui::X11Cursor> new_cursor,
                           base::OnceClosure started_callback) = 0;

  // Updates the cursor while the move loop is running.
  virtual void UpdateCursor(scoped_refptr<ui::X11Cursor> cursor) = 0;

  // Ends the move loop that's currently in progress.
  virtual void EndMoveLoop() = 0;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_MOVE_LOOP_H_
