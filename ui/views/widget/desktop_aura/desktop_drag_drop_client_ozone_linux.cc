// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone_linux.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/layout.h"
#include "ui/views/widget/widget.h"

namespace views {

DesktopDragDropClientOzoneLinux::DesktopDragDropClientOzoneLinux(
    aura::Window* root_window,
    ui::WmDragHandler* drag_handler)
    : DesktopDragDropClientOzone(root_window, drag_handler) {}

DesktopDragDropClientOzoneLinux::~DesktopDragDropClientOzoneLinux() = default;

ui::WmDragHandler::LocationDelegate*
DesktopDragDropClientOzoneLinux::GetLocationDelegate() {
  return this;
}

void DesktopDragDropClientOzoneLinux::OnDragLocationChanged(
    const gfx::Point& screen_point_px) {
  DCHECK(drag_context());

  if (!drag_context()->widget)
    return;
  const bool dispatch_mouse_event = !drag_context()->last_screen_location_px;
  drag_context()->last_screen_location_px = screen_point_px;
  if (dispatch_mouse_event) {
    // Post a task to dispatch mouse movement event when control returns to the
    // message loop. This allows smoother dragging since the events are
    // dispatched without waiting for the drag widget updates.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DesktopDragDropClientOzoneLinux::UpdateDragWidgetLocation,
            weak_factory_.GetWeakPtr()));
  }
}

void DesktopDragDropClientOzoneLinux::OnDragOperationChanged(
    ui::mojom::DragOperation operation) {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window());
  if (!cursor_client)
    return;

  ui::mojom::CursorType cursor_type = ui::mojom::CursorType::kNull;
  switch (operation) {
    case ui::mojom::DragOperation::kNone:
      cursor_type = ui::mojom::CursorType::kDndNone;
      break;
    case ui::mojom::DragOperation::kMove:
      cursor_type = ui::mojom::CursorType::kDndMove;
      break;
    case ui::mojom::DragOperation::kCopy:
      cursor_type = ui::mojom::CursorType::kDndCopy;
      break;
    case ui::mojom::DragOperation::kLink:
      cursor_type = ui::mojom::CursorType::kDndLink;
      break;
  }
  cursor_client->SetCursor(cursor_type);
}

std::optional<gfx::AcceleratedWidget>
DesktopDragDropClientOzoneLinux::GetDragWidget() {
  DCHECK(drag_context());
  if (drag_context()->widget)
    return drag_context()
        ->widget->GetNativeWindow()
        ->GetHost()
        ->GetAcceleratedWidget();
  return std::nullopt;
}

void DesktopDragDropClientOzoneLinux::UpdateDragWidgetLocation() {
  if (!drag_context())
    return;

  float scale_factor = ui::GetScaleFactorForNativeView(
      drag_context()->widget->GetNativeWindow());
  gfx::Point scaled_point = gfx::ScaleToRoundedPoint(
      *drag_context()->last_screen_location_px, 1.f / scale_factor);
  drag_context()->widget->SetBounds(
      gfx::Rect(scaled_point - drag_context()->offset, drag_context()->size));
  drag_context()->widget->StackAtTop();

  drag_context()->last_screen_location_px.reset();
}

}  // namespace views
