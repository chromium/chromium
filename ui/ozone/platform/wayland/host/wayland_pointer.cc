// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_pointer.h"

#include <linux/input.h>

#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

// TODO(forney): Handle version 5 of wl_pointer.

namespace ui {

WaylandPointer::WaylandPointer(wl_pointer* pointer,
                               WaylandConnection* connection,
                               Delegate* delegate)
    : obj_(pointer), connection_(connection), delegate_(delegate) {
  static const wl_pointer_listener listener = {
      &WaylandPointer::Enter,  &WaylandPointer::Leave, &WaylandPointer::Motion,
      &WaylandPointer::Button, &WaylandPointer::Axis,
  };

  DCHECK(delegate_);
  delegate_->OnPointerCreated(this);

  wl_pointer_add_listener(obj_.get(), &listener, this);
}

WaylandPointer::~WaylandPointer() {
  delegate_->OnPointerDestroyed(this);
}

// static
void WaylandPointer::Enter(void* data,
                           wl_pointer* obj,
                           uint32_t serial,
                           wl_surface* surface,
                           wl_fixed_t surface_x,
                           wl_fixed_t surface_y) {
  DCHECK(data);
  WaylandPointer* pointer = static_cast<WaylandPointer*>(data);
  WaylandWindow* window = wl::RootWindowFromWlSurface(surface);
  gfx::PointF location{wl_fixed_to_double(surface_x),
                       wl_fixed_to_double(surface_y)};
  pointer->delegate_->OnPointerFocusChanged(window, location);
}

// static
void WaylandPointer::Leave(void* data,
                           wl_pointer* obj,
                           uint32_t serial,
                           wl_surface* surface) {
  DCHECK(data);
  WaylandPointer* pointer = static_cast<WaylandPointer*>(data);
  pointer->delegate_->OnPointerFocusChanged(nullptr, {});
}

// static
void WaylandPointer::Motion(void* data,
                            wl_pointer* obj,
                            uint32_t time,
                            wl_fixed_t surface_x,
                            wl_fixed_t surface_y) {
  WaylandPointer* pointer = static_cast<WaylandPointer*>(data);
  gfx::PointF location(wl_fixed_to_double(surface_x),
                       wl_fixed_to_double(surface_y));
  pointer->delegate_->OnPointerMotionEvent(location);
}

// static
void WaylandPointer::Button(void* data,
                            wl_pointer* obj,
                            uint32_t serial,
                            uint32_t time,
                            uint32_t button,
                            uint32_t state) {
  WaylandPointer* pointer = static_cast<WaylandPointer*>(data);
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

  // Set serial only on button presses. Popup windows can be created on
  // button/touch presses, and, thus, require the serial of the last serial when
  // the button was pressed. Otherwise, Wayland server dismisses the popup
  // requests (see the protocol definition).
  if (state == WL_POINTER_BUTTON_STATE_PRESSED)
    pointer->connection_->set_serial(serial);
  EventType type = state == WL_POINTER_BUTTON_STATE_PRESSED ? ET_MOUSE_PRESSED
                                                            : ET_MOUSE_RELEASED;
  pointer->delegate_->OnPointerButtonEvent(type, changed_button);
}

// static
void WaylandPointer::Axis(void* data,
                          wl_pointer* obj,
                          uint32_t time,
                          uint32_t axis,
                          wl_fixed_t value) {
  static const double kAxisValueScale = 10.0;
  WaylandPointer* pointer = static_cast<WaylandPointer*>(data);
  gfx::Vector2d offset;
  // Wayland compositors send axis events with values in the surface coordinate
  // space. They send a value of 10 per mouse wheel click by convention, so
  // clients (e.g. GTK+) typically scale down by this amount to convert to
  // discrete step coordinates. wl_pointer version 5 improves the situation by
  // adding axis sources and discrete axis events.
  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
    offset.set_y(-wl_fixed_to_double(value) / kAxisValueScale *
                 MouseWheelEvent::kWheelDelta);
  } else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
    offset.set_x(wl_fixed_to_double(value) / kAxisValueScale *
                 MouseWheelEvent::kWheelDelta);
  } else {
    return;
  }
  pointer->delegate_->OnPointerAxisEvent(offset);
}

}  // namespace ui
