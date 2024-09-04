// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_DRAG_DROP_CLIENT_OBSERVER_H_
#define UI_AURA_CLIENT_DRAG_DROP_CLIENT_OBSERVER_H_

#include "build/chromeos_buildflags.h"
#include "ui/aura/aura_export.h"

namespace ui {
namespace mojom {
enum class DragOperation;
}  // namespace mojom

class DropTargetEvent;
}  // namespace ui

namespace aura {
namespace client {

// Observes drag-and-drop sessions. NOTE: drop could be asynchronous. Therefore,
// an active async drop could be interrupted by a new drag-and-drop session. In
// this case, observers are NOT notified of the interrupted async drop.
// Some possible calling orders are listed below:
// 1. Sync/async drop completion: `OnDragStarted()` -> `OnDragUpdated()` ->
// `OnDragCompleted()` -> `OnDropCompleted()`.
// 2. Async drop cancellation: OnDragStarted()` -> `OnDragUpdated()` ->
// `OnDragCompleted()` -> `OnDragCancelled()`.
class AURA_EXPORT DragDropClientObserver {
 public:
  virtual ~DragDropClientObserver() = default;

  // Called when dragging started.
  virtual void OnDragStarted() {}

  // Called when dragging is updated.
  virtual void OnDragUpdated(const ui::DropTargetEvent& event) {}

  // Called when dragging completes successfully.
  virtual void OnDragCompleted(const ui::DropTargetEvent& event) {}

  // Called when dragging is cancelled.
  // NOTE:
  // 1. Drag 'n drop cancellations may be processed asynchronously.
  // Hence, this hook might be called before the action is actually processed.
  // 2. This method is called in a disallowed async drop. In this case,
  // `OnDragCancelled()` is called after `OnDragCompleted()`.
  virtual void OnDragCancelled() {}

  // Called when drop is completed. `drag_operation` indicates the operation
  // when drop completes.
  // NOTE: drop completion could be performed asynchronously. When an async drop
  // is completed after a new drag-and-drop session starts, this method is not
  // called for this drop.
  virtual void OnDropCompleted(ui::mojom::DragOperation drag_operation) {}

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Called when the set of currently selected drag operation changes during the
  // drag. |action| is a bitmask of the actions selected by the client. This is
  // to provide feedback during the operation, it does not take precedence over
  // the operation returned from StartDragAndDrop.
  virtual void OnDragActionsChanged(int actions) {}
#endif

  // Called during destruction of the observed `DragDropClient`. Note that the
  // client being destroyed does not necessarily imply the end of the drag
  // session.
  virtual void OnDragDropClientDestroying() {}
};

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_DRAG_DROP_CLIENT_OBSERVER_H_
