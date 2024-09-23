// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DRAG_UTILS_H_
#define UI_VIEWS_DRAG_UTILS_H_

#include <memory>

#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace gfx {
class Point;
}

namespace views {
class Widget;

// Starts a drag operation. This blocks until the drag operation completes or is
// cancelled by calling `CancelShellDrag()`.
VIEWS_EXPORT void RunShellDrag(gfx::NativeView view,
                               std::unique_ptr<ui::OSExchangeData> data,
                               const gfx::Point& location,
                               int operation,
                               ui::mojom::DragEventSource source);

// Cancels a currently running drag operation. If `allow_widget_mismatch` is
// true, the check whether a drag session is currently running is skipped; this
// can be used to cancel a drag that was initiated by a different widget, if the
// platform supports it.
VIEWS_EXPORT void CancelShellDrag(gfx::NativeView view,
                                  bool allow_widget_mismatch = false);

// Returns the device scale for the display associated with this |widget|'s
// native view.
VIEWS_EXPORT float ScaleFactorForDragFromWidget(const Widget* widget);

}  // namespace views

#endif  // UI_VIEWS_DRAG_UTILS_H_
