// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_DRAG_DROP_DELEGATE_H_
#define UI_AURA_CLIENT_DRAG_DROP_DELEGATE_H_

#include <memory>

#include "base/functional/callback.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/layer_tree_owner.h"

namespace ui {
class DropTargetEvent;
}  // namespace ui

namespace aura {
class Window;
namespace client {

struct AURA_EXPORT DragUpdateInfo {
  DragUpdateInfo();
  DragUpdateInfo(int op, ui::DataTransferEndpoint endpoint);

  DragUpdateInfo(const DragUpdateInfo& update_info);
  DragUpdateInfo& operator=(const DragUpdateInfo& update_info);

  // A bitmask of the DragDropTypes::DragOperation supported.
  int drag_operation = ui::DragDropTypes::DRAG_NONE;
  // An object representing the destination window.
  ui::DataTransferEndpoint data_endpoint{ui::EndpointType::kDefault};
};

// Delegate interface for drag and drop actions on aura::Window.
class AURA_EXPORT DragDropDelegate {
 public:
  // Callback emitted by GetDropCallback used to handle deferred drop events.
  // Note that it does not contain a location. If implementers need a location,
  // they should bind it in GetDropCallback. See crbug.com/1289902.
  using DropCallback = base::OnceCallback<void(
      std::unique_ptr<ui::OSExchangeData> data,
      ui::mojom::DragOperation& output_drag_op,
      std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner)>;

  // OnDragEntered is invoked when the mouse enters this window during a drag &
  // drop session. This is immediately followed by an invocation of
  // OnDragUpdated, and eventually one of OnDragExited, or GetDropCallback.
  virtual void OnDragEntered(const ui::DropTargetEvent& event) = 0;

  // Invoked during a drag and drop session while the mouse is over the window.
  // This should return DragUpdateInfo object based on the location of the
  // event.
  virtual DragUpdateInfo OnDragUpdated(const ui::DropTargetEvent& event) = 0;

  // Invoked during a drag and drop session when the mouse exits the window, or
  // when the drag session was canceled and the mouse was over the window.
  virtual void OnDragExited() = 0;

  // Invoked during a drag and drop session when the user release the mouse, but
  // the drop is held because of the DataTransferPolicyController. If the
  // returned callback is null, there's nothing to do and the drop event is
  // ignored.
  virtual DropCallback GetDropCallback(const ui::DropTargetEvent& event) = 0;

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
