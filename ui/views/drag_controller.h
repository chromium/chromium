// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DRAG_CONTROLLER_H_
#define UI_VIEWS_DRAG_CONTROLLER_H_

#include "ui/views/views_export.h"

namespace gfx {
class Point;
}

namespace ui {
class OSExchangeData;
}

namespace views {
class View;

// DragController is responsible for writing drag data for a view, as well as
// supplying the supported drag operations. Use DragController if you don't
// want to subclass.
class VIEWS_EXPORT DragController {
 public:
  // Writes the data for the drag.
  virtual void WriteDragDataForView(View* sender,
                                    const gfx::Point& press_pt,
                                    ui::OSExchangeData* data) = 0;

  // Returns the supported drag operations (see DragDropTypes for possible
  // values). A drag is only started if this returns a non-zero value.
  virtual int GetDragOperationsForView(View* sender,
                                       const gfx::Point& p) = 0;

  // Returns true if a drag operation can be started.
  // |press_pt| represents the coordinates where the mouse was initially
  // pressed down. |p| is the current mouse coordinates.
  virtual bool CanStartDragForView(View* sender,
                                   const gfx::Point& press_pt,
                                   const gfx::Point& p) = 0;

 protected:
  virtual ~DragController() = default;
};

}  // namespace views

#endif  // UI_VIEWS_DRAG_CONTROLLER_H_
