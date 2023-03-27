// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/fuchsia/keyboard_client.h"

#include <limits>
#include <tuple>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/event.h"
#include "ui/events/fuchsia/input_event_sink.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_code_conversion_fuchsia.h"

namespace ui {

namespace {

// Adds `flag` to `event_flags` if `modifier` is present. Also removes handled
// modifiers from `unhandled_modifiers`.
inline void MaybeAddFlag(fuchsia::ui::input3::Modifiers modifier,
                         EventFlags flag,
                         EventFlags& event_flags,
                         fuchsia::ui::input3::Modifiers& unhandled_modifiers) {
  if (unhandled_modifiers & modifier) {
    event_flags |= flag;
    // Remove modifier from unhandled.
    unhandled_modifiers &= ~modifier;
  }
}

// Converts the state of modifiers managed by Fuchsia (e.g. Caps and Num Lock)
// into ui::Event flags.
int ModifiersToEventFlags(fuchsia::ui::input3::Modifiers modifiers) {
  EventFlags event_flags = EF_NONE;
  MaybeAddFlag(fuchsia::ui::input3::Modifiers::CAPS_LOCK, EF_CAPS_LOCK_ON,
               event_flags, modifiers);
  MaybeAddFlag(fuchsia::ui::input3::Modifiers::NUM_LOCK, EF_NUM_LOCK_ON,
               event_flags, modifiers);
  MaybeAddFlag(fuchsia::ui::input3::Modifiers::SCROLL_LOCK, EF_SCROLL_LOCK_ON,
               event_flags, modifiers);

  // This mapping is present in case blink adds support in the future, but blink
  // doesn't currently output the Function modifier. See
  // https://crsrc.org/c/ui/events/blink/blink_event_util.cc;l=268?q=EventFlagsToWebEventModifiers
  MaybeAddFlag(fuchsia::ui::input3::Modifiers::FUNCTION, EF_FUNCTION_DOWN,
               event_flags, modifiers);
  if (modifiers & fuchsia::ui::input3::Modifiers::SYMBOL) {
    // fuchsia::ui::input3::Modifiers::SYMBOL has no equivalent in
    // //ui/events/event_constants.h.
    DLOG(WARNING) << "Ignoring unsupported Symbol modifier.";
    modifiers &= ~fuchsia::ui::input3::Modifiers::SYMBOL;
  }

  MaybeAddFlag(fuchsia::ui::input3::Modifiers::SHIFT, EF_SHIFT_DOWN,
               event_flags, modifiers);
  if (modifiers & (fuchsia::ui::input3::Modifiers::LEFT_SHIFT |
                   fuchsia::ui::input3::Modifiers::RIGHT_SHIFT)) {
    DCHECK(event_flags & EF_SHIFT_DOWN)
        << "Fuchsia is expected to provide an agnostic SHIFT modifier for both "
           "LEFT and RIGHT SHIFT";
    modifiers &= ~fuchsia::ui::input3::Modifiers::LEFT_SHIFT &
                 ~fuchsia::ui::input3::Modifiers::RIGHT_SHIFT;
  }

  MaybeAddFlag(fuchsia::ui::input3::Modifiers::ALT, EF_ALT_DOWN, event_flags,
               modifiers);
  if (modifiers & (fuchsia::ui::input3::Modifiers::LEFT_ALT |
                   fuchsia::ui::input3::Modifiers::RIGHT_ALT)) {
    DCHECK(event_flags & EF_ALT_DOWN)
        << "Fuchsia is expected to provide an agnostic ALT modifier for both "
           "LEFT and RIGHT ALT";
    modifiers &= ~fuchsia::ui::input3::Modifiers::LEFT_ALT &
                 ~fuchsia::ui::input3::Modifiers::RIGHT_ALT;
  }

  MaybeAddFlag(fuchsia::ui::input3::Modifiers::ALT_GRAPH, EF_ALTGR_DOWN,
               event_flags, modifiers);

  MaybeAddFlag(fuchsia::ui::input3::Modifiers::META, EF_COMMAND_DOWN,
               event_flags, modifiers);
  if (modifiers & (fuchsia::ui::input3::Modifiers::LEFT_META |
                   fuchsia::ui::input3::Modifiers::RIGHT_META)) {
    DCHECK(event_flags & EF_COMMAND_DOWN)
        << "Fuchsia is expected to provide an agnostic META modifier for both "
           "LEFT and RIGHT META";
    modifiers &= ~fuchsia::ui::input3::Modifiers::LEFT_META &
                 ~fuchsia::ui::input3::Modifiers::RIGHT_META;
  }

  MaybeAddFlag(fuchsia::ui::input3::Modifiers::CTRL, EF_CONTROL_DOWN,
               event_flags, modifiers);
  if (modifiers & (fuchsia::ui::input3::Modifiers::LEFT_CTRL |
                   fuchsia::ui::input3::Modifiers::RIGHT_CTRL)) {
    DCHECK(event_flags & EF_CONTROL_DOWN)
        << "Fuchsia is expected to provide an agnostic CTRL modifier for both "
           "LEFT and RIGHT CTRL";
    modifiers &= ~fuchsia::ui::input3::Modifiers::LEFT_CTRL &
                 ~fuchsia::ui::input3::Modifiers::RIGHT_CTRL;
  }

  DLOG_IF(WARNING, modifiers)
      << "Unkown Modifier received: " << static_cast<uint64_t>(modifiers);
  return event_flags;
}

absl::optional<EventType> ConvertKeyEventType(
    fuchsia::ui::input3::KeyEventType type) {
  switch (type) {
    case fuchsia::ui::input3::KeyEventType::PRESSED:
      return ET_KEY_PRESSED;
    case fuchsia::ui::input3::KeyEventType::RELEASED:
      return ET_KEY_RELEASED;
    case fuchsia::ui::input3::KeyEventType::SYNC:
    case fuchsia::ui::input3::KeyEventType::CANCEL:
      // SYNC and CANCEL should not generate ui::Events.
      return absl::nullopt;
    default:
      NOTREACHED() << "Unknown KeyEventType received: "
                   << static_cast<int>(type);
      return absl::nullopt;
  }
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
  if (!IsValid(key_event)) {
    binding_.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  if (ProcessKeyEvent(key_event)) {
    callback(fuchsia::ui::input3::KeyEventStatus::HANDLED);
  } else {
    callback(fuchsia::ui::input3::KeyEventStatus::NOT_HANDLED);
  }
}

bool KeyboardClient::IsValid(const fuchsia::ui::input3::KeyEvent& key_event) {
  if (!key_event.has_type() || !key_event.has_timestamp())
    return false;

  if (!key_event.has_key() && !key_event.has_key_meaning())
    return false;

  return true;
}

bool KeyboardClient::ProcessKeyEvent(
    const fuchsia::ui::input3::KeyEvent& key_event) {
  absl::optional<EventType> event_type = ConvertKeyEventType(key_event.type());
  if (!event_type)
    return false;

  // Convert |key_event| to a ui::KeyEvent.
  int event_flags = EF_NONE;
  if (key_event.has_modifiers())
    event_flags |= ModifiersToEventFlags(key_event.modifiers());
  if (key_event.has_repeat_sequence()) {
    event_flags |= EF_IS_REPEAT;
  }

  // Derive the DOM Key and Code directly from the event's fields.
  // |key_event| has already been validated, so is guaranteed to have one
  // or both of the |key| or |key_meaning| fields set.
  DomCode dom_code = DomCode::NONE;
  DomKey dom_key = DomKey::UNIDENTIFIED;
  KeyboardCode key_code = VKEY_UNKNOWN;

  if (key_event.has_key()) {
    dom_code = KeycodeConverter::UsbKeycodeToDomCode(key_event.key());

    // Derive the legacy key_code. At present this only takes into account the
    // DOM Code, and event flags, so requires that key() be set.
    // TODO(crbug.com/1187257): Take into account the KeyMeaning, similarly to
    // the X11 event conversion implementation.
    // TODO(fxbug.dev/106600): Remove default-derivation of DOM Key, once the
    // platform defines the missing values.
    std::ignore =
        DomCodeToUsLayoutDomKey(dom_code, event_flags, &dom_key, &key_code);
  }

  if (key_event.has_key_meaning()) {
    // If the KeyMeaning is specified then use it to set the DOM Key.

    // Ignore events with codepoints outside the Basic Multilingual Plane,
    // since the Chromium keyboard pipeline cannot currently handle them.
    if (key_event.key_meaning().is_codepoint() &&
        (key_event.key_meaning().codepoint() >
         std::numeric_limits<char16_t>::max())) {
      return false;
    }

    DomKey dom_key_from_meaning =
        DomKeyFromFuchsiaKeyMeaning(key_event.key_meaning());
    if (dom_key_from_meaning != DomKey::UNIDENTIFIED)
      dom_key = dom_key_from_meaning;
  }

  ui::KeyEvent converted_event(
      *event_type, key_code, dom_code, event_flags, dom_key,
      base::TimeTicks::FromZxTime(key_event.timestamp()));
  event_sink_->DispatchEvent(&converted_event);
  return converted_event.handled();
}

}  // namespace ui
