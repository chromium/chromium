// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/fuchsia/keyboard_client.h"

#include <lib/async/default.h>

#include <limits>
#include <tuple>
#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/notreached.h"
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
inline void MaybeAddFlag(fuchsia_ui_input3::Modifiers modifier,
                         EventFlags flag,
                         EventFlags& event_flags,
                         fuchsia_ui_input3::Modifiers& unhandled_modifiers) {
  if (unhandled_modifiers & modifier) {
    event_flags |= flag;
    // Remove modifier from unhandled.
    unhandled_modifiers &= ~modifier;
  }
}

// Converts the state of modifiers managed by Fuchsia (e.g. Caps and Num Lock)
// into ui::Event flags.
int ModifiersToEventFlags(fuchsia_ui_input3::Modifiers modifiers) {
  EventFlags event_flags = EF_NONE;
  MaybeAddFlag(fuchsia_ui_input3::Modifiers::kCapsLock, EF_CAPS_LOCK_ON,
               event_flags, modifiers);
  MaybeAddFlag(fuchsia_ui_input3::Modifiers::kNumLock, EF_NUM_LOCK_ON,
               event_flags, modifiers);
  MaybeAddFlag(fuchsia_ui_input3::Modifiers::kScrollLock, EF_SCROLL_LOCK_ON,
               event_flags, modifiers);

  // This mapping is present in case blink adds support in the future, but blink
  // doesn't currently output the Function modifier. See
  // https://crsrc.org/c/ui/events/blink/blink_event_util.cc;l=268?q=EventFlagsToWebEventModifiers
  MaybeAddFlag(fuchsia_ui_input3::Modifiers::kFunction, EF_FUNCTION_DOWN,
               event_flags, modifiers);
  if (modifiers & fuchsia_ui_input3::Modifiers::kSymbol) {
    // fuchsia_ui_input3::Modifiers::SYMBOL has no equivalent in
    // //ui/events/event_constants.h.
    DLOG(WARNING) << "Ignoring unsupported Symbol modifier.";
    modifiers &= ~fuchsia_ui_input3::Modifiers::kSymbol;
  }

  MaybeAddFlag(fuchsia_ui_input3::Modifiers::kShift, EF_SHIFT_DOWN, event_flags,
               modifiers);
  if (modifiers & (fuchsia_ui_input3::Modifiers::kLeftShift |
                   fuchsia_ui_input3::Modifiers::kRightShift)) {
    DCHECK(event_flags & EF_SHIFT_DOWN)
        << "Fuchsia is expected to provide an agnostic SHIFT modifier for both "
           "LEFT and RIGHT SHIFT";
    modifiers &= ~fuchsia_ui_input3::Modifiers::kLeftShift &
                 ~fuchsia_ui_input3::Modifiers::kRightShift;
  }

  MaybeAddFlag(fuchsia_ui_input3::Modifiers::kAlt, EF_ALT_DOWN, event_flags,
               modifiers);
  if (modifiers & (fuchsia_ui_input3::Modifiers::kLeftAlt |
                   fuchsia_ui_input3::Modifiers::kRightAlt)) {
    DCHECK(event_flags & EF_ALT_DOWN)
        << "Fuchsia is expected to provide an agnostic ALT modifier for both "
           "LEFT and RIGHT ALT";
    modifiers &= ~fuchsia_ui_input3::Modifiers::kLeftAlt &
                 ~fuchsia_ui_input3::Modifiers::kRightAlt;
  }

  MaybeAddFlag(fuchsia_ui_input3::Modifiers::kAltGraph, EF_ALTGR_DOWN,
               event_flags, modifiers);

  MaybeAddFlag(fuchsia_ui_input3::Modifiers::kMeta, EF_COMMAND_DOWN,
               event_flags, modifiers);
  if (modifiers & (fuchsia_ui_input3::Modifiers::kLeftMeta |
                   fuchsia_ui_input3::Modifiers::kRightMeta)) {
    DCHECK(event_flags & EF_COMMAND_DOWN)
        << "Fuchsia is expected to provide an agnostic META modifier for both "
           "LEFT and RIGHT META";
    modifiers &= ~fuchsia_ui_input3::Modifiers::kLeftMeta &
                 ~fuchsia_ui_input3::Modifiers::kRightMeta;
  }

  MaybeAddFlag(fuchsia_ui_input3::Modifiers::kCtrl, EF_CONTROL_DOWN,
               event_flags, modifiers);
  if (modifiers & (fuchsia_ui_input3::Modifiers::kLeftCtrl |
                   fuchsia_ui_input3::Modifiers::kRightCtrl)) {
    DCHECK(event_flags & EF_CONTROL_DOWN)
        << "Fuchsia is expected to provide an agnostic CTRL modifier for both "
           "LEFT and RIGHT CTRL";
    modifiers &= ~fuchsia_ui_input3::Modifiers::kLeftCtrl &
                 ~fuchsia_ui_input3::Modifiers::kRightCtrl;
  }

  DLOG_IF(WARNING, modifiers)
      << "Unkown Modifier received: " << static_cast<uint64_t>(modifiers);
  return event_flags;
}

std::optional<EventType> ConvertKeyEventType(
    fuchsia_ui_input3::KeyEventType type) {
  switch (type) {
    case fuchsia_ui_input3::KeyEventType::kPressed:
      return EventType::kKeyPressed;
    case fuchsia_ui_input3::KeyEventType::kReleased:
      return EventType::kKeyReleased;
    case fuchsia_ui_input3::KeyEventType::kSync:
    case fuchsia_ui_input3::KeyEventType::kCancel:
      // SYNC and CANCEL should not generate ui::Events.
      return std::nullopt;
    default:
      NOTREACHED() << "Unknown KeyEventType received: "
                   << static_cast<int>(type);
  }
}

}  // namespace

KeyboardClient::KeyboardClient(
    fidl::Client<fuchsia_ui_input3::Keyboard>& keyboard_fidl_client,
    fuchsia_ui_views::ViewRef view_ref,
    InputEventSink* event_sink)
    : event_sink_(event_sink) {
  DCHECK(event_sink_);

  // Connect to the Keyboard service and register `keyboard_client_` as a
  // listener.
  auto keyboard_listener_endpoints =
      fidl::CreateEndpoints<fuchsia_ui_input3::KeyboardListener>();
  ZX_CHECK(keyboard_listener_endpoints.is_ok(),
           keyboard_listener_endpoints.status_value());
  keyboard_fidl_client
      ->AddListener(
          {{.view_ref = std::move(view_ref),
            .listener = std::move(keyboard_listener_endpoints->client)}})
      .Then([](auto result) {});
  binding_.emplace(async_get_default_dispatcher(),
                   std::move(keyboard_listener_endpoints->server), this,
                   fidl::kIgnoreBindingClosure);
}

KeyboardClient::~KeyboardClient() = default;

void KeyboardClient::OnKeyEvent(
    KeyboardClient::OnKeyEventRequest& request,
    KeyboardClient::OnKeyEventCompleter::Sync& completer) {
  if (!IsValid(request.event())) {
    binding_->Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  if (ProcessKeyEvent(request.event())) {
    completer.Reply(fuchsia_ui_input3::KeyEventStatus::kHandled);
  } else {
    completer.Reply(fuchsia_ui_input3::KeyEventStatus::kNotHandled);
  }
}

bool KeyboardClient::IsValid(const fuchsia_ui_input3::KeyEvent& key_event) {
  if (!key_event.type() || !key_event.timestamp()) {
    return false;
  }

  if (!key_event.key() && !key_event.key_meaning()) {
    return false;
  }

  return true;
}

bool KeyboardClient::ProcessKeyEvent(
    const fuchsia_ui_input3::KeyEvent& key_event) {
  std::optional<EventType> event_type =
      ConvertKeyEventType(key_event.type().value());
  if (!event_type)
    return false;

  // Convert `key_event` to a ui::KeyEvent.
  int event_flags = EF_NONE;
  if (key_event.modifiers()) {
    event_flags |= ModifiersToEventFlags(key_event.modifiers().value());
  }
  if (key_event.repeat_sequence()) {
    event_flags |= EF_IS_REPEAT;
  }

  // Derive the DOM Key and Code directly from the event's fields.
  // `key_event` has already been validated, so is guaranteed to have one
  // or both of the `key` or `key_meaning` fields set.
  DomCode dom_code = DomCode::NONE;
  DomKey dom_key = DomKey::UNIDENTIFIED;
  KeyboardCode key_code = VKEY_UNKNOWN;

  if (key_event.key()) {
    dom_code = KeycodeConverter::UsbKeycodeToDomCode(
        static_cast<uint32_t>(key_event.key().value()));

    // Derive the legacy key_code. At present this only takes into account the
    // DOM Code, and event flags, so requires that key() be set.
    // TODO(crbug.com/42050247): Take into account the KeyMeaning, similarly to
    // the X11 event conversion implementation.
    // TODO(fxbug.dev/106600): Remove default-derivation of DOM Key, once the
    // platform defines the missing values.
    std::ignore =
        DomCodeToUsLayoutDomKey(dom_code, event_flags, &dom_key, &key_code);
  }

  if (key_event.key_meaning()) {
    // If the KeyMeaning is specified then use it to set the DOM Key.

    // Ignore events with codepoints outside the Basic Multilingual Plane,
    // since the Chromium keyboard pipeline cannot currently handle them.
    if (key_event.key_meaning()->codepoint() &&
        (key_event.key_meaning()->codepoint().value() >
         std::numeric_limits<char16_t>::max())) {
      return false;
    }

    DomKey dom_key_from_meaning =
        DomKeyFromFuchsiaKeyMeaning(key_event.key_meaning().value());
    if (dom_key_from_meaning != DomKey::UNIDENTIFIED)
      dom_key = dom_key_from_meaning;
  }

  ui::KeyEvent converted_event(
      *event_type, key_code, dom_code, event_flags, dom_key,
      base::TimeTicks::FromZxTime(key_event.timestamp().value()));
  event_sink_->DispatchEvent(&converted_event);
  return converted_event.handled();
}

}  // namespace ui
