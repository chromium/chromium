// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_WM_WM_DRAG_HANDLER_H_
#define UI_PLATFORM_WINDOW_WM_WM_DRAG_HANDLER_H_

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
}

namespace ui {
class PlatformWindow;
class OSExchangeData;

class COMPONENT_EXPORT(WM) WmDragHandler {
 public:
  // Notifies when the drag operation finished.
  using DragFinishedCallback =
      base::OnceCallback<void(mojom::DragOperation operation)>;

  // A delegate class for a platform on which chrome manages a drag image and
  // needs to receive the drag location. This can be null if the platform itself
  // manages the drag image.
  class LocationDelegate {
   public:
    // Called every time when the drag location has changed.
    virtual void OnDragLocationChanged(const gfx::Point& screen_point_px) = 0;
    // Called when the currently negotiated operation has changed.
    virtual void OnDragOperationChanged(mojom::DragOperation operation) = 0;
    // DragWidget (if any) should be ignored when finding top window and
    // dispatching mouse events.
    virtual std::optional<gfx::AcceleratedWidget> GetDragWidget() = 0;

   protected:
    virtual ~LocationDelegate();
  };

  // Starts dragging `data`. Whereas, `operations` is a bitmask of
  // DragDropTypes::DragOperation values, which defines possible operations for
  // the drag source. The destination sets the resulting operation when the drop
  // action is performed. `source` indicates the source event type triggering
  // the drag, and `can_grab_pointer` indicates whether the implementation can
  // grab the mouse pointer (some platforms may need this). In progress updates
  // on the drag operation come back through the `location_delegate` on the
  // platform that chrome needs manages a drag image). This can be null if the
  // platform manages a drag image. `drag_started_callback` is called after the
  // request to start the drag was sent to the OS (see below for details), and
  // `drag_finished_callback` is called when drag operation finishes. These
  // callbacks are necessary because of the nested message loop (see below).
  //
  // `drag_started_callback` is called if and only if the drag actually starts.
  // If initialization fails and the drag doesn't start, it will not be called.
  // Also note that it might be called even when this method returns false if
  // the drag does start but is cancelled later on.
  //
  // This method runs a nested message loop, returning when the drag operation
  // is done. Care must be taken when calling this as it's entirely possible
  // that when this returns this object (and the calling object) have been
  // destroyed.
  //
  // Returns whether the operation ended well (i.e., had not been canceled).
  virtual bool StartDrag(const OSExchangeData& data,
                         int operations,
                         mojom::DragEventSource source,
                         gfx::NativeCursor cursor,
                         bool can_grab_pointer,
                         base::OnceClosure drag_started_callback,
                         DragFinishedCallback drag_finished_callback,
                         LocationDelegate* location_delegate) = 0;

  // Cancels the drag.
  virtual void CancelDrag() = 0;

  // Updates the drag image. An empty |image| may be used to hide a previously
  // set non-empty drag image, and a non-empty |image| shows the drag image
  // again if it was previously hidden.
  //
  // This must be called during an active drag session.
  virtual void UpdateDragImage(const gfx::ImageSkia& image,
                               const gfx::Vector2d& offset) = 0;

  // Returns whether capture should be released before a StartDrag() call.
  virtual bool ShouldReleaseCaptureForDrag(ui::OSExchangeData* data) const;
};

COMPONENT_EXPORT(WM)
void SetWmDragHandler(PlatformWindow* platform_window,
                      WmDragHandler* drag_handler);
COMPONENT_EXPORT(WM)
WmDragHandler* GetWmDragHandler(const PlatformWindow& platform_window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_WM_WM_DRAG_HANDLER_H_
