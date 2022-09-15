// Copyright 2012 The Chromium Authors
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
      desktop_host_(desktop_host) {
  drop_target_ = new DesktopDropTargetWin(root_window);
  drop_target_->Init(window);
}

DesktopDragDropClientWin::~DesktopDragDropClientWin() {
  if (drag_drop_in_progress_)
    DragCancel();
}

ui::mojom::DragOperation DesktopDragDropClientWin::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int allowed_operations,
    ui::mojom::DragEventSource source) {
  drag_drop_in_progress_ = true;
  gfx::Point touch_screen_point;
  if (source == ui::mojom::DragEventSource::kTouch) {
    touch_screen_point =
        screen_location + source_window->GetBoundsInScreen().OffsetFromOrigin();
    source_window->GetHost()->ConvertDIPToPixels(&touch_screen_point);
    desktop_host_->StartTouchDrag(touch_screen_point);
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

  // Never consider the current scope as hung. The hang watching deadline (if
  // any) is not valid since the user can take unbounded time to complete the
  // drag. (http://crbug.com/806174)
  base::HangWatcher::InvalidateActiveExpectations();

  HRESULT result = ::DoDragDrop(
      ui::OSExchangeDataProviderWin::GetIDataObject(*data.get()),
      drag_source_.Get(),
      ui::DragDropTypes::DragOperationToDropEffect(allowed_operations),
      &effect);
  if (alive && source == ui::mojom::DragEventSource::kTouch) {
    desktop_host_->FinishTouchDrag(touch_screen_point);
    // Move the mouse cursor to where the drag drop started, to avoid issues
    // when the drop is outside of the Chrome window.
    ::SetCursorPos(touch_screen_point.x(), touch_screen_point.y());
  }
  drag_source_copy->set_data(nullptr);

  if (alive)
    drag_drop_in_progress_ = false;

  if (result != DRAGDROP_S_DROP)
    effect = DROPEFFECT_NONE;

  return ui::PreferredDragOperation(
      ui::DragDropTypes::DropEffectToDragOperation(effect));
}

void DesktopDragDropClientWin::DragCancel() {
  drag_source_->CancelDrag();
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
