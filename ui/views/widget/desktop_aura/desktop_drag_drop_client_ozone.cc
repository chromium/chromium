// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_handler/wm_drag_handler.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"

namespace views {

namespace {

aura::Window* GetTargetWindow(aura::Window* root_window,
                              const gfx::Point& point) {
  gfx::Point root_location(point);
  root_window->GetHost()->ConvertScreenInPixelsToDIP(&root_location);
  return root_window->GetEventHandlerForPoint(root_location);
}

}  // namespace

DesktopDragDropClientOzone::DesktopDragDropClientOzone(
    aura::Window* root_window,
    views::DesktopNativeCursorManager* cursor_manager,
    ui::WmDragHandler* drag_handler)
    : root_window_(root_window),
      cursor_manager_(cursor_manager),
      drag_handler_(drag_handler) {}

DesktopDragDropClientOzone::~DesktopDragDropClientOzone() {
  ResetDragDropTarget();

  if (in_move_loop_)
    DragCancel();
}

int DesktopDragDropClientOzone::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
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
      *data.get(), operation, cursor_client->GetCursor(),
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

void DesktopDragDropClientOzone::OnDragEnter(
    const gfx::PointF& point,
    std::unique_ptr<ui::OSExchangeData> data,
    int operation) {
  last_drag_point_ = point;
  drag_operation_ = operation;

  // If it doesn't have |data|, it defers sending events to
  // |drag_drop_delegate_|. It will try again before handling drop.
  if (!data)
    return;

  os_exchange_data_ = std::move(data);
  std::unique_ptr<ui::DropTargetEvent> event = CreateDropTargetEvent(point);
  if (drag_drop_delegate_ && event)
    drag_drop_delegate_->OnDragEntered(*event);
}

int DesktopDragDropClientOzone::OnDragMotion(const gfx::PointF& point,
                                             int operation) {
  last_drag_point_ = point;
  drag_operation_ = operation;
  int client_operation =
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE;

  if (os_exchange_data_) {
    std::unique_ptr<ui::DropTargetEvent> event = CreateDropTargetEvent(point);
    // If |os_exchange_data_| has a valid data, |drag_drop_delegate_| returns
    // the operation which it expects.
    if (drag_drop_delegate_ && event)
      client_operation = drag_drop_delegate_->OnDragUpdated(*event);
  }
  return client_operation;
}

void DesktopDragDropClientOzone::OnDragDrop(
    std::unique_ptr<ui::OSExchangeData> data) {
  // If it doesn't have |os_exchange_data_|, it needs to update it with |data|.
  if (!os_exchange_data_) {
    DCHECK(data);
    os_exchange_data_ = std::move(data);
    std::unique_ptr<ui::DropTargetEvent> event =
        CreateDropTargetEvent(last_drag_point_);
    // Sends the deferred drag events to |drag_drop_delegate_| before handling
    // drop.
    if (drag_drop_delegate_ && event) {
      drag_drop_delegate_->OnDragEntered(*event);
      // TODO(jkim): It doesn't use the return value from 'OnDragUpdated' and
      // doesn't have a chance to update the expected operation.
      // https://crbug.com/875164
      drag_drop_delegate_->OnDragUpdated(*event);
    }
  } else {
    // If it has |os_exchange_data_|, it doesn't expect |data| on OnDragDrop.
    DCHECK(!data);
  }
  PerformDrop();
}

void DesktopDragDropClientOzone::OnDragLeave() {
  os_exchange_data_.reset();
  ResetDragDropTarget();
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

std::unique_ptr<ui::DropTargetEvent>
DesktopDragDropClientOzone::CreateDropTargetEvent(const gfx::PointF& location) {
  const gfx::Point point(location.x(), location.y());
  aura::Window* window = GetTargetWindow(root_window_, point);
  if (!window)
    return nullptr;

  UpdateDragDropDelegate(window);
  gfx::Point root_location(location.x(), location.y());
  root_window_->GetHost()->ConvertScreenInPixelsToDIP(&root_location);
  gfx::PointF target_location(root_location);
  aura::Window::ConvertPointToTarget(root_window_, window, &target_location);

  return std::make_unique<ui::DropTargetEvent>(
      *os_exchange_data_, target_location, gfx::PointF(root_location),
      drag_operation_);
}

void DesktopDragDropClientOzone::UpdateDragDropDelegate(aura::Window* window) {
  aura::client::DragDropDelegate* delegate =
      aura::client::GetDragDropDelegate(window);

  if (drag_drop_delegate_ == delegate)
    return;

  ResetDragDropTarget();
  if (delegate)
    drag_drop_delegate_ = delegate;
}

void DesktopDragDropClientOzone::ResetDragDropTarget() {
  if (!drag_drop_delegate_)
    return;
  drag_drop_delegate_->OnDragExited();
  drag_drop_delegate_ = nullptr;
}

void DesktopDragDropClientOzone::PerformDrop() {
  std::unique_ptr<ui::DropTargetEvent> event =
      CreateDropTargetEvent(last_drag_point_);
  if (drag_drop_delegate_ && event)
    drag_operation_ = drag_drop_delegate_->OnPerformDrop(
        *event, std::move(os_exchange_data_));
  DragDropSessionCompleted();
}

}  // namespace views
