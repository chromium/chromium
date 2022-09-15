// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_DRAG_DROP_CLIENT_OBSERVER_H_
#define UI_AURA_CLIENT_DRAG_DROP_CLIENT_OBSERVER_H_

#include "build/chromeos_buildflags.h"
#include "ui/aura/aura_export.h"

namespace ui {
class DropTargetEvent;
}  // namespace ui

namespace aura {
namespace client {

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
  //
  // NOTE: Drag 'n drop cancellations may be processed asynchronously.
  // Hence, this hook might be called before the action is actually processed.
  virtual void OnDragCancelled() {}

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Called when the set of currently selected drag operation changes during the
  // drag. |action| is a bitmask of the actions selected by the client. This is
  // to provide feedback during the operation, it does not take precedence over
  // the operation returned from StartDragAndDrop.
  virtual void OnDragActionsChanged(int actions) {}
#endif
};

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_DRAG_DROP_CLIENT_OBSERVER_H_
