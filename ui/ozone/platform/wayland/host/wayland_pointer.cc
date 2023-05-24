// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_pointer.h"

#include <linux/input.h>
#include <stylus-unstable-v2-client-protocol.h>

#include "base/logging.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"

namespace ui {

namespace {

// TODO(https://crbug.com/1353873): Remove this method when Compositors other
// than Exo comply with `wl_pointer.frame`.
wl::EventDispatchPolicy EventDispatchPolicyForPlatform() {
  return
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      wl::EventDispatchPolicy::kOnFrame;
#else
      wl::EventDispatchPolicy::kImmediate;
#endif
}

bool ShouldSuppressPointerEnterOrLeaveEvents(WaylandConnection* connection) {
  // Some Compositors (eg Exo) send spurious wl_pointer.enter|leave events
  // during ongoing tab drag 'n drop operations.
  //
  // While this needs to be fixed on the Compositor side, the particular
  // scenario of bogus events interfere w/ Lacros' tab dragging detaching
  // and retaching behavior.
  // Basically, the spurious `wl_pointer.enter` and `wl_pointer.leave` events
  // conflict with logic that sets the 'focused window' when a
  // `wl_drag_source.enter` event is received. For this reason, ignore those
  // events.
  if (connection->zaura_shell() &&
      connection->zaura_shell()->HasBugFix(1405471)) {
    return false;
  }

  const bool is_dragging_window =
      connection->window_drag_controller() &&
      connection->window_drag_controller()->state() !=
          WaylandWindowDragController::State::kIdle;
  return is_dragging_window;
}

}  // namespace

WaylandPointer::WaylandPointer(wl_pointer* pointer,
                               WaylandConnection* connection,
                               Delegate* delegate)
    : obj_(pointer), connection_(connection), delegate_(delegate) {
  static constexpr wl_pointer_listener listener = {
      &Enter, &Leave,      &Motion,   &Button,       &Axis,
      &Frame, &AxisSource, &AxisStop, &AxisDiscrete, &AxisValue120,
  };

  wl_pointer_add_listener(obj_.get(), &listener, this);

  SetupStylus();
}

WaylandPointer::~WaylandPointer() {
  // Even though, WaylandPointer::Leave is always called when Wayland destroys
  // wl_pointer, it's better to be explicit as some Wayland compositors may have
  // bugs.
  delegate_->OnPointerFocusChanged(nullptr, {},
                                   wl::EventDispatchPolicy::kImmediate);
  delegate_->OnResetPointerFlags();
}

// static
void WaylandPointer::Enter(void* data,
                           wl_pointer* obj,
                           uint32_t serial,
                           wl_surface* surface,
                           wl_fixed_t surface_x,
                           wl_fixed_t surface_y) {
  auto* pointer = static_cast<WaylandPointer*>(data);

  if (ShouldSuppressPointerEnterOrLeaveEvents(pointer->connection_)) {
    LOG(ERROR) << "Compositor sent a spurious wl_pointer.enter event during"
                  " a window drag 'n drop operation. IGNORING.";
    return;
  }

  pointer->connection_->serial_tracker().UpdateSerial(
      wl::SerialType::kMouseEnter, serial);

  WaylandWindow* window = wl::RootWindowFromWlSurface(surface);
  if (!window) {
    return;
  }

  gfx::PointF location{static_cast<float>(wl_fixed_to_double(surface_x)),
                       static_cast<float>(wl_fixed_to_double(surface_y))};

  pointer->delegate_->OnPointerFocusChanged(
      window, pointer->connection_->MaybeConvertLocation(location, window),
      EventDispatchPolicyForPlatform());
}

// static
void WaylandPointer::Leave(void* data,
                           wl_pointer* obj,
                           uint32_t serial,
                           wl_surface* surface) {
  auto* pointer = static_cast<WaylandPointer*>(data);

  if (ShouldSuppressPointerEnterOrLeaveEvents(pointer->connection_)) {
    LOG(ERROR) << "Compositor sent a spurious wl_pointer.leave event during"
                  " a window drag 'n drop operation. IGNORING.";
    return;
  }

  pointer->connection_->serial_tracker().ResetSerial(
      wl::SerialType::kMouseEnter);

  auto event_dispatch_policy =
      pointer->connection_->zaura_shell() &&
              pointer->connection_->zaura_shell()->HasBugFix(1352584)
          ? EventDispatchPolicyForPlatform()
          : wl::EventDispatchPolicy::kImmediate;

  pointer->delegate_->OnPointerFocusChanged(
      nullptr, pointer->delegate_->GetPointerLocation(), event_dispatch_policy);
}

// static
void WaylandPointer::Motion(void* data,
                            wl_pointer* obj,
                            uint32_t time,
                            wl_fixed_t surface_x,
                            wl_fixed_t surface_y) {
  auto* pointer = static_cast<WaylandPointer*>(data);
  gfx::PointF location(wl_fixed_to_double(surface_x),
                       wl_fixed_to_double(surface_y));
  const WaylandWindow* target = pointer->delegate_->GetPointerTarget();

  pointer->delegate_->OnPointerMotionEvent(
      pointer->connection_->MaybeConvertLocation(location, target),
      EventDispatchPolicyForPlatform());
}

// static
void WaylandPointer::Button(void* data,
                            wl_pointer* obj,
                            uint32_t serial,
                            uint32_t time,
                            uint32_t button,
                            uint32_t state) {
  auto* pointer = static_cast<WaylandPointer*>(data);
  int changed_button;
  switch (button) {
    case BTN_LEFT:
      changed_button = EF_LEFT_MOUSE_BUTTON;
      break;
    case BTN_MIDDLE:
      changed_button = EF_MIDDLE_MOUSE_BUTTON;
      break;
    case BTN_RIGHT:
      changed_button = EF_RIGHT_MOUSE_BUTTON;
      break;
    case BTN_BACK:
    case BTN_SIDE:
      changed_button = EF_BACK_MOUSE_BUTTON;
      break;
    case BTN_FORWARD:
    case BTN_EXTRA:
      changed_button = EF_FORWARD_MOUSE_BUTTON;
      break;
    default:
      return;
  }

  EventType type = state == WL_POINTER_BUTTON_STATE_PRESSED ? ET_MOUSE_PRESSED
                                                            : ET_MOUSE_RELEASED;
  if (type == ET_MOUSE_PRESSED) {
    pointer->connection_->serial_tracker().UpdateSerial(
        wl::SerialType::kMousePress, serial);
  }
  pointer->delegate_->OnPointerButtonEvent(type, changed_button,
                                           /*window=*/nullptr,
                                           EventDispatchPolicyForPlatform());
}

// static
void WaylandPointer::Axis(void* data,
                          wl_pointer* obj,
                          uint32_t time,
                          uint32_t axis,
                          wl_fixed_t value) {
  static const double kAxisValueScale = 10.0;
  auto* pointer = static_cast<WaylandPointer*>(data);
  gfx::Vector2dF offset;
  // Wayland compositors send axis events with values in the surface coordinate
  // space. They send a value of 10 per mouse wheel click by convention, so
  // clients (e.g. GTK+) typically scale down by this amount to convert to
  // discrete step coordinates. wl_pointer version 5 improves the situation by
  // adding axis sources and discrete axis events.
  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
    offset.set_y(-wl_fixed_to_double(value) / kAxisValueScale *
                 MouseWheelEvent::kWheelDelta);
  } else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
    offset.set_x(-wl_fixed_to_double(value) / kAxisValueScale *
                 MouseWheelEvent::kWheelDelta);
  } else {
    return;
  }
  // If we did not receive the axis event source explicitly, set it to the mouse
  // wheel so far.  Should this be a part of some complex event coming from the
  // different source, the compositor will let us know sooner or later.
  if (!pointer->axis_source_received_)
    pointer->delegate_->OnPointerAxisSourceEvent(WL_POINTER_AXIS_SOURCE_WHEEL);
  pointer->delegate_->OnPointerAxisEvent(offset);
}

// ---- Version 5 ----

// static
void WaylandPointer::Frame(void* data, wl_pointer* obj) {
  auto* pointer = static_cast<WaylandPointer*>(data);
  // The frame event ends the sequence of pointer events.  Clear the flag.  The
  // next frame will set it when necessary.
  pointer->axis_source_received_ = false;
  pointer->delegate_->OnPointerFrameEvent();
}

// static
void WaylandPointer::AxisSource(void* data,
                                wl_pointer* obj,
                                uint32_t axis_source) {
  auto* pointer = static_cast<WaylandPointer*>(data);
  pointer->axis_source_received_ = true;
  pointer->delegate_->OnPointerAxisSourceEvent(axis_source);
}

// static
void WaylandPointer::AxisStop(void* data,
                              wl_pointer* obj,
                              uint32_t time,
                              uint32_t axis) {
  auto* pointer = static_cast<WaylandPointer*>(data);
  pointer->delegate_->OnPointerAxisStopEvent(axis);
}

// static
void WaylandPointer::AxisDiscrete(void* data,
                                  wl_pointer* obj,
                                  uint32_t axis,
                                  int32_t discrete) {
  // TODO(crbug.com/1129259): Use this event for better handling of mouse wheel
  // events.
  NOTIMPLEMENTED_LOG_ONCE();
}

// --- Version 8 ---

// static
void WaylandPointer::AxisValue120(void* data,
                                  wl_pointer* obj,
                                  uint32_t axis,
                                  int32_t value120) {
  // TODO(crbug.com/1129259): Use this event for better handling of mouse wheel
  // events.
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandPointer::SetupStylus() {
  auto* stylus_v2 = connection_->stylus_v2();
  if (!stylus_v2)
    return;

  zcr_pointer_stylus_v2_.reset(
      zcr_stylus_v2_get_pointer_stylus(stylus_v2, obj_.get()));

  static zcr_pointer_stylus_v2_listener kPointerStylusV2Listener = {
      &Tool, &Force, &Tilt};
  zcr_pointer_stylus_v2_add_listener(zcr_pointer_stylus_v2_.get(),
                                     &kPointerStylusV2Listener, this);
}

// static
void WaylandPointer::Tool(void* data,
                          struct zcr_pointer_stylus_v2* x,
                          uint32_t wl_pointer_type) {
  auto* pointer = static_cast<WaylandPointer*>(data);

  ui::EventPointerType pointer_type = ui::EventPointerType::kMouse;
  switch (wl_pointer_type) {
    case (ZCR_POINTER_STYLUS_V2_TOOL_TYPE_PEN):
      pointer_type = EventPointerType::kPen;
      break;
    case (ZCR_POINTER_STYLUS_V2_TOOL_TYPE_ERASER):
      pointer_type = ui::EventPointerType::kEraser;
      break;
    case (ZCR_POINTER_STYLUS_V2_TOOL_TYPE_TOUCH):
      pointer_type = EventPointerType::kTouch;
      break;
    case (ZCR_POINTER_STYLUS_V2_TOOL_TYPE_NONE):
      break;
  }

  pointer->delegate_->OnPointerStylusToolChanged(pointer_type);
}

// static
void WaylandPointer::Force(void* data,
                           struct zcr_pointer_stylus_v2* obj,
                           uint32_t time,
                           wl_fixed_t force) {
  auto* pointer = static_cast<WaylandPointer*>(data);
  DCHECK(pointer);

  pointer->delegate_->OnPointerStylusForceChanged(wl_fixed_to_double(force));
}

// static
void WaylandPointer::Tilt(void* data,
                          struct zcr_pointer_stylus_v2* obj,
                          uint32_t time,
                          wl_fixed_t tilt_x,
                          wl_fixed_t tilt_y) {
  auto* pointer = static_cast<WaylandPointer*>(data);
  DCHECK(pointer);

  pointer->delegate_->OnPointerStylusTiltChanged(
      gfx::Vector2dF(wl_fixed_to_double(tilt_x), wl_fixed_to_double(tilt_y)));
}

}  // namespace ui
