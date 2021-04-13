// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_WM_WM_DRAG_HANDLER_H_
#define UI_PLATFORM_WINDOW_WM_WM_DRAG_HANDLER_H_

#include "base/component_export.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
}

namespace ui {
class PlatformWindow;
class OSExchangeData;

class COMPONENT_EXPORT(WM) WmDragHandler {
 public:
  // During the drag operation, the handler may send updates
  class Delegate {
   public:
    // Called every time when the drag location has changed.
    virtual void OnDragLocationChanged(const gfx::Point& screen_point_px) = 0;
    // Called when the currently negotiated operation has changed.
    virtual void OnDragOperationChanged(
        DragDropTypes::DragOperation operation) = 0;
    // Called once when the operation has finished.
    virtual void OnDragFinished(int operation) = 0;

   protected:
    virtual ~Delegate();
  };

  // Starts dragging |data|.
  // |operation| is bitmask of DRAG_NONE, DRAG_MOVE, DRAG_COPY and DRAG_LINK
  // in DragDropTypes::DragOperation that defines operations possible for the
  // drag source.  The destination sets the resulting operation when the drop
  // action is performed.
  // |can_grab_pointer| indicates whether the implementation can grab the mouse
  // pointer (some platforms may need this).
  // In progress updates on the drag operation come back through the |delegate|.
  //
  // This method runs a nested message loop, returning when the drag operation
  // is done. Care must be taken when calling this as it's entirely possible
  // that when this returns this object (and the calling object) have been
  // destroyed.
  //
  // Returns whether the operation ended well (i.e., had not been canceled).
  virtual bool StartDrag(const OSExchangeData& data,
                         int operation,
                         gfx::NativeCursor cursor,
                         bool can_grab_pointer,
                         Delegate* delegate) = 0;

  // Cancels the drag.
  virtual void CancelDrag() = 0;
};

COMPONENT_EXPORT(WM)
void SetWmDragHandler(PlatformWindow* platform_window,
                      WmDragHandler* drag_handler);
COMPONENT_EXPORT(WM)
WmDragHandler* GetWmDragHandler(const PlatformWindow& platform_window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_WM_WM_DRAG_HANDLER_H_
