// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_win.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/scoped_observation.h"
#include "base/threading/hang_watcher.h"
#include "ui/aura/env.h"
#include "ui/aura/window_observer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drag_source_win.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#include "ui/base/win/event_creation_utils.h"
#include "ui/display/win/screen_win.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/desktop_aura/desktop_drop_target_win.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

namespace views {

namespace {

class SourceWindowObserver : public aura::WindowObserver {
 public:
  explicit SourceWindowObserver(aura::Window* window)
      : scoped_observation_(this) {
    scoped_observation_.Observe(window);
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    source_window_alive_ = false;
    scoped_observation_.Reset();
  }

  bool source_window_alive() { return source_window_alive_; }

 private:
  bool source_window_alive_ = true;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      scoped_observation_;
};

}  // namespace

DesktopDragDropClientWin::DesktopDragDropClientWin(
    aura::Window* root_window,
    HWND window,
    DesktopWindowTreeHostWin* desktop_host)
    : drag_drop_in_progress_(false), desktop_host_(desktop_host) {
  drop_target_ = new DesktopDropTargetWin(root_window);
  drop_target_->Init(window);
}

DesktopDragDropClientWin::~DesktopDragDropClientWin() {
  if (drag_drop_in_progress_) {
    DragCancel();
  }
}

ui::mojom::DragOperation DesktopDragDropClientWin::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int allowed_operations,
    ui::mojom::DragEventSource source) {
  gfx::Point touch_screen_point;
  if (source == ui::mojom::DragEventSource::kTouch) {
    source_window->GetHost()->ConvertDIPToPixels(&touch_screen_point);
    display::Screen* screen = display::Screen::Get();
    CHECK(screen);
    aura::Window* window =
        screen->GetWindowAtScreenPoint(screen->GetCursorScreenPoint());
    touch_screen_point = screen_location;
    source_window->GetHost()->ConvertDIPToPixels(&touch_screen_point);
    bool touch_down = aura::Env::GetInstance()->is_touch_down();
    bool touch_over_other_window =
        !window || window->GetRootWindow() != root_window;
    // Check that the cursor is over the window being dragged from. If not,
    // don't start the drag because ::DoDragDrop will not do the drag.
    if (!touch_down || touch_over_other_window) {
      return ui::PreferredDragOperation(
          ui::DragDropTypes::DropEffectToDragOperation(DROPEFFECT_NONE));
    }
    desktop_host_->StartTouchDrag(touch_screen_point);
  }
  // Observe the source window to avoid accessing it if the window is
  // destroyed while the drag is ongoing.
  SourceWindowObserver source_window_observer(source_window);
  base::WeakPtr<DesktopDragDropClientWin> alive(weak_factory_.GetWeakPtr());

  drag_drop_in_progress_ = true;
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
  if (source == ui::mojom::DragEventSource::kTouch) {
    if (source_window_observer.source_window_alive()) {
      // Kill the gesture that initiated the drag to avoid issues with lingering
      // touch events.
      source_window->CleanupGestureState();
    }
    if (alive) {
      desktop_host_->FinishTouchDrag(touch_screen_point);
    }
  }
  drag_source_copy->set_data(nullptr);

  if (alive) {
    drag_drop_in_progress_ = false;
  }

  if (result != DRAGDROP_S_DROP) {
    effect = DROPEFFECT_NONE;
  }

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
