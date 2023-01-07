// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/drag_drop_client.h"

#include "ui/aura/window.h"
#include "ui/base/class_property.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(aura::client::DragDropClient*)

namespace aura {
namespace client {

DEFINE_UI_CLASS_PROPERTY_KEY(DragDropClient*,
                             kRootWindowDragDropClientKey,
                             nullptr)

void SetDragDropClient(Window* root_window, DragDropClient* client) {
  DCHECK_EQ(root_window->GetRootWindow(), root_window);
  root_window->SetProperty(kRootWindowDragDropClientKey, client);
}

DragDropClient* GetDragDropClient(Window* root_window) {
  if (root_window)
    DCHECK_EQ(root_window->GetRootWindow(), root_window);
  return root_window ?
      root_window->GetProperty(kRootWindowDragDropClientKey) : NULL;
}

}  // namespace client
}  // namespace aura
