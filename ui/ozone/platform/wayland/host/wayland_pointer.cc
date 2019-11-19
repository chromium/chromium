// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_pointer.h"

#include <linux/input.h>
#include <wayland-client.h>
#include <memory>

#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

// TODO(forney): Handle version 5 of wl_pointer.

namespace ui {

namespace {

bool VerifyFlagsAfterMasking(int flags, int original_flags, int modifiers) {
  flags &= ~modifiers;
  return flags == original_flags;
}

bool HasAnyButtonFlag(int flags) {
  return (flags & (EF_LEFT_MOUSE_BUTTON | EF_MIDDLE_MOUSE_BUTTON |
                   EF_RIGHT_MOUSE_BUTTON | EF_BACK_MOUSE_BUTTON |
                   EF_FORWARD_MOUSE_BUTTON)) != 0;
}

}  // namespace

WaylandPointer::WaylandPointer(wl_pointer* pointer,
                               const EventDispatchCallback& callback)
    : obj_(pointer), callback_(callback), weak_ptr_factory_(this) {
  static const wl_pointer_listener listener = {
      &WaylandPointer::Enter,  &WaylandPointer::Leave, &WaylandPointer::Motion,
      &WaylandPointer::Button, &WaylandPointer::Axis,
  };

  wl_pointer_add_listener(obj_.get(), &listener, this);

  cursor_ = std::make_unique<WaylandCursor>();
}

WaylandPointer::~WaylandPointer() {
  if (window_with_pointer_focus_) {
    window_with_pointer_focus_->set_pointer_focus(false);
    window_with_pointer_focus_->set_has_implicit_grab(false);
  }
}

// static
void WaylandPointer::Enter(void* data,
                           wl_pointer* obj,
                           uint32_t serial,
                           wl_surface* surface,
                           wl_fixed_t surface_x,
                           wl_fixed_t surface_y) {
  WaylandPointer* pointer = static_cast<WaylandPointer*>(data);
  pointer->location_.SetPoint(wl_fixed_to_double(surface_x),
                              wl_fixed_to_double(surface_y));
  pointer->FocusWindow(surface);
  MouseEvent event(ET_MOUSE_ENTERED, pointer->location_, pointer->location_,
                   EventTimeForNow(), pointer->flags_, 0);
  pointer->callback_.Run(&event);
}

// static
void WaylandPointer::Leave(void* data,
                           wl_pointer* obj,
                           uint32_t serial,
                           wl_surface* surface) {
  WaylandPointer* pointer = static_cast<WaylandPointer*>(data);
  MouseEvent event(ET_MOUSE_EXITED, gfx::Point(), gfx::Point(),
                   EventTimeForNow(), pointer->flags_, 0);
  pointer->callback_.Run(&event);
  pointer->UnfocusWindow(surface);
}

// static
void WaylandPointer::Motion(void* data,
                            wl_pointer* obj,
                            uint32_t time,
                            wl_fixed_t surface_x,
                            wl_fixed_t surface_y) {
  WaylandPointer* pointer = static_cast<WaylandPointer*>(data);
  pointer->location_.SetPoint(wl_fixed_to_double(surface_x),
                              wl_fixed_to_double(surface_y));
  MouseEvent event(ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                   EventTimeForNow(), pointer->GetFlagsWithKeyboardModifiers(),
                   0);
  event.set_location_f(pointer->location_);
  event.set_root_location_f(pointer->location_);
  pointer->callback_.Run(&event);
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
      changed_button = EF_BACK_MOUSE_BUTTON;
      break;
    case BTN_FORWARD:
      changed_button = EF_FORWARD_MOUSE_BUTTON;
      break;
    default:
      return;
  }

  EventType type;
  if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
    type = ET_MOUSE_PRESSED;
    pointer->flags_ |= changed_button;
    pointer->connection_->set_serial(serial);
  } else {
    type = ET_MOUSE_RELEASED;
    pointer->flags_ &= ~changed_button;
  }

  // See comment bellow.
  if (type == ET_MOUSE_PRESSED)
    pointer->MaybeSetOrResetImplicitGrab();

  // MouseEvent's flags should contain the button that was released too.
  const int flags = pointer->GetFlagsWithKeyboardModifiers() | changed_button;
  MouseEvent event(type, gfx::Point(), gfx::Point(), EventTimeForNow(), flags,
                   changed_button);
  event.set_location_f(pointer->location_);
  event.set_root_location_f(pointer->location_);

  auto weak_ptr = pointer->weak_ptr_factory_.GetWeakPtr();
  pointer->callback_.Run(&event);

  // Reset implicit grab only after the event has been sent. Otherwise,
  // we may end up in a situation, when a target checks for a pointer grab on
  // the MouseRelease event type, and fails to release capture due to early
  // pointer focus reset. Setting implicit grab is done normally before the
  // event has been sent.
  if (weak_ptr && type == ET_MOUSE_RELEASED)
    pointer->MaybeSetOrResetImplicitGrab();
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
  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
    offset.set_y(-wl_fixed_to_double(value) / kAxisValueScale *
                 MouseWheelEvent::kWheelDelta);
  else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
    offset.set_x(wl_fixed_to_double(value) / kAxisValueScale *
                 MouseWheelEvent::kWheelDelta);
  else
    return;
  MouseWheelEvent event(offset, gfx::Point(), gfx::Point(), EventTimeForNow(),
                        pointer->GetFlagsWithKeyboardModifiers(), 0);
  event.set_location_f(pointer->location_);
  event.set_root_location_f(pointer->location_);
  pointer->callback_.Run(&event);
}

void WaylandPointer::MaybeSetOrResetImplicitGrab() {
  if (!window_with_pointer_focus_)
    return;

  window_with_pointer_focus_->set_has_implicit_grab(HasAnyButtonFlag(flags_));
}

int WaylandPointer::GetFlagsWithKeyboardModifiers() {
  assert(sizeof(flags_) == sizeof(keyboard_modifiers_));

  // Remove old modifiers from flags and then update them with new modifiers.
  flags_ &= ~keyboard_modifiers_;
  keyboard_modifiers_ = connection_->GetKeyboardModifiers();

  int old_flags = flags_;
  flags_ |= keyboard_modifiers_;
  DCHECK(VerifyFlagsAfterMasking(flags_, old_flags, keyboard_modifiers_));
  return flags_;
}

void WaylandPointer::ResetFlags() {
  flags_ = 0;
  keyboard_modifiers_ = 0;
}

void WaylandPointer::FocusWindow(wl_surface* surface) {
  if (surface) {
    WaylandWindow* window = WaylandWindow::FromSurface(surface);
    window->set_pointer_focus(true);
    window_with_pointer_focus_ = window;
  }
}

void WaylandPointer::UnfocusWindow(wl_surface* surface) {
  if (surface) {
    WaylandWindow* window = WaylandWindow::FromSurface(surface);
    window->set_pointer_focus(false);
    window->set_has_implicit_grab(false);
    window_with_pointer_focus_ = nullptr;
  }
}

}  // namespace ui
