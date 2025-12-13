// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_tablet_tool.h"

#include <cursor-shape-v1-client-protocol.h>
#include <linux/input-event-codes.h>
#include <tablet-unstable-v2-client-protocol.h>

#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_shape.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_tablet_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

namespace {

EventPointerType ToolToPointerType(uint32_t tool_type) {
  switch (tool_type) {
    case ZWP_TABLET_TOOL_V2_TYPE_PEN:
      return EventPointerType::kPen;
    case ZWP_TABLET_TOOL_V2_TYPE_ERASER:
      return EventPointerType::kEraser;
    case ZWP_TABLET_TOOL_V2_TYPE_BRUSH:
    case ZWP_TABLET_TOOL_V2_TYPE_PENCIL:
    case ZWP_TABLET_TOOL_V2_TYPE_AIRBRUSH:
      return EventPointerType::kPen;
    case ZWP_TABLET_TOOL_V2_TYPE_MOUSE:
      return EventPointerType::kMouse;
    case ZWP_TABLET_TOOL_V2_TYPE_LENS:
      return EventPointerType::kMouse;
    case ZWP_TABLET_TOOL_V2_TYPE_FINGER:
      return EventPointerType::kTouch;
    default:
      return EventPointerType::kUnknown;
  }
}

EventFlags ButtonToEventFlags(uint32_t button) {
  switch (button) {
    case BTN_TOUCH:
      return EF_LEFT_MOUSE_BUTTON;
    case BTN_STYLUS:
      return EF_RIGHT_MOUSE_BUTTON;
    case BTN_STYLUS2:
      return EF_MIDDLE_MOUSE_BUTTON;
    default:
      return 0;
  }
}

wl::EventDispatchPolicy GetEventDispatchPolicy() {
  return IsDispatchPointerEventsOnFrameEventEnabled()
             ? wl::EventDispatchPolicy::kOnFrame
             : wl::EventDispatchPolicy::kImmediate;
}

}  // namespace

WaylandTabletTool::FrameData::FrameData() = default;

WaylandTabletTool::FrameData::~FrameData() = default;

WaylandTabletTool::WaylandTabletTool(zwp_tablet_tool_v2* tool,
                                     WaylandTabletSeat* seat,
                                     WaylandConnection* connection,
                                     Delegate* delegate,
                                     WaylandPointer::Delegate* pointer_delegate)
    : connection_(connection),
      seat_(seat),
      delegate_(delegate),
      pointer_delegate_(pointer_delegate),
      tool_(tool) {
  static constexpr zwp_tablet_tool_v2_listener kListener = {
      .type = &Type,
      .hardware_serial = &HardwareSerial,
      .hardware_id_wacom = &HardwareIdWacom,
      .capability = &Capability,
      .done = &Done,
      .removed = &Removed,
      .proximity_in = &ProximityIn,
      .proximity_out = &ProximityOut,
      .down = &Down,
      .up = &Up,
      .motion = &Motion,
      .pressure = &Pressure,
      .distance = &Distance,
      .tilt = &Tilt,
      .rotation = &Rotation,
      .slider = &Slider,
      .wheel = &Wheel,
      .button = &Button,
      .frame = &Frame,
  };
  zwp_tablet_tool_v2_add_listener(tool_.get(), &kListener, this);
}

WaylandTabletTool::~WaylandTabletTool() = default;

void WaylandTabletTool::DispatchBufferedEvents() {
  if (frame_data_.proximity_in && frame_data_.proximity_target.get()) {
    delegate_->OnTabletToolProximityIn(
        frame_data_.proximity_target.get(),
        frame_data_.location.value_or(gfx::PointF()),
        frame_data_.pointer_details, frame_data_.timestamp);
  }

  if (frame_data_.down) {
    delegate_->OnTabletToolButton(EF_LEFT_MOUSE_BUTTON, true,
                                  frame_data_.pointer_details,
                                  frame_data_.timestamp);
  }
  if (frame_data_.up) {
    delegate_->OnTabletToolButton(EF_LEFT_MOUSE_BUTTON, false,
                                  frame_data_.pointer_details,
                                  frame_data_.timestamp);
  }

  for (const auto& button_state : frame_data_.button_states) {
    delegate_->OnTabletToolButton(button_state.first, button_state.second,
                                  frame_data_.pointer_details,
                                  frame_data_.timestamp);
  }

  if (frame_data_.location) {
    delegate_->OnTabletToolMotion(*frame_data_.location,
                                  frame_data_.pointer_details,
                                  frame_data_.timestamp);
  }

  if (frame_data_.proximity_out) {
    delegate_->OnTabletToolProximityOut(frame_data_.timestamp);
  }
}

void WaylandTabletTool::ResetFrameData() {
  frame_data_ = FrameData();
  frame_data_.pointer_details.pointer_type = pointer_type_;
}

// static
void WaylandTabletTool::Type(void* data,
                             zwp_tablet_tool_v2* tool,
                             uint32_t tool_type) {
  auto* self = static_cast<WaylandTabletTool*>(data);
  self->pointer_type_ = ToolToPointerType(tool_type);
  self->frame_data_.pointer_details.pointer_type = self->pointer_type_;
}

// static
void WaylandTabletTool::HardwareSerial(void* data,
                                       zwp_tablet_tool_v2* tool,
                                       uint32_t serial_hi,
                                       uint32_t serial_lo) {
  // Not used.
}

// static
void WaylandTabletTool::HardwareIdWacom(void* data,
                                        zwp_tablet_tool_v2* tool,
                                        uint32_t id_hi,
                                        uint32_t id_lo) {
  // Not used.
}

// static
void WaylandTabletTool::Capability(void* data,
                                   zwp_tablet_tool_v2* tool,
                                   uint32_t capability) {
  // We handle all capabilities by default.
}

// static
void WaylandTabletTool::Done(void* data, zwp_tablet_tool_v2* tool) {
  // All static info received.
}

// static
void WaylandTabletTool::Removed(void* data, zwp_tablet_tool_v2* tool) {
  auto* self = static_cast<WaylandTabletTool*>(data);
  self->seat_->OnToolRemoved(self);
}

// static
void WaylandTabletTool::ProximityIn(void* data,
                                    zwp_tablet_tool_v2* tool,
                                    uint32_t serial,
                                    zwp_tablet_v2* tablet,
                                    wl_surface* surface) {
  auto* self = static_cast<WaylandTabletTool*>(data);
  if (!self->cursor_shape_device_) {
    if (auto* shape = self->connection_->wayland_cursor_shape()) {
      self->cursor_shape_device_ =
          shape->CreateTabletToolShapeDevice(self->tool_.get());
    }
  }
  if (self->cursor_shape_device_) {
    wp_cursor_shape_device_v1_set_shape(
        self->cursor_shape_device_.get(), serial,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
  }

  if (!surface) {
    return;
  }
  WaylandWindow* window = wl::RootWindowFromWlSurface(surface);
  if (!window) {
    return;
  }

  self->connection_->serial_tracker().UpdateSerial(wl::SerialType::kMousePress,
                                                   serial);
  self->frame_data_.proximity_in = true;
  self->frame_data_.proximity_out = false;
  self->frame_data_.proximity_target = window->AsWeakPtr();
  self->frame_data_.proximity_serial = serial;

  self->pointer_delegate_->OnPointerFocusChanged(
      window, self->pointer_delegate_->GetPointerLocation(), EventTimeForNow(),
      GetEventDispatchPolicy());
}

// static
void WaylandTabletTool::ProximityOut(void* data, zwp_tablet_tool_v2* tool) {
  auto* self = static_cast<WaylandTabletTool*>(data);
  self->frame_data_.proximity_out = true;
  self->frame_data_.proximity_in = false;

  self->pointer_delegate_->OnPointerFocusChanged(
      nullptr, self->pointer_delegate_->GetPointerLocation(), EventTimeForNow(),
      GetEventDispatchPolicy());
}

// static
void WaylandTabletTool::Down(void* data,
                             zwp_tablet_tool_v2* tool,
                             uint32_t serial) {
  auto* self = static_cast<WaylandTabletTool*>(data);
  self->connection_->serial_tracker().UpdateSerial(wl::SerialType::kMousePress,
                                                   serial);
  self->frame_data_.down = true;
  self->frame_data_.up = false;
}

// static
void WaylandTabletTool::Up(void* data, zwp_tablet_tool_v2* tool) {
  auto* self = static_cast<WaylandTabletTool*>(data);
  self->frame_data_.up = true;
  self->frame_data_.down = false;
}

// static
void WaylandTabletTool::Motion(void* data,
                               zwp_tablet_tool_v2* tool,
                               wl_fixed_t x,
                               wl_fixed_t y) {
  auto* self = static_cast<WaylandTabletTool*>(data);
  self->frame_data_.location =
      gfx::PointF(wl_fixed_to_double(x), wl_fixed_to_double(y));
}

// static
void WaylandTabletTool::Pressure(void* data,
                                 zwp_tablet_tool_v2* tool,
                                 uint32_t pressure) {
  auto* self = static_cast<WaylandTabletTool*>(data);
  self->frame_data_.pointer_details.force =
      static_cast<float>(pressure) / 65535.f;
}

// static
void WaylandTabletTool::Distance(void* data,
                                 zwp_tablet_tool_v2* tool,
                                 uint32_t distance) {
  // Distance is not currently part of PointerDetails.
}

// static
void WaylandTabletTool::Tilt(void* data,
                             zwp_tablet_tool_v2* tool,
                             wl_fixed_t tilt_x,
                             wl_fixed_t tilt_y) {
  auto* self = static_cast<WaylandTabletTool*>(data);
  self->frame_data_.pointer_details.tilt_x = wl_fixed_to_double(tilt_x);
  self->frame_data_.pointer_details.tilt_y = wl_fixed_to_double(tilt_y);
}

// static
void WaylandTabletTool::Rotation(void* data,
                                 zwp_tablet_tool_v2* tool,
                                 wl_fixed_t degrees) {
  auto* self = static_cast<WaylandTabletTool*>(data);
  self->frame_data_.pointer_details.twist = wl_fixed_to_double(degrees);
}

// static
void WaylandTabletTool::Slider(void* data,
                               zwp_tablet_tool_v2* tool,
                               int32_t position) {
  auto* self = static_cast<WaylandTabletTool*>(data);
  // Tangential pressure is in [-1, 1].
  self->frame_data_.pointer_details.tangential_pressure =
      static_cast<float>(position) / 32767.f;
}

// static
void WaylandTabletTool::Wheel(void* data,
                              zwp_tablet_tool_v2* tool,
                              wl_fixed_t degrees,
                              int32_t clicks) {
  // Not implemented, wheel on tablet tool is rare.
}

// static
void WaylandTabletTool::Button(void* data,
                               zwp_tablet_tool_v2* tool,
                               uint32_t serial,
                               uint32_t button,
                               uint32_t state) {
  auto* self = static_cast<WaylandTabletTool*>(data);
  if (int changed_button = ButtonToEventFlags(button)) {
    self->connection_->serial_tracker().UpdateSerial(
        wl::SerialType::kMousePress, serial);
    self->frame_data_.button_states.emplace_back(
        changed_button, state == ZWP_TABLET_TOOL_V2_BUTTON_STATE_PRESSED);
  }
}

// static
void WaylandTabletTool::Frame(void* data,
                              zwp_tablet_tool_v2* tool,
                              uint32_t time) {
  auto* self = static_cast<WaylandTabletTool*>(data);
  self->frame_data_.pointer_details.pointer_type = self->pointer_type_;
  self->frame_data_.timestamp = wl::EventMillisecondsToTimeTicks(time);
  self->DispatchBufferedEvents();
  self->ResetFrameData();
}

}  // namespace ui
