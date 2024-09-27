// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/keyboard_modifier_event_rewriter.h"

#include <variant>

#include "ash/constants/ash_features.h"
#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "ui/base/accelerators/ash/right_alt_event_property.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/ash/event_property.h"
#include "ui/events/ash/event_rewriter_metrics.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/ash/pref_names.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_rewriter_continuation.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"

namespace ui {
namespace {

using PhysicalCode = KeyboardModifierEventRewriter::PhysicalCode;
using UnmappedCode = KeyboardModifierEventRewriter::UnmappedCode;

bool IsFirstPartyKoreanIME() {
  auto* manager = ash::input_method::InputMethodManager::Get();
  if (!manager) {
    return false;
  }

  auto current_input_method =
      manager->GetActiveIMEState()->GetCurrentInputMethod();
  return ash::extension_ime_util::IsCros1pKorean(current_input_method.id());
}

DomCode GetDomCodeFromPhysicalCode(const PhysicalCode& physical_code) {
  if (const UnmappedCode* unmapped_code =
          std::get_if<UnmappedCode>(&physical_code)) {
    switch (*unmapped_code) {
      case UnmappedCode::kRightAlt:
        return DomCode::LAUNCH_ASSISTANT;
    }
  }

  return std::get<DomCode>(physical_code);
}

}  // namespace

KeyboardModifierEventRewriter::KeyboardModifierEventRewriter(
    std::unique_ptr<Delegate> delegate,
    KeyboardLayoutEngine* keyboard_layout_engine,
    KeyboardCapability* keyboard_capability,
    ash::input_method::ImeKeyboard* ime_keyboard)
    : delegate_(std::move(delegate)),
      keyboard_layout_engine_(keyboard_layout_engine),
      keyboard_capability_(keyboard_capability),
      ime_keyboard_(ime_keyboard) {}

KeyboardModifierEventRewriter::~KeyboardModifierEventRewriter() = default;

EventDispatchDetails KeyboardModifierEventRewriter::RewriteEvent(
    const Event& event,
    const Continuation continuation) {
  std::unique_ptr<Event> rewritten_event;
  switch (event.type()) {
    case EventType::kKeyPressed: {
      bool should_record_metrics = !(event.flags() & EF_IS_REPEAT);
      if (should_record_metrics) {
        RecordModifierKeyPressedBeforeRemapping(
            *keyboard_capability_, GetKeyboardDeviceIdProperty(event),
            event.AsKeyEvent()->code());
      }

      rewritten_event = RewritePressKeyEvent(*event.AsKeyEvent());

      if (should_record_metrics) {
        const KeyEvent* event_for_record = rewritten_event
                                               ? rewritten_event->AsKeyEvent()
                                               : event.AsKeyEvent();
        RecordModifierKeyPressedAfterRemapping(
            *keyboard_capability_,
            GetKeyboardDeviceIdProperty(*event_for_record),
            event_for_record->code(), event.AsKeyEvent()->code(),
            HasRightAltProperty(*event_for_record));
      }
      break;
    }
    case EventType::kKeyReleased:
      rewritten_event = RewriteReleaseKeyEvent(*event.AsKeyEvent());
      break;
    default: {
      // Update flags by reconstructing them from the modifier key status.
      int flags = event.flags();
      int rewritten_flags = RewriteModifierFlags(event.flags());
      if (flags != rewritten_flags) {
        rewritten_event = event.Clone();

        // SetNativeEvent must be called explicitly as native events are not
        // copied on ChromeOS by default. This is because `PlatformEvent` is a
        // pointer by default, so its lifetime can not be guaranteed in general.
        // In this case, the lifetime of  `rewritten_event` is guaranteed to be
        // less than the original `event`.
        SetNativeEvent(*rewritten_event, event.native_event());

        // Note: this updates DomKey to reflect the new flags.
        rewritten_event->SetFlags(rewritten_flags);
      }
      break;
    }
  }

  return continuation->SendEvent(rewritten_event ? rewritten_event.get()
                                                 : &event);
}

std::unique_ptr<Event> KeyboardModifierEventRewriter::RewritePressKeyEvent(
    const KeyEvent& event) {
  internal::PhysicalKey physical_key{event.code(),
                                     GetKeyboardDeviceIdProperty(event)};

  // Remap key based on user preferences.
  RemappedKey remapped = RemapPressKey(event).value_or(
      RemappedKey{event.code(), event.GetDomKey(), event.key_code()});

  // Normalize ALT_GRAPH_LATCH to ALT_GRAPH here with remembering altgr-latch
  // behavior, in order to merge the normalize into remap info.
  if (remapped.key == DomKey::ALT_GRAPH_LATCH) {
    altgr_latch_ = true;
    remapped.key = DomKey::ALT_GRAPH;
    remapped.key_code = VKEY_ALTGR;
  }

  // Remember the remapping on press. This remapping will be reapplied to the
  // release event.
  if (GetDomCodeFromPhysicalCode(remapped.code) != event.code() ||
      remapped.key != event.GetDomKey() ||
      remapped.key_code != event.key_code() ||
      std::holds_alternative<UnmappedCode>(remapped.code)) {
    remapped_keys_.insert_or_assign(physical_key, remapped);
  }

  // Update modifier flags.
  {
    EventFlags modifier_flag = ModifierDomKeyToEventFlag(remapped.key);
    if (modifier_flag == EF_CAPS_LOCK_ON) {
      // This is to be consistent with KeyboardEvdev::UpdateModifier.
      modifier_flag = EF_MOD3_DOWN;
    }
    // Short term workaround for Neo-2 keyboard. See b/349505909 for details.
    // TODO: Get rid of this once we support level3-shift properly.
    if (keyboard_layout_engine_->GetLayoutName() == "de(neo)" &&
        remapped.key == DomKey::ALT_GRAPH) {
      modifier_flag |= EF_MOD3_DOWN;
    }
    if (pressed_modifier_keys_.insert_or_assign(physical_key, modifier_flag)
            .second) {
      // Flip capslock state if needed. Note: do not on repeated events.
      // Toggling of CapsLock in the `ImeKeyboard` is handled by
      // `CapsLockEventRewriter`, here we only rewrite the physical key press to
      // CapsLock.
      if (!ash::features::IsModifierSplitEnabled() &&
          remapped.key == DomKey::CAPS_LOCK) {
        ime_keyboard_->SetCapsLockEnabled(!ime_keyboard_->IsCapsLockEnabled());
      }
    }
  }

  // Rebuild rewritten event.
  auto rewritten_event = BuildRewrittenEvent(event, remapped);

  // Update the altgr latch.
  if (!KeycodeConverter::IsDomKeyForModifier(
          (rewritten_event ? *rewritten_event : event).GetDomKey())) {
    altgr_latch_ = false;
  }

  return rewritten_event;
}

std::optional<KeyboardModifierEventRewriter::RemappedKey>
KeyboardModifierEventRewriter::RemapPressKey(const KeyEvent& event) {
  if (!delegate_->RewriteModifierKeys() || (event.flags() & EF_FINAL)) {
    return std::nullopt;
  }

  // For the Korean IME, right alt is used for Korean/English mode
  // switching. It should not be rewritten under any circumstance. Due to
  // b/311333438, the DomKey from the given keyboard layout is ignored.
  // Additionally, due to b/311327069, the DomCode and DomKey both get
  // remapped every time a modifier is pressed, even if it is not remapped.
  // By special casing right alt only for the Korean IME, we avoid this
  // problem.

  // TODO(b/311333438, b/311327069): Implement a complete solution to deal
  // with modifier remapping.
  if (event.GetDomKey() == DomKey::HANGUL_MODE && IsFirstPartyKoreanIME()) {
    return std::nullopt;
  }

  // TODO(b/369892786): Do not use VKEY as source of truth in events.
  if (event.key_code() >= VKEY_BUTTON_0 && event.key_code() <= VKEY_BUTTON_Z) {
    return std::nullopt;
  }

  // First DomCode is remapped based on user's preferences.
  const PhysicalCode remapped_code =
      GetRemappedPhysicalCode(event.code(), GetKeyboardDeviceIdProperty(event))
          .value_or(event.code());

  const DomCode remapped_dom_code = GetDomCodeFromPhysicalCode(remapped_code);
  if (remapped_dom_code == DomCode::NONE) {
    return {{DomCode::NONE, DomKey::NONE, VKEY_UNKNOWN}};
  }

  // Update DomKey and KeyboardCode respecting the current keyboard layout.
  // Use the modifier flags from the previous state.
  // This re-lookup is also needed for keys which didn't remapped, because the
  // modifier flags used to interpret original KeyEvent may be remapped.
  DomKey dom_key;
  KeyboardCode keycode;
  if (!keyboard_layout_engine_->Lookup(remapped_dom_code,
                                       RewriteModifierFlags(event.flags()),
                                       &dom_key, &keycode)) {
    LOG(ERROR) << "Failed to look up kayboard layout";
    return std::nullopt;
  }

  return {{remapped_code, dom_key, keycode}};
}

std::unique_ptr<Event> KeyboardModifierEventRewriter::RewriteReleaseKeyEvent(
    const KeyEvent& event) {
  int device_id = GetKeyboardDeviceIdProperty(event);
  internal::PhysicalKey physical_key{event.code(), device_id};
  pressed_modifier_keys_.erase(physical_key);

  // Instead of looking up the remap rule again here, we'll just reuse the remap
  // data on the pressed event, so that this release event is remapped in
  // the same way with the pressed event.
  std::optional<RemappedKey> remapped;
  if (auto it = remapped_keys_.find(physical_key); it != remapped_keys_.end()) {
    remapped = it->second;
    remapped_keys_.erase(it);
  }

  return BuildRewrittenEvent(event, remapped.value_or(RemappedKey{
                                        event.code(),
                                        event.GetDomKey(),
                                        event.key_code(),
                                    }));
}

std::unique_ptr<KeyEvent> KeyboardModifierEventRewriter::BuildRewrittenEvent(
    const KeyEvent& event,
    const RemappedKey& remapped) {
  // Events with unmapped codes must always be rewritten.
  EventFlags flags = RewriteModifierFlags(event.flags());
  if (GetDomCodeFromPhysicalCode(remapped.code) == event.code() &&
      remapped.key == event.GetDomKey() &&
      remapped.key_code == event.key_code() && flags == event.flags() &&
      !std::holds_alternative<UnmappedCode>(remapped.code)) {
    // Nothing is rewritten.
    return nullptr;
  }

  auto rewritten_event =
      std::make_unique<KeyEvent>(event.type(), remapped.key_code,
                                 GetDomCodeFromPhysicalCode(remapped.code),
                                 flags, remapped.key, event.time_stamp());
  rewritten_event->set_scan_code(event.scan_code());
  rewritten_event->set_source_device_id(event.source_device_id());
  if (const auto* properties = event.properties()) {
    rewritten_event->SetProperties(*properties);
  }
  // Set property if the unmapped code is Right Alt.
  if (const UnmappedCode* unmapped_code =
          std::get_if<UnmappedCode>(&remapped.code)) {
    if (*unmapped_code ==
        KeyboardModifierEventRewriter::UnmappedCode::kRightAlt) {
      SetRightAltProperty(rewritten_event.get());
    }
  }
  return rewritten_event;
}

EventFlags KeyboardModifierEventRewriter::RewriteModifierFlags(
    EventFlags flags) const {
  // Bit mask of modifier flags to be rewritten.
  constexpr EventFlags kTargetModifierFlags = EF_CONTROL_DOWN | EF_ALT_DOWN |
                                              EF_COMMAND_DOWN | EF_ALTGR_DOWN |
                                              EF_MOD3_DOWN | EF_FUNCTION_DOWN;
  flags &= ~kTargetModifierFlags;
  if (!ash::features::IsModifierSplitEnabled()) {
    flags &= ~EF_CAPS_LOCK_ON;
  }

  // Recalculate modifier flags from the currently pressed keys.
  for (const auto& [unused, modifier] : pressed_modifier_keys_) {
    flags |= modifier;
  }

  if (!ash::features::IsModifierSplitEnabled()) {
    // Update CapsLock.
    if (ime_keyboard_->IsCapsLockEnabled()) {
      flags |= EF_CAPS_LOCK_ON;
    }
  }

  // Update latched ALTGR modifier.
  if (altgr_latch_) {
    flags |= EF_ALTGR_DOWN;
  }

  return flags;
}

std::optional<PhysicalCode>
KeyboardModifierEventRewriter::GetRemappedPhysicalCode(DomCode code,
                                                       int device_id) const {
  bool is_left = true;
  mojom::ModifierKey modifier_key;
  std::string_view pref_name;
  switch (code) {
    case DomCode::META_RIGHT:
      is_left = false;
      [[fallthrough]];
    case DomCode::META_LEFT:
      modifier_key = mojom::ModifierKey::kMeta;
      switch (static_cast<KeyboardCapability::DeviceType>(
          keyboard_capability_->GetDeviceType(device_id))) {
        case KeyboardCapability::DeviceType::kDeviceExternalAppleKeyboard:
          pref_name = prefs::kLanguageRemapExternalCommandKeyTo;
          break;

        case KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard:
        case KeyboardCapability::DeviceType::kDeviceExternalUnknown:
          pref_name = prefs::kLanguageRemapExternalMetaKeyTo;
          break;

        case KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard:
        case KeyboardCapability::DeviceType::
            kDeviceExternalNullTopRowChromeOsKeyboard:
        case KeyboardCapability::DeviceType::kDeviceInternalKeyboard:
        case KeyboardCapability::DeviceType::kDeviceInternalRevenKeyboard:
        case KeyboardCapability::DeviceType::kDeviceHotrodRemote:
        case KeyboardCapability::DeviceType::kDeviceVirtualCoreKeyboard:
        case KeyboardCapability::DeviceType::kDeviceUnknown:
          // Use the preference for internal Search key remapping.
          pref_name = prefs::kLanguageRemapSearchKeyTo;
          break;
      }
      break;

    case DomCode::CONTROL_RIGHT:
      is_left = false;
      [[fallthrough]];
    case DomCode::CONTROL_LEFT:
      modifier_key = mojom::ModifierKey::kControl;
      pref_name = prefs::kLanguageRemapControlKeyTo;
      break;

    case DomCode::ALT_RIGHT:
      is_left = false;
      [[fallthrough]];
    case DomCode::ALT_LEFT:
      modifier_key = mojom::ModifierKey::kAlt;
      pref_name = prefs::kLanguageRemapAltKeyTo;
      break;

    case DomCode::CAPS_LOCK:
      modifier_key = mojom::ModifierKey::kCapsLock;
      pref_name = prefs::kLanguageRemapCapsLockKeyTo;
      break;

    case DomCode::ESCAPE:
      modifier_key = mojom::ModifierKey::kEscape;
      pref_name = prefs::kLanguageRemapEscapeKeyTo;
      break;

    case DomCode::BACKSPACE:
      modifier_key = mojom::ModifierKey::kBackspace;
      pref_name = prefs::kLanguageRemapBackspaceKeyTo;
      break;

    case DomCode::LAUNCH_ASSISTANT:
      // Right alt key must be checked explicitly on a per-device basis as it
      // shares the dom code.
      if (keyboard_capability_->HasRightAltKey(device_id)) {
        modifier_key = mojom::ModifierKey::kRightAlt;
        break;
      }
      modifier_key = mojom::ModifierKey::kAssistant;
      pref_name = prefs::kLanguageRemapAssistantKeyTo;
      break;

    case DomCode::FN:
      modifier_key = mojom::ModifierKey::kFunction;
      break;

    default:
      // No remapping.
      return std::nullopt;
  }
  CHECK(!pref_name.empty() ||
        ash::features::IsInputDeviceSettingsSplitEnabled());

  auto modifier_value = delegate_->GetKeyboardRemappedModifierValue(
      device_id, modifier_key, std::string(pref_name));

  switch (modifier_value.value_or(modifier_key)) {
    case mojom::ModifierKey::kMeta:
      return is_left ? DomCode::META_LEFT : DomCode::META_RIGHT;
    case mojom::ModifierKey::kControl:
      return is_left ? DomCode::CONTROL_LEFT : DomCode::CONTROL_RIGHT;
    case mojom::ModifierKey::kAlt:
      return is_left ? DomCode::ALT_LEFT : DomCode::ALT_RIGHT;
    case mojom::ModifierKey::kVoid:
      return DomCode::NONE;
    case mojom::ModifierKey::kCapsLock:
      return DomCode::CAPS_LOCK;
    case mojom::ModifierKey::kEscape:
      return DomCode::ESCAPE;
    case mojom::ModifierKey::kBackspace:
      return DomCode::BACKSPACE;
    case mojom::ModifierKey::kAssistant:
      return DomCode::LAUNCH_ASSISTANT;
    case mojom::ModifierKey::kIsoLevel5ShiftMod3:
      LOG(FATAL) << "Unexpected IsoLevel5ShiftMod3 config";
    case mojom::ModifierKey::kFunction:
      return DomCode::FN;
    case mojom::ModifierKey::kRightAlt:
      return UnmappedCode::kRightAlt;
  }
}

}  // namespace ui
