// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_injector_evdev.h"

#include <utility>

#include "base/logging.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"

namespace ui {

InputInjectorEvdev::InputInjectorEvdev(
    std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher,
    CursorDelegateEvdev* cursor)
    : cursor_(cursor), dispatcher_(std::move(dispatcher)) {}

InputInjectorEvdev::~InputInjectorEvdev() = default;

void InputInjectorEvdev::SetDeviceId(int device_id) {
  device_id_ = device_id;
}

void InputInjectorEvdev::InjectMouseButton(EventFlags button, bool down) {
  unsigned int code;
  switch (button) {
    case EF_LEFT_MOUSE_BUTTON:
      code = BTN_LEFT;
      break;
    case EF_RIGHT_MOUSE_BUTTON:
      code = BTN_RIGHT;
      break;
    case EF_MIDDLE_MOUSE_BUTTON:
      code = BTN_MIDDLE;
      break;
    case EF_BACK_MOUSE_BUTTON:
      code = BTN_BACK;
      break;
    case EF_FORWARD_MOUSE_BUTTON:
      code = BTN_FORWARD;
      break;
    default:
      LOG(WARNING) << "Invalid flag: " << button << " for the button parameter";
      return;
  }

  dispatcher_->DispatchMouseButtonEvent(MouseButtonEventParams(
      device_id_, EF_NONE, cursor_->GetLocation(), code, down,
      MouseButtonMapType::kNone, PointerDetails(EventPointerType::kMouse),
      EventTimeForNow()));
}

void InputInjectorEvdev::InjectMouseWheel(int delta_x, int delta_y) {
  dispatcher_->DispatchMouseWheelEvent(MouseWheelEventParams(
      device_id_, cursor_->GetLocation(), gfx::Vector2d(delta_x, delta_y),
      EventTimeForNow()));
}

void InputInjectorEvdev::MoveCursorTo(const gfx::PointF& location) {
  if (!cursor_)
    return;

  cursor_->MoveCursorTo(location);

  // Mouse warping moves the mouse cursor to the adjacent display if the mouse
  // is positioned at the edge of the current display.
  // This is useful/needed for real mouse movements (as without mouse warping
  // the mouse would be stuck on one display).
  // Here we use absolute coordinates though, so mouse warping is not desirable
  // as our coordinates already cover all available displays.
  const int event_flags = EF_NOT_SUITABLE_FOR_MOUSE_WARPING;

  dispatcher_->DispatchMouseMoveEvent(MouseMoveEventParams(
      device_id_, event_flags, cursor_->GetLocation(),
      nullptr /* ordinal_delta */, PointerDetails(EventPointerType::kMouse),
      EventTimeForNow()));
}

void InputInjectorEvdev::InjectKeyEvent(DomCode physical_key,
                                        bool down,
                                        bool suppress_auto_repeat) {
  if (physical_key == DomCode::NONE)
    return;

  int evdev_code = KeycodeConverter::DomCodeToEvdevCode(physical_key);
  dispatcher_->DispatchKeyEvent(
      KeyEventParams(device_id_, EF_NONE, evdev_code, 0 /*scan_code*/, down,
                     suppress_auto_repeat, EventTimeForNow()));
}

}  // namespace ui
