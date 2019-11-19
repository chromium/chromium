// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DRAG_UTILS_H_
#define UI_VIEWS_DRAG_UTILS_H_

#include <memory>

#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace gfx {
class Point;
}

namespace views {
class Widget;

// Starts a drag operation. This blocks until the drag operation completes.
VIEWS_EXPORT void RunShellDrag(gfx::NativeView view,
                               std::unique_ptr<ui::OSExchangeData> data,
                               const gfx::Point& location,
                               int operation,
                               ui::DragDropTypes::DragEventSource source);

// Returns the device scale for the display associated with this |widget|'s
// native view.
VIEWS_EXPORT float ScaleFactorForDragFromWidget(const Widget* widget);

}  // namespace views

#endif  // UI_VIEWS_DRAG_UTILS_H_
