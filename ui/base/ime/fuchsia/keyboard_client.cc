// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/fuchsia/keyboard_client.h"
#include <memory>

#include "base/logging.h"
#include "base/notreached.h"
#include "ui/events/fuchsia/input_event_sink.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace ui {

namespace {

// Converts the state of modifiers managed by Fuchsia (e.g. Caps and Num Lock)
// into ui::Event flags.
int ModifiersToEventFlags(const fuchsia::ui::input3::Modifiers& modifiers) {
  int event_flags = 0;
  if ((modifiers & fuchsia::ui::input3::Modifiers::CAPS_LOCK) ==
      fuchsia::ui::input3::Modifiers::CAPS_LOCK) {
    event_flags |= EF_CAPS_LOCK_ON;
  }
  if ((modifiers & fuchsia::ui::input3::Modifiers::NUM_LOCK) ==
      fuchsia::ui::input3::Modifiers::NUM_LOCK) {
    event_flags |= EF_NUM_LOCK_ON;
  }
  if ((modifiers & fuchsia::ui::input3::Modifiers::SCROLL_LOCK) ==
      fuchsia::ui::input3::Modifiers::SCROLL_LOCK) {
    event_flags |= EF_SCROLL_LOCK_ON;
  }
  return event_flags;
}

}  // namespace

KeyboardClient::KeyboardClient(fuchsia::ui::input3::Keyboard* keyboard_service,
                               fuchsia::ui::views::ViewRef view_ref,
                               InputEventSink* event_sink)
    : binding_(this), event_sink_(event_sink) {
  DCHECK(event_sink_);

  // Connect to the Keyboard service and register |keyboard_client_| as a
  // listener.
  fidl::InterfaceHandle<fuchsia::ui::input3::KeyboardListener>
      keyboard_listener;
  fidl::InterfaceRequest<fuchsia::ui::input3::KeyboardListener>
      keyboard_listener_request = keyboard_listener.NewRequest();
  keyboard_service->AddListener(std::move(view_ref),
                                std::move(keyboard_listener), [] {});
  binding_.Bind(std::move(keyboard_listener_request));
}

KeyboardClient::~KeyboardClient() = default;

void KeyboardClient::OnKeyEvent(
    fuchsia::ui::input3::KeyEvent key_event,
    fuchsia::ui::input3::KeyboardListener::OnKeyEventCallback callback) {
  if (ProcessKeyEvent(key_event)) {
    callback(fuchsia::ui::input3::KeyEventStatus::HANDLED);
  } else {
    callback(fuchsia::ui::input3::KeyEventStatus::NOT_HANDLED);
  }
}

bool KeyboardClient::ProcessKeyEvent(
    const fuchsia::ui::input3::KeyEvent& key_event) {
  if (!key_event.has_type() || !key_event.has_key() ||
      !key_event.has_timestamp()) {
    LOG(ERROR) << "Could not process incomplete input3::KeyEvent.";
    return false;
  }

  // Update activation flags of modifier keys (SHIFT, ALT, etc). This needs to
  // be done for all key event types.
  UpdatedCachedModifiers(key_event);

  EventType event_type;
  switch (key_event.type()) {
    case fuchsia::ui::input3::KeyEventType::PRESSED:
      event_type = ET_KEY_PRESSED;
      break;
    case fuchsia::ui::input3::KeyEventType::RELEASED:
      event_type = ET_KEY_RELEASED;
      break;
    case fuchsia::ui::input3::KeyEventType::SYNC:
    case fuchsia::ui::input3::KeyEventType::CANCEL:
      // SYNC and CANCEL should not generate ui::Events.
      return true;
    default:
      NOTIMPLEMENTED() << "Unknown KeyEventType received: "
                       << static_cast<int>(event_type);
      return false;
  }

  // Convert |key_event| to a ui::KeyEvent.
  DomCode dom_code =
      KeycodeConverter::UsbKeycodeToDomCode(static_cast<int>(key_event.key()));
  int event_flags = EventFlagsForCachedModifiers();
  if (key_event.has_modifiers())
    event_flags |= ModifiersToEventFlags(key_event.modifiers());

  // TODO(https://crbug.com/1187257): Use input3.KeyMeaning instead of US layout
  // as the default.
  DomKey dom_key;
  KeyboardCode key_code;
  if (!DomCodeToUsLayoutDomKey(dom_code, event_flags, &dom_key, &key_code)) {
    LOG(ERROR) << "DomCodeToUsLayoutDomKey() failed for key: "
               << static_cast<uint32_t>(key_event.key());
  }

  ui::KeyEvent ui_key_event(event_type, key_code, dom_code, event_flags,
                            dom_key,
                            base::TimeTicks::FromZxTime(key_event.timestamp()));
  event_sink_->DispatchEvent(&ui_key_event);
  return ui_key_event.handled();
}

// TODO(https://crbug.com/850697): Add additional modifiers as they become
// supported.
void KeyboardClient::UpdatedCachedModifiers(
    const fuchsia::ui::input3::KeyEvent& key_event) {
  // A SYNC event indicates that the key was pressed while the view gained input
  // focus. A CANCEL event indicates the key was held when the view lost input
  // focus. In both cases, the state of locally tracked modifiers should be
  // updated.
  bool modifier_active =
      key_event.type() == fuchsia::ui::input3::KeyEventType::PRESSED ||
      key_event.type() == fuchsia::ui::input3::KeyEventType::SYNC;
  switch (key_event.key()) {
    case fuchsia::input::Key::LEFT_SHIFT:
      left_shift_ = modifier_active;
      break;
    case fuchsia::input::Key::RIGHT_SHIFT:
      right_shift_ = modifier_active;
      break;
    case fuchsia::input::Key::LEFT_ALT:
      left_alt_ = modifier_active;
      break;
    case fuchsia::input::Key::RIGHT_ALT:
      right_alt_ = modifier_active;
      break;
    case fuchsia::input::Key::LEFT_CTRL:
      left_ctrl_ = modifier_active;
      break;
    case fuchsia::input::Key::RIGHT_CTRL:
      right_ctrl_ = modifier_active;
      break;
    default:
      break;
  }
}

int KeyboardClient::EventFlagsForCachedModifiers() {
  int event_flags = 0;
  if (left_shift_ || right_shift_)
    event_flags |= EF_SHIFT_DOWN;
  if (left_alt_ || right_alt_)
    event_flags |= EF_ALT_DOWN;
  if (left_ctrl_ || right_ctrl_)
    event_flags |= EF_CONTROL_DOWN;
  return event_flags;
}

}  // namespace ui
