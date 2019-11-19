// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_DRAG_DROP_CLIENT_H_
#define UI_AURA_CLIENT_DRAG_DROP_CLIENT_H_

#include <memory>

#include "ui/aura/aura_export.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
}

namespace ui {
class OSExchangeData;
}

namespace aura {
class Window;
namespace client {

class DragDropClientObserver;

// An interface implemented by an object that controls a drag and drop session.
class AURA_EXPORT DragDropClient {
 public:
  virtual ~DragDropClient() {}

  // Initiates a drag and drop session. Returns the drag operation that was
  // applied at the end of the drag drop session. |screen_location| is in
  // screen coordinates. At most one drag and drop operation is allowed.
  // It must not start drag operation while |IsDragDropInProgress| returns true.
  virtual int StartDragAndDrop(std::unique_ptr<ui::OSExchangeData> data,
                               aura::Window* root_window,
                               aura::Window* source_window,
                               const gfx::Point& screen_location,
                               int operation,
                               ui::DragDropTypes::DragEventSource source) = 0;

  // Called when a drag and drop session is cancelled.
  virtual void DragCancel() = 0;

  // Returns true if a drag and drop session is in progress.
  virtual bool IsDragDropInProgress() = 0;

  virtual void AddObserver(DragDropClientObserver* observer) = 0;
  virtual void RemoveObserver(DragDropClientObserver* observer) = 0;
};

AURA_EXPORT void SetDragDropClient(Window* root_window,
                                   DragDropClient* client);
AURA_EXPORT DragDropClient* GetDragDropClient(Window* root_window);

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_DRAG_DROP_CLIENT_H_
