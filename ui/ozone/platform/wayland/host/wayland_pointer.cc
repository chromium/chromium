// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_pointer.h"

#include <linux/input.h>
#include <stylus-unstable-v2-client-protocol.h>

#include "base/logging.h"
#include "base/version.h"
#include "ui/events/base_event_utils.h"
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

// TODO(crbug.com/40235357): Remove this method when Compositors other
// than Exo comply with `wl_pointer.frame`.
wl::EventDispatchPolicy EventDispatchPolicyForPlatform() {
  return
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      wl::EventDispatchPolicy::kOnFrame;
#else
      wl::EventDispatchPolicy::kImmediate;
#endif
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
  };
  wl_pointer_add_listener(obj_.get(), &kPointerListener, this);

  SetupStylus();
}

WaylandPointer::~WaylandPointer() {
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

  if (self->SuppressFocusChangeEvents()) {
    LOG(WARNING) << "Suppressing enter event received during window drag.";
    return;
  }

  WaylandWindow* window = wl::RootWindowFromWlSurface(surface);
  if (!window) {
    return;
  }

  gfx::PointF location{static_cast<float>(wl_fixed_to_double(surface_x)),
                       static_cast<float>(wl_fixed_to_double(surface_y))};

  self->delegate_->OnPointerFocusChanged(
      window, self->connection_->MaybeConvertLocation(location, window),
      timestamp, EventDispatchPolicyForPlatform());
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

  if (self->SuppressFocusChangeEvents()) {
    LOG(WARNING) << "Suppressing leave event received during window drag.";
    return;
  }

  auto event_dispatch_policy = EventDispatchPolicyForPlatform();

  self->delegate_->OnPointerFocusChanged(nullptr,
                                         self->delegate_->GetPointerLocation(),
                                         timestamp, event_dispatch_policy);
}

// static
void WaylandPointer::OnMotion(void* data,
                              wl_pointer* pointer,
                              uint32_t time,
                              wl_fixed_t surface_x,
                              wl_fixed_t surface_y) {
  auto* self = static_cast<WaylandPointer*>(data);

  gfx::PointF location(wl_fixed_to_double(surface_x),
                       wl_fixed_to_double(surface_y));
  const WaylandWindow* target = self->delegate_->GetPointerTarget();

  self->delegate_->OnPointerMotionEvent(
      self->connection_->MaybeConvertLocation(location, target),
      wl::EventMillisecondsToTimeTicks(time), EventDispatchPolicyForPlatform(),
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
      /*window=*/nullptr, EventDispatchPolicyForPlatform(),
      /*allow_release_of_unpressed_button=*/false, /*is_synthesized=*/false);
}

// static
void WaylandPointer::OnAxis(void* data,
                            wl_pointer* pointer,
                            uint32_t time,
                            uint32_t axis,
                            wl_fixed_t value) {
  static const double kAxisValueScale = 10.0;
  auto* self = static_cast<WaylandPointer*>(data);
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
  auto timestamp = wl::EventMillisecondsToTimeTicks(time);
  if (!self->axis_source_received_) {
    self->delegate_->OnPointerAxisSourceEvent(WL_POINTER_AXIS_SOURCE_WHEEL);
  }
  self->delegate_->OnPointerAxisEvent(offset, timestamp);
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
  // TODO(crbug.com/40720099): Use this event for better handling of mouse wheel
  // events.
  NOTIMPLEMENTED_LOG_ONCE();
}

// --- Version 8 ---

// static
void WaylandPointer::OnAxisValue120(void* data,
                                    wl_pointer* pointer,
                                    uint32_t axis,
                                    int32_t value120) {
  // TODO(crbug.com/40720099): Use this event for better handling of mouse wheel
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
      .tool = &OnTool, .force = &OnForce, .tilt = &OnTilt};
  zcr_pointer_stylus_v2_add_listener(zcr_pointer_stylus_v2_.get(),
                                     &kPointerStylusV2Listener, this);
}

// static
void WaylandPointer::OnTool(void* data,
                            struct zcr_pointer_stylus_v2* stylus,
                            uint32_t wl_pointer_type) {
  auto* self = static_cast<WaylandPointer*>(data);

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

  self->delegate_->OnPointerStylusToolChanged(pointer_type);
}

// static
void WaylandPointer::OnForce(void* data,
                             struct zcr_pointer_stylus_v2* stylus,
                             uint32_t time,
                             wl_fixed_t force) {
  auto* self = static_cast<WaylandPointer*>(data);
  DCHECK(self);

  self->delegate_->OnPointerStylusForceChanged(wl_fixed_to_double(force));
}

// static
void WaylandPointer::OnTilt(void* data,
                            struct zcr_pointer_stylus_v2* stylus,
                            uint32_t time,
                            wl_fixed_t tilt_x,
                            wl_fixed_t tilt_y) {
  auto* self = static_cast<WaylandPointer*>(data);
  DCHECK(self);

  self->delegate_->OnPointerStylusTiltChanged(
      gfx::Vector2dF(wl_fixed_to_double(tilt_x), wl_fixed_to_double(tilt_y)));
}

// Enter/Leave events cause undesirable tab detaches in window dragging
// sessions. At least KWin and Mutter are known to send leave/enter events
// before the events currently used by the window drag controller to detect
// drop, see the crbug linked below for more details.
//
// TODO(crbug.com/329479345): Move this suppression logic to drag controller
// code once they're refactored to intercept events for the whole session. Also,
// limit it to apply only in between the first data_device.enter and
// dnd_drop_performed.
bool WaylandPointer::SuppressFocusChangeEvents() const {
  // Compositor version is available only on Exo, via aura-shell protocol.
  if (connection_->GetServerVersion().IsValid() &&
      connection_->GetServerVersion() > base::Version("112.0.5615")) {
    return false;
  }
  return connection_->window_drag_controller() &&
         connection_->window_drag_controller()->IsDragInProgress();
}

}  // namespace ui
