// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_handler/wm_drag_handler.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"

namespace views {

DesktopDragDropClientOzone::DesktopDragDropClientOzone(
    aura::Window* root_window,
    views::DesktopNativeCursorManager* cursor_manager,
    ui::WmDragHandler* drag_handler)
    : root_window_(root_window),
      cursor_manager_(cursor_manager),
      drag_handler_(drag_handler) {}

DesktopDragDropClientOzone::~DesktopDragDropClientOzone() {
  if (in_move_loop_)
    DragCancel();
}

int DesktopDragDropClientOzone::StartDragAndDrop(
    const ui::OSExchangeData& data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& root_location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  if (!drag_handler_)
    return ui::DragDropTypes::DragOperation::DRAG_NONE;

  DCHECK(!in_move_loop_);
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_closure_ = run_loop.QuitClosure();

  // Chrome expects starting drag and drop to release capture.
  aura::Window* capture_window =
      aura::client::GetCaptureClient(root_window)->GetGlobalCaptureWindow();
  if (capture_window)
    capture_window->ReleaseCapture();

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);

  initial_cursor_ = source_window->GetHost()->last_cursor();
  drag_operation_ = operation;
  cursor_client->SetCursor(
      cursor_manager_->GetInitializedCursor(ui::CursorType::kGrabbing));

  drag_handler_->StartDrag(
      data, operation, cursor_client->GetCursor(),
      base::BindOnce(&DesktopDragDropClientOzone::OnDragSessionClosed,
                     base::Unretained(this)));
  in_move_loop_ = true;
  run_loop.Run();
  DragDropSessionCompleted();
  return drag_operation_;
}

void DesktopDragDropClientOzone::DragCancel() {
  QuitRunLoop();
}

bool DesktopDragDropClientOzone::IsDragDropInProgress() {
  return in_move_loop_;
}

void DesktopDragDropClientOzone::AddObserver(
    aura::client::DragDropClientObserver* observer) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopDragDropClientOzone::RemoveObserver(
    aura::client::DragDropClientObserver* observer) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopDragDropClientOzone::OnDragSessionClosed(int dnd_action) {
  drag_operation_ = dnd_action;
  QuitRunLoop();
}

void DesktopDragDropClientOzone::DragDropSessionCompleted() {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window_);
  if (!cursor_client)
    return;

  cursor_client->SetCursor(initial_cursor_);
}

void DesktopDragDropClientOzone::QuitRunLoop() {
  in_move_loop_ = false;
  if (quit_closure_.is_null())
    return;
  std::move(quit_closure_).Run();
}

}  // namespace views
