// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_WINDOW_DRAG_CONTROLLER_TEST_API_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_WINDOW_DRAG_CONTROLLER_TEST_API_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"

namespace ui {

class WaylandWindow;
class WaylandToplevelWindow;

class WaylandWindowDragControllerTestApi {
 public:
  explicit WaylandWindowDragControllerTestApi(WaylandWindowDragController* impl)
      : impl_(impl) {}
  WaylandWindowDragControllerTestApi(
      const WaylandWindowDragControllerTestApi&) = delete;
  WaylandWindowDragControllerTestApi& operator=(
      const WaylandWindowDragControllerTestApi&) = delete;
  ~WaylandWindowDragControllerTestApi() = default;

  // Force drag controller to consider any of the window drag protocols (ie:
  // zcr-extended-drag-v1 or xdg-toplevel-drag-v1) to be available.
  void set_extended_drag_available(bool available) {
    impl_->window_drag_protocol_available_for_testing_ = available;
  }

  WaylandWindowDragController::State state() { return impl_->state_; }
  WaylandWindow* drag_target_window() { return impl_->drag_target_window_; }
  WaylandWindow* dragged_window() { return impl_->dragged_window_; }
  WaylandWindow* origin_window() { return impl_->origin_window_; }
  WaylandWindow* pointer_grab_owner() { return impl_->pointer_grab_owner_; }
  const gfx::Vector2d& drag_offset() const { return impl_->drag_offset_; }
  bool has_received_enter() const { return impl_->has_received_enter_; }

  WaylandTouch::Delegate* touch_delegate() { return impl_->touch_delegate_; }
  WaylandPointer::Delegate* pointer_delegate() {
    return impl_->pointer_delegate_;
  }

  std::optional<wl::Serial> GetSerial(mojom::DragEventSource drag_source,
                                      WaylandToplevelWindow* origin) {
    return impl_->GetSerial(drag_source, origin);
  }

  void EmulateOnDragDrop(base::TimeTicks timestamp) {
    impl_->OnDragDrop(timestamp);
  }

 private:
  raw_ptr<WaylandWindowDragController> impl_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_WINDOW_DRAG_CONTROLLER_TEST_API_H_
