// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/mock_drag_controller.h"

#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"

namespace views {

MockDragController::MockDragController() = default;

MockDragController::~MockDragController() = default;

void MockDragController::WriteDragDataForView(View* sender,
                                              const gfx::Point& press_pt,
                                              ui::OSExchangeData* data) {
  data->SetString(u"test");
}

int MockDragController::GetDragOperationsForView(View* sender,
                                                 const gfx::Point& p) {
  return ui::DragDropTypes::DRAG_COPY;
}

bool MockDragController::CanStartDragForView(View* sender,
                                             const gfx::Point& press_pt,
                                             const gfx::Point& p) {
  return true;
}

}  // namespace views
