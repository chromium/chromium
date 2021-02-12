// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_DRAG_DROP_DELEGATE_H_
#define UI_AURA_CLIENT_DRAG_DROP_DELEGATE_H_

#include <memory>

#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/dragdrop/os_exchange_data.h"

namespace ui {
class DropTargetEvent;
}

namespace aura {
class Window;
namespace client {

struct AURA_EXPORT DragUpdateInfo {
  DragUpdateInfo();
  DragUpdateInfo(int op, ui::DataTransferEndpoint endpoint);

  DragUpdateInfo& operator=(const DragUpdateInfo& update_info);

  // A bitmask of the DragDropTypes::DragOperation supported.
  int drag_operation = ui::DragDropTypes::DRAG_NONE;
  // An object representing the destination window.
  ui::DataTransferEndpoint data_endpoint{ui::EndpointType::kDefault};
};

// Delegate interface for drag and drop actions on aura::Window.
class AURA_EXPORT DragDropDelegate {
 public:
  // OnDragEntered is invoked when the mouse enters this window during a drag &
  // drop session. This is immediately followed by an invocation of
  // OnDragUpdated, and eventually one of OnDragExited or OnPerformDrop.
  virtual void OnDragEntered(const ui::DropTargetEvent& event) = 0;

  // Invoked during a drag and drop session while the mouse is over the window.
  // This should return DragUpdateInfo object based on the location of the
  // event.
  virtual DragUpdateInfo OnDragUpdated(const ui::DropTargetEvent& event) = 0;

  // Invoked during a drag and drop session when the mouse exits the window, or
  // when the drag session was canceled and the mouse was over the window.
  virtual void OnDragExited() = 0;

  // Invoked during a drag and drop session when OnDragUpdated returns a valid
  // operation and the user release the mouse. This function gets the ownership
  // of underlying OSExchangeData. A reference to this same OSExchangeData is
  // also stored in the DropTargetEvent. Implementor of this function should be
  // aware of keeping the OSExchageData alive until it wants to access it
  // through the parameter or the stored reference in DropTargetEvent.
  virtual ui::mojom::DragOperation OnPerformDrop(
      const ui::DropTargetEvent& event,
      std::unique_ptr<ui::OSExchangeData> data) = 0;

 protected:
  virtual ~DragDropDelegate() {}
};

AURA_EXPORT void SetDragDropDelegate(Window* window,
                                     DragDropDelegate* delegate);
AURA_EXPORT DragDropDelegate* GetDragDropDelegate(Window* window);

AURA_EXPORT extern const WindowProperty<DragDropDelegate*>* const
    kDragDropDelegateKey;

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_DRAG_DROP_DELEGATE_H_
