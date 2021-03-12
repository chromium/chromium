// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/fuchsia/input_event_dispatcher.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "ui/events/event.h"
#include "ui/events/fuchsia/input_event_sink.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace ui {
namespace {

int KeyModifiersToFlags(int modifiers) {
  int flags = 0;
  if (modifiers & fuchsia::ui::input::kModifierShift)
    flags |= EF_SHIFT_DOWN;
  if (modifiers & fuchsia::ui::input::kModifierControl)
    flags |= EF_CONTROL_DOWN;
  if (modifiers & fuchsia::ui::input::kModifierAlt)
    flags |= EF_ALT_DOWN;
  // TODO(crbug.com/850697): Add AltGraph support.
  return flags;
}

}  // namespace

InputEventDispatcher::InputEventDispatcher(InputEventSink* event_sink)
    : event_sink_(event_sink) {
  DCHECK(event_sink_);
}

InputEventDispatcher::~InputEventDispatcher() = default;

bool InputEventDispatcher::ProcessEvent(
    const fuchsia::ui::input::InputEvent& event) const {
  switch (event.Which()) {
    case fuchsia::ui::input::InputEvent::Tag::kPointer:
      switch (event.pointer().type) {
        case fuchsia::ui::input::PointerEventType::MOUSE:
          return ProcessMouseEvent(event.pointer());
        case fuchsia::ui::input::PointerEventType::TOUCH:
          return ProcessTouchEvent(event.pointer());
        case fuchsia::ui::input::PointerEventType::STYLUS:
        case fuchsia::ui::input::PointerEventType::INVERTED_STYLUS:
          NOTIMPLEMENTED() << "Stylus input is not yet supported.";
          return false;
      }

    case fuchsia::ui::input::InputEvent::Tag::kKeyboard:
      return ProcessKeyboardEvent(event.keyboard());

    case fuchsia::ui::input::InputEvent::Tag::kFocus:
    case fuchsia::ui::input::InputEvent::Tag::Invalid:
      return false;
  }
}

bool InputEventDispatcher::ProcessMouseEvent(
    const fuchsia::ui::input::PointerEvent& event) const {
  int flags = 0;
  EventType event_type;
  int buttons_flags = 0;
  if (event.buttons & fuchsia::ui::input::kMouseButtonPrimary) {
    buttons_flags |= EF_LEFT_MOUSE_BUTTON;
  }
  if (event.buttons & fuchsia::ui::input::kMouseButtonSecondary) {
    buttons_flags |= EF_RIGHT_MOUSE_BUTTON;
  }
  if (event.buttons & fuchsia::ui::input::kMouseButtonTertiary) {
    buttons_flags |= EF_MIDDLE_MOUSE_BUTTON;
  }

  switch (event.phase) {
    case fuchsia::ui::input::PointerEventPhase::DOWN:
      event_type = ET_MOUSE_PRESSED;
      break;
    case fuchsia::ui::input::PointerEventPhase::MOVE:
      event_type = flags ? ET_MOUSE_DRAGGED : ET_MOUSE_MOVED;
      break;
    case fuchsia::ui::input::PointerEventPhase::UP:
      event_type = ET_MOUSE_RELEASED;
      break;

    // Following phases are not expected for mouse events.
    case fuchsia::ui::input::PointerEventPhase::HOVER:
    case fuchsia::ui::input::PointerEventPhase::CANCEL:
    case fuchsia::ui::input::PointerEventPhase::ADD:
    case fuchsia::ui::input::PointerEventPhase::REMOVE:
      NOTREACHED() << "Unexpected mouse phase "
                   << fidl::ToUnderlying(event.phase);
      return false;
  }

  ui::MouseEvent mouse_event(event_type, gfx::Point(), gfx::Point(),
                             base::TimeTicks::FromZxTime(event.event_time),
                             buttons_flags, buttons_flags);
  mouse_event.set_location_f(gfx::PointF(event.x, event.y));
  event_sink_->DispatchEvent(&mouse_event);
  return mouse_event.handled();
}

bool InputEventDispatcher::ProcessTouchEvent(
    const fuchsia::ui::input::PointerEvent& event) const {
  EventType event_type;
  bool hovering = false;
  switch (event.phase) {
    case fuchsia::ui::input::PointerEventPhase::HOVER:
      hovering = true;
      event_type = ET_TOUCH_PRESSED;
      break;
    case fuchsia::ui::input::PointerEventPhase::DOWN:
      event_type = ET_TOUCH_PRESSED;
      break;
    case fuchsia::ui::input::PointerEventPhase::MOVE:
      event_type = ET_TOUCH_MOVED;
      break;
    case fuchsia::ui::input::PointerEventPhase::CANCEL:
      event_type = ET_TOUCH_CANCELLED;
      break;
    case fuchsia::ui::input::PointerEventPhase::UP:
      event_type = ET_TOUCH_RELEASED;
      break;
    case fuchsia::ui::input::PointerEventPhase::ADD:
    case fuchsia::ui::input::PointerEventPhase::REMOVE:
      return false;
  }

  // TODO(crbug.com/876933): Add more detailed fields such as
  // force/orientation/tilt once they are added to PointerEvent.
  ui::PointerDetails pointer_details(ui::EventPointerType::kTouch,
                                     event.pointer_id);

  ui::TouchEvent touch_event(event_type, gfx::Point(),
                             base::TimeTicks::FromZxTime(event.event_time),
                             pointer_details);
  touch_event.set_hovering(hovering);
  touch_event.set_location_f(gfx::PointF(event.x, event.y));
  event_sink_->DispatchEvent(&touch_event);
  return touch_event.handled();
}

bool InputEventDispatcher::ProcessKeyboardEvent(
    const fuchsia::ui::input::KeyboardEvent& event) const {
  EventType event_type;

  switch (event.phase) {
    case fuchsia::ui::input::KeyboardEventPhase::PRESSED:
    case fuchsia::ui::input::KeyboardEventPhase::REPEAT:
      event_type = ET_KEY_PRESSED;
      break;

    case fuchsia::ui::input::KeyboardEventPhase::RELEASED:
      event_type = ET_KEY_RELEASED;
      break;

    case fuchsia::ui::input::KeyboardEventPhase::CANCELLED:
      NOTIMPLEMENTED() << "Key event cancellation is not supported.";
      event_type = ET_KEY_RELEASED;
      break;
  }

  DomCode dom_code = KeycodeConverter::NativeKeycodeToDomCode(event.hid_usage);
  DomKey dom_key;
  KeyboardCode key_code;
  if (!DomCodeToUsLayoutDomKey(dom_code, KeyModifiersToFlags(event.modifiers),
                               &dom_key, &key_code)) {
    LOG(ERROR) << "DomCodeToUsLayoutDomKey() failed for usb_key: "
               << event.hid_usage;
    key_code = VKEY_UNKNOWN;
  }

  if (event.code_point)
    dom_key = DomKey::FromCharacter(event.code_point);

  ui::KeyEvent key_event(event_type, key_code, dom_code,
                         KeyModifiersToFlags(event.modifiers), dom_key,
                         base::TimeTicks::FromZxTime(event.event_time));
  event_sink_->DispatchEvent(&key_event);
  return key_event.handled();
}

}  // namespace ui
