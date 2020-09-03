// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_injector_evdev.h"

#include <utility>

#include "base/logging.h"
#include "ui/events/event.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"
#include "ui/events/ozone/evdev/keyboard_util_evdev.h"

namespace ui {

namespace {

const int kDeviceIdForInjection = -1;

}  // namespace

InputInjectorEvdev::InputInjectorEvdev(
    std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher,
    CursorDelegateEvdev* cursor)
    : cursor_(cursor), dispatcher_(std::move(dispatcher)) {}

InputInjectorEvdev::~InputInjectorEvdev() {}

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
    default:
      LOG(WARNING) << "Invalid flag: " << button << " for the button parameter";
      return;
  }

  dispatcher_->DispatchMouseButtonEvent(MouseButtonEventParams(
      kDeviceIdForInjection, EF_NONE, cursor_->GetLocation(), code, down,
      false /* allow_remap */, PointerDetails(EventPointerType::kMouse),
      EventTimeForNow()));
}

void InputInjectorEvdev::InjectMouseWheel(int delta_x, int delta_y) {
  dispatcher_->DispatchMouseWheelEvent(MouseWheelEventParams(
      kDeviceIdForInjection, cursor_->GetLocation(),
      gfx::Vector2d(delta_x, delta_y), EventTimeForNow()));
}

void InputInjectorEvdev::MoveCursorTo(const gfx::PointF& location) {
  if (!cursor_)
    return;

  cursor_->MoveCursorTo(location);

  dispatcher_->DispatchMouseMoveEvent(MouseMoveEventParams(
      kDeviceIdForInjection, EF_NONE, cursor_->GetLocation(),
      nullptr /* ordinal_delta */, PointerDetails(EventPointerType::kMouse),
      EventTimeForNow()));
}

void InputInjectorEvdev::InjectKeyEvent(DomCode physical_key,
                                        bool down,
                                        bool suppress_auto_repeat) {
  if (physical_key == DomCode::NONE)
    return;

  int native_keycode = KeycodeConverter::DomCodeToNativeKeycode(physical_key);
  int evdev_code = NativeCodeToEvdevCode(native_keycode);

  dispatcher_->DispatchKeyEvent(KeyEventParams(
      kDeviceIdForInjection, ui::EF_NONE, evdev_code, 0 /*scan_code*/, down,
      suppress_auto_repeat, EventTimeForNow()));
}

}  // namespace ui
