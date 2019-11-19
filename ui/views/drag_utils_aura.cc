// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/drag_utils.h"

#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace views {

void RunShellDrag(gfx::NativeView view,
                  std::unique_ptr<ui::OSExchangeData> data,
                  const gfx::Point& location,
                  int operation,
                  ui::DragDropTypes::DragEventSource source) {
  gfx::Point screen_location(location);
  wm::ConvertPointToScreen(view, &screen_location);
  aura::Window* root_window = view->GetRootWindow();
  if (aura::client::GetDragDropClient(root_window)) {
    aura::client::GetDragDropClient(root_window)
        ->StartDragAndDrop(std::move(data), root_window, view, screen_location,
                           operation, source);
  }
}

}  // namespace views
