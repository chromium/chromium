// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_win.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/threading/hang_watcher.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drag_source_win.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#include "ui/base/win/event_creation_utils.h"
#include "ui/display/win/screen_win.h"
#include "ui/views/widget/desktop_aura/desktop_drop_target_win.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

namespace views {

DesktopDragDropClientWin::DesktopDragDropClientWin(
    aura::Window* root_window,
    HWND window,
    DesktopWindowTreeHostWin* desktop_host)
    : drag_drop_in_progress_(false),
      drag_operation_(0),
      desktop_host_(desktop_host) {
  drop_target_ = new DesktopDropTargetWin(root_window);
  drop_target_->Init(window);
}

DesktopDragDropClientWin::~DesktopDragDropClientWin() {
  if (drag_drop_in_progress_)
    DragCancel();
}

int DesktopDragDropClientWin::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int operation,
    ui::mojom::DragEventSource source) {
  drag_drop_in_progress_ = true;
  drag_operation_ = operation;
  if (source == ui::mojom::DragEventSource::kTouch) {
    gfx::Point screen_point = display::win::ScreenWin::DIPToScreenPoint(
        {screen_location.x(), screen_location.y()});
    // Send a mouse down and mouse move before do drag drop runs its own event
    // loop. This is required for ::DoDragDrop to start the drag.
    ui::SendMouseEvent(screen_point,
                       MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_ABSOLUTE);
    ui::SendMouseEvent(screen_point, MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE);
    desktop_host_->SetInTouchDrag(true);
    // Gesture state gets left in a state where you can't start
    // another drag, unless it's cleaned up. Cleaning it up before starting
    // drag drop also fixes an issue with getting two kGestureScrollBegin events
    // in a row. See crbug.com/1120809.
    source_window->CleanupGestureState();
  }
  base::WeakPtr<DesktopDragDropClientWin> alive(weak_factory_.GetWeakPtr());

  drag_source_ = ui::DragSourceWin::Create();
  Microsoft::WRL::ComPtr<ui::DragSourceWin> drag_source_copy = drag_source_;
  drag_source_copy->set_data(data.get());
  ui::OSExchangeDataProviderWin::GetDataObjectImpl(*data)->set_in_drag_loop(
      true);

  DWORD effect;

  // Disable hang watching until the end of the function since the user can take
  // unbounded time to complete the drag. (http://crbug.com/806174)
  base::HangWatchScopeDisabled disabler;
  base::TimeTicks start_time = base::TimeTicks::Now();

  HRESULT result = ::DoDragDrop(
      ui::OSExchangeDataProviderWin::GetIDataObject(*data.get()),
      drag_source_.Get(),
      ui::DragDropTypes::DragOperationToDropEffect(operation), &effect);
  if (alive && source == ui::mojom::DragEventSource::kTouch) {
    // In a normal drag drop, ::DoDragDrop calls QueryContinueDrag every time
    // it gets a mouse or keyboard event. The windows doc
    // https://docs.microsoft.com/en-us/windows/win32/api/oleidl/nf-oleidl-idropsource-querycontinuedrag
    // says "every time it detects a change in keyboard or mouse button state"
    // but empirically, on a Yoga laptop with a touch screen running Windows 10,
    // it's called when it gets a mouse move event as well. (::DoDragDrop
    // doesn't support touch, so Chrome synthesizes mouse events from touch
    // events during drag drop.)
    // In the touch failure case, when ::DoDragDrop blocks waiting for a right
    // mouse button down event to start the drag, it only calls
    // QueryContinueDrag once, when it gets an event that terminates the blocked
    // drag drop, e.g., a swipe gesture from outside the Chrome window. So, we
    // detect the failure case when a drag drop lasts more than one second, and
    // QueryContinueDrag was not called more than once.
    // See crbug.com/1126230.
    UMA_HISTOGRAM_BOOLEAN("Windows.TouchDrag.Success",
                          drag_source_->num_query_continues() > 1 ||
                              (base::TimeTicks::Now() - start_time <
                               base::TimeDelta::FromSeconds(1)));
    desktop_host_->SetInTouchDrag(false);
  }
  drag_source_copy->set_data(nullptr);

  if (alive)
    drag_drop_in_progress_ = false;

  if (result != DRAGDROP_S_DROP)
    effect = DROPEFFECT_NONE;

  return ui::DragDropTypes::DropEffectToDragOperation(effect);
}

void DesktopDragDropClientWin::DragCancel() {
  drag_source_->CancelDrag();
  drag_operation_ = 0;
}

bool DesktopDragDropClientWin::IsDragDropInProgress() {
  return drag_drop_in_progress_;
}

void DesktopDragDropClientWin::AddObserver(
    aura::client::DragDropClientObserver* observer) {
  NOTIMPLEMENTED();
}

void DesktopDragDropClientWin::RemoveObserver(
    aura::client::DragDropClientObserver* observer) {
  NOTIMPLEMENTED();
}

void DesktopDragDropClientWin::OnNativeWidgetDestroying(HWND window) {
  if (drop_target_.get()) {
    RevokeDragDrop(window);
    drop_target_ = nullptr;
  }
}

}  // namespace views
