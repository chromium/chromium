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
      // TODO(http://fxbug.dev/69620): Add support for SYNC and CANCEL.
      return false;
    default:
      NOTIMPLEMENTED() << "Unknown KeyEventType received: "
                       << static_cast<int>(event_type);
      return false;
  }

  // Update activation flags of modifier keys (SHIFT, ALT, etc).
  UpdateModifiers(key_event);

  if (key_event.type() == fuchsia::ui::input3::KeyEventType::RELEASED)
    return true;

  // Convert |key_event| to a ui::KeyEvent.
  DomCode dom_code =
      KeycodeConverter::UsbKeycodeToDomCode(static_cast<int>(key_event.key()));
  DomKey dom_key;
  KeyboardCode key_code;
  int flags =
      key_event.has_modifiers() ? ComputeFlagValue(key_event.modifiers()) : 0;

  // TODO(https://crbug.com/1187257): Use input3.KeyMeaning instead of US layout
  // as the default.
  if (!DomCodeToUsLayoutDomKey(dom_code, flags, &dom_key, &key_code)) {
    LOG(ERROR) << "DomCodeToUsLayoutDomKey() failed for key: "
               << static_cast<int>(key_event.key());
  }

  ui::KeyEvent ui_key_event(event_type, key_code, dom_code, flags, dom_key,
                            base::TimeTicks::FromZxTime(key_event.timestamp()));
  event_sink_->DispatchEvent(&ui_key_event);
  return ui_key_event.handled();
}

// TODO(https://crbug.com/850697): Add additional modifiers as they become
// supported.
void KeyboardClient::UpdateModifiers(
    const fuchsia::ui::input3::KeyEvent& key_event) {
  if (key_event.type() == fuchsia::ui::input3::KeyEventType::PRESSED ||
      key_event.type() == fuchsia::ui::input3::KeyEventType::RELEASED) {
    bool modifier_active =
        key_event.type() == fuchsia::ui::input3::KeyEventType::PRESSED;
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
}

int KeyboardClient::ComputeFlagValue(fuchsia::ui::input3::Modifiers modifiers) {
  int flags = 0;
  if ((modifiers & fuchsia::ui::input3::Modifiers::CAPS_LOCK) ==
      fuchsia::ui::input3::Modifiers::CAPS_LOCK) {
    flags |= EF_CAPS_LOCK_ON;
  }
  if ((modifiers & fuchsia::ui::input3::Modifiers::NUM_LOCK) ==
      fuchsia::ui::input3::Modifiers::NUM_LOCK) {
    flags |= EF_NUM_LOCK_ON;
  }
  if ((modifiers & fuchsia::ui::input3::Modifiers::SCROLL_LOCK) ==
      fuchsia::ui::input3::Modifiers::SCROLL_LOCK) {
    flags |= EF_SCROLL_LOCK_ON;
  }
  if (left_shift_ || right_shift_)
    flags |= EF_SHIFT_DOWN;
  if (left_alt_ || right_alt_)
    flags |= EF_ALT_DOWN;
  if (left_ctrl_ || right_ctrl_)
    flags |= EF_CONTROL_DOWN;

  return flags;
}

}  // namespace ui
