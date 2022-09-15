// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/drag_drop_delegate.h"

#include "ui/base/class_property.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT,
                                       aura::client::DragDropDelegate*)

namespace aura {
namespace client {

DragUpdateInfo::DragUpdateInfo() = default;

DragUpdateInfo::DragUpdateInfo(int op, ui::DataTransferEndpoint endpoint)
    : drag_operation(op), data_endpoint(endpoint) {}

DragUpdateInfo::DragUpdateInfo(const DragUpdateInfo& update_info) = default;

DragUpdateInfo& DragUpdateInfo::operator=(const DragUpdateInfo& update_info) =
    default;

DEFINE_UI_CLASS_PROPERTY_KEY(DragDropDelegate*, kDragDropDelegateKey, nullptr)

void SetDragDropDelegate(Window* window, DragDropDelegate* delegate) {
  window->SetProperty(kDragDropDelegateKey, delegate);
}

DragDropDelegate* GetDragDropDelegate(Window* window) {
  return window->GetProperty(kDragDropDelegateKey);
}

}  // namespace client
}  // namespace aura
