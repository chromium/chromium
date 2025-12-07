// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_pointer.h"

#include <linux/input.h>
#include <wayland-util.h>

#include <optional>

#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/version.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"

namespace ui {

namespace {

// See TODO in //ui/ozone/common/features.cc
wl::EventDispatchPolicy GetEventDispatchPolicy() {
  return IsDispatchPointerEventsOnFrameEventEnabled()
             ? wl::EventDispatchPolicy::kOnFrame
             : wl::EventDispatchPolicy::kImmediate;
}

}  // namespace

WaylandPointer::WaylandPointer(wl_pointer* pointer,
                               WaylandConnection* connection,
                               Delegate* delegate)
    : obj_(pointer), connection_(connection), delegate_(delegate) {
  static constexpr wl_pointer_listener kPointerListener = {
      .enter = &OnEnter,
      .leave = &OnLeave,
      .motion = &OnMotion,
      .button = &OnButton,
      .axis = &OnAxis,
      .frame = &OnFrame,
      .axis_source = &OnAxisSource,
      .axis_stop = &OnAxisStop,
      .axis_discrete = &OnAxisDiscrete,
      .axis_value120 = &OnAxisValue120,
      .axis_relative_direction = &OnAxisRelativeDirection,
  };
  wl_pointer_add_listener(obj_.get(), &kPointerListener, this);
}

WaylandPointer::~WaylandPointer() {
  // If a cursor already exists, we need to reset it first before
  // destroying the pointer to prevent dangling references.
  connection_->ResetCursor();
  // Even though, WaylandPointer::Leave is always called when Wayland destroys
  // wl_pointer, it's better to be explicit as some Wayland compositors may have
  // bugs.
  delegate_->OnPointerFocusChanged(nullptr, {}, EventTimeForNow(),
                                   wl::EventDispatchPolicy::kImmediate);
  delegate_->ReleasePressedPointerButtons(nullptr, EventTimeForNow());
}

// static
void WaylandPointer::OnEnter(void* data,
                             wl_pointer* pointer,
                             uint32_t serial,
                             wl_surface* surface,
                             wl_fixed_t surface_x,
                             wl_fixed_t surface_y) {
  // enter event doesn't have timestamp. Use EventTimeForNow().
  const auto timestamp = EventTimeForNow();
  auto* self = static_cast<WaylandPointer*>(data);

  self->connection_->serial_tracker().UpdateSerial(wl::SerialType::kMouseEnter,
                                                   serial);
  WaylandWindow* window = wl::RootWindowFromWlSurface(surface);
  if (!window) {
    return;
  }
  self->delegate_->OnPointerFocusChanged(
      window,
      gfx::PointF(static_cast<float>(wl_fixed_to_double(surface_x)),
                  static_cast<float>(wl_fixed_to_double(surface_y))),
      timestamp, GetEventDispatchPolicy());
}

// static
void WaylandPointer::OnLeave(void* data,
                             wl_pointer* pointer,
                             uint32_t serial,
                             wl_surface* surface) {
  // leave event doesn't have timestamp. Use EventTimeForNow().
  const auto timestamp = EventTimeForNow();
  auto* self = static_cast<WaylandPointer*>(data);

  self->connection_->serial_tracker().ResetSerial(wl::SerialType::kMouseEnter);
  self->delegate_->OnPointerFocusChanged(nullptr,
                                         self->delegate_->GetPointerLocation(),
                                         timestamp, GetEventDispatchPolicy());
}

// static
void WaylandPointer::OnMotion(void* data,
                              wl_pointer* pointer,
                              uint32_t time,
                              wl_fixed_t surface_x,
                              wl_fixed_t surface_y) {
  auto* self = static_cast<WaylandPointer*>(data);

  self->delegate_->OnPointerMotionEvent(
      gfx::PointF(wl_fixed_to_double(surface_x), wl_fixed_to_double(surface_y)),
      wl::EventMillisecondsToTimeTicks(time), GetEventDispatchPolicy(),
      /*is_synthesized=*/false);
}

// static
void WaylandPointer::OnButton(void* data,
                              wl_pointer* pointer,
                              uint32_t serial,
                              uint32_t time,
                              uint32_t button,
                              uint32_t state) {
  auto* self = static_cast<WaylandPointer*>(data);
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

  EventType type = state == WL_POINTER_BUTTON_STATE_PRESSED
                       ? EventType::kMousePressed
                       : EventType::kMouseReleased;
  if (type == EventType::kMousePressed) {
    self->connection_->serial_tracker().UpdateSerial(
        wl::SerialType::kMousePress, serial);
  }
  self->delegate_->OnPointerButtonEvent(
      type, changed_button, wl::EventMillisecondsToTimeTicks(time),
      /*window=*/nullptr, GetEventDispatchPolicy(),
      /*allow_release_of_unpressed_button=*/false, /*is_synthesized=*/false);
}

// static
void WaylandPointer::OnAxis(void* data,
                            wl_pointer* pointer,
                            uint32_t time,
                            uint32_t axis,
                            wl_fixed_t value) {
  // Wayland compositors send axis events with values in the surface coordinate
  // space. They send a value of 10 per mouse wheel click by convention, so
  // clients (e.g. GTK+) typically scale down by this amount to convert to
  // discrete step coordinates. wl_pointer version 5 improves the situation by
  // adding axis sources and discrete axis events.
  const double kAxisValueScale = 10.0;
  const double delta = -wl_fixed_to_double(value) / kAxisValueScale *
                       MouseWheelEvent::kWheelDelta;
  const auto timestamp = wl::EventMillisecondsToTimeTicks(time);
  auto* self = static_cast<WaylandPointer*>(data);
  self->OnAxisImpl(delta, axis, timestamp, /*is_high_resolution=*/false);
}

// ---- Version 5 ----

// static
void WaylandPointer::OnFrame(void* data, wl_pointer* pointer) {
  auto* self = static_cast<WaylandPointer*>(data);
  // The frame event ends the sequence of pointer events.  Clear the flag.  The
  // next frame will set it when necessary.
  self->axis_source_received_ = false;
  self->delegate_->OnPointerFrameEvent();
}

// static
void WaylandPointer::OnAxisSource(void* data,
                                  wl_pointer* pointer,
                                  uint32_t axis_source) {
  auto* self = static_cast<WaylandPointer*>(data);
  self->axis_source_received_ = true;
  self->delegate_->OnPointerAxisSourceEvent(axis_source);
}

// static
void WaylandPointer::OnAxisStop(void* data,
                                wl_pointer* pointer,
                                uint32_t time,
                                uint32_t axis) {
  auto* self = static_cast<WaylandPointer*>(data);
  self->delegate_->OnPointerAxisStopEvent(
      axis, wl::EventMillisecondsToTimeTicks(time));
}

// static
void WaylandPointer::OnAxisDiscrete(void* data,
                                    wl_pointer* pointer,
                                    uint32_t axis,
                                    int32_t discrete) {
  // Deprecated since version 8
  NOTIMPLEMENTED_LOG_ONCE();
}

// --- Version 8 ---

// static
void WaylandPointer::OnAxisValue120(void* data,
                                    wl_pointer* pointer,
                                    uint32_t axis,
                                    int32_t value120) {
  const double delta = -value120;
  auto* self = static_cast<WaylandPointer*>(data);
  self->OnAxisImpl(delta, axis, /*timestamp=*/std::nullopt,
                   /*is_high_resolution=*/true);
}

void WaylandPointer::OnAxisImpl(double delta,
                                uint32_t axis,
                                std::optional<base::TimeTicks> timestamp,
                                bool is_high_resolution) {
  gfx::Vector2dF offset;
  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
    offset.set_y(delta);
  } else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
    offset.set_x(delta);
  } else {
    return;
  }
  delegate_->OnPointerAxisEvent(offset, timestamp, is_high_resolution);

  // If we did not receive the axis event source explicitly, set it to the mouse
  // wheel so far.  Should this be a part of some complex event coming from the
  // different source, the compositor will let us know sooner or later.
  if (!axis_source_received_) {
    delegate_->OnPointerAxisSourceEvent(WL_POINTER_AXIS_SOURCE_WHEEL);
  }
}

// --- Version 9 ---

// static
void WaylandPointer::OnAxisRelativeDirection(void* data,
                                             wl_pointer* obj,
                                             uint32_t axis,
                                             uint32_t direction) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ui
