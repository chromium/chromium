// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/keyboard_modifier_event_rewriter.h"

#include "base/containers/fixed_flat_map.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/ash/event_property.h"
#include "ui/events/ash/event_rewriter_metrics.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/pref_names.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter_continuation.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ui {
namespace {

bool IsFirstPartyKoreanIME() {
  auto* manager = ash::input_method::InputMethodManager::Get();
  if (!manager) {
    return false;
  }

  auto current_input_method =
      manager->GetActiveIMEState()->GetCurrentInputMethod();
  return ash::extension_ime_util::IsCros1pKorean(current_input_method.id());
}

bool IsISOLevel5ShiftUsedByCurrentInputMethod() {
  // Since both German Neo2 XKB layout and Caps Lock depend on Mod3Mask,
  // it's not possible to make both features work. For now, we don't remap
  // Mod3Mask when Neo2 is in use.
  // TODO(yusukes): Remove the restriction.
  auto* manager = ash::input_method::InputMethodManager::Get();
  return manager->IsISOLevel5ShiftUsedByCurrentInputMethod();
}

constexpr auto kRemappedKeyMap =
    base::MakeFixedFlatMap<mojom::ModifierKey,
                           KeyboardModifierEventRewriter::RemappedKey>({
        {mojom::ModifierKey::kControl,
         {DomCode::CONTROL_LEFT, DomKey::CONTROL, VKEY_CONTROL,
          EF_CONTROL_DOWN}},
        {mojom::ModifierKey::kIsoLevel5ShiftMod3,
         {DomCode::CAPS_LOCK, DomKey::ALT_GRAPH, VKEY_ALTGR,
          EF_MOD3_DOWN | EF_ALTGR_DOWN}},
        {mojom::ModifierKey::kMeta,
         {DomCode::META_LEFT, DomKey::META, VKEY_LWIN, EF_COMMAND_DOWN}},
        {mojom::ModifierKey::kAlt,
         {DomCode::ALT_LEFT, DomKey::ALT, VKEY_MENU, EF_ALT_DOWN}},
        {mojom::ModifierKey::kVoid,
         {DomCode::NONE, DomKey::NONE, VKEY_UNKNOWN, EF_NONE}},
        {mojom::ModifierKey::kCapsLock,
         {DomCode::CAPS_LOCK, DomKey::CAPS_LOCK, VKEY_CAPITAL,
          EF_CAPS_LOCK_ON | EF_MOD3_DOWN}},
        {mojom::ModifierKey::kEscape,
         {DomCode::ESCAPE, DomKey::ESCAPE, VKEY_ESCAPE, EF_NONE}},
        {mojom::ModifierKey::kBackspace,
         {DomCode::BACKSPACE, DomKey::BACKSPACE, VKEY_BACK, EF_NONE}},
        {mojom::ModifierKey::kAssistant,
         {DomCode::LAUNCH_ASSISTANT, DomKey::LAUNCH_ASSISTANT, VKEY_ASSISTANT,
          EF_NONE}},
    });

const KeyboardModifierEventRewriter::RemappedKey* FindRemappedKeyByDomCode(
    DomCode code) {
  for (const auto& entry : kRemappedKeyMap) {
    if (entry.second.code == code) {
      return &entry.second;
    }
  }
  return nullptr;
}

constexpr KeyboardModifierEventRewriter::RemappedKey kAltGraphRemap = {
    std::nullopt,
    DomKey::ALT_GRAPH,
    VKEY_ALTGR,
    EF_ALTGR_DOWN,
};

constexpr KeyboardModifierEventRewriter::RemappedKey kIsoLevel5ShiftMod3Remap =
    {
        DomCode::CAPS_LOCK,
        DomKey::ALT_GRAPH,
        VKEY_ALTGR,
        EF_MOD3_DOWN | EF_ALTGR_DOWN,
};

std::optional<DomCode> RelocateDomCode(DomCode original_code,
                                       std::optional<DomCode> rewritten_code) {
  if (KeycodeConverter::DomCodeToLocation(original_code) ==
          DomKeyLocation::RIGHT &&
      rewritten_code.has_value()) {
    switch (*rewritten_code) {
      case DomCode::CONTROL_LEFT:
        return DomCode::CONTROL_RIGHT;
      case DomCode::ALT_LEFT:
        return DomCode::ALT_RIGHT;
      case DomCode::META_LEFT:
        return DomCode::META_RIGHT;
      default:
        // Do nothing.
        break;
    }
  }
  return rewritten_code;
}

KeyboardCode RelocateKeyboardCode(DomCode original_code,
                                  KeyboardCode key_code) {
  // The only L/R variation of KeyboardCode that this rewriter supports is
  // LWIN/RWIN.
  if (KeycodeConverter::DomCodeToLocation(original_code) ==
          DomKeyLocation::RIGHT &&
      key_code == VKEY_LWIN) {
    return VKEY_RWIN;
  }
  return key_code;
}

}  // namespace

KeyboardModifierEventRewriter::KeyboardModifierEventRewriter(
    std::unique_ptr<Delegate> delegate,
    KeyboardCapability* keyboard_capability,
    ash::input_method::ImeKeyboard* ime_keyboard)
    : delegate_(std::move(delegate)),
      keyboard_capability_(keyboard_capability),
      ime_keyboard_(ime_keyboard) {}

KeyboardModifierEventRewriter::~KeyboardModifierEventRewriter() = default;

EventDispatchDetails KeyboardModifierEventRewriter::RewriteEvent(
    const Event& event,
    const Continuation continuation) {
  std::unique_ptr<Event> rewritten_event;
  switch (event.type()) {
    case ET_KEY_PRESSED: {
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
            event_for_record->code());
      }
      break;
    }
    case ET_KEY_RELEASED:
      rewritten_event = RewriteReleaseKeyEvent(*event.AsKeyEvent());
      break;
    default:
      // Do nothing,
      break;
  }

  // Update flags by reconstructing them from the modifier key status.
  {
    int flags = rewritten_event ? rewritten_event->flags() : event.flags();
    int rewritten_flags = RewriteModifierFlags(event.flags());
    if (flags != rewritten_flags) {
      if (!rewritten_event) {
        rewritten_event = event.Clone();
      }
      // Note: this updates DomKey to reflect the new flags.
      rewritten_event->SetFlags(rewritten_flags);
    }
  }

  const Event& result_event = rewritten_event ? *rewritten_event : event;
  if (result_event.type() == ET_KEY_PRESSED &&
      !KeycodeConverter::IsDomKeyForModifier(
          result_event.AsKeyEvent()->GetDomKey())) {
    altgr_latch_ = false;
  }
  return continuation->SendEvent(&result_event);
}

std::unique_ptr<Event> KeyboardModifierEventRewriter::RewritePressKeyEvent(
    const KeyEvent& event) {
  int device_id = GetKeyboardDeviceIdProperty(event);

  if (!delegate_->RewriteModifierKeys() || (event.flags() & EF_FINAL)) {
    if (auto* remapped_key = FindRemappedKeyByDomCode(event.code())) {
      pressed_modifier_keys_.insert_or_assign(
          internal::PhysicalKey{event.code(), device_id}, *remapped_key);
    }
    return nullptr;
  }

  const RemappedKey* remapped_key = nullptr;
  switch (event.GetDomKey()) {
    case DomKey::ALT_GRAPH:
      // The Neo2 codes modifiers such that CapsLock appears as VKEY_ALTGR,
      // but AltGraph (right Alt) also appears as VKEY_ALTGR in Neo2,
      // as it does in other layouts. Neo2's "Mod3" is represented in
      // EventFlags by a combination of AltGr+Mod3, while its "Mod4" is
      // AltGr alone.
      if (IsISOLevel5ShiftUsedByCurrentInputMethod()) {
        remapped_key = GetRemappedKey(event.code() == DomCode::CAPS_LOCK
                                          ? mojom::ModifierKey::kCapsLock
                                          : mojom::ModifierKey::kMeta,
                                      device_id);
        if (remapped_key && remapped_key->key_code == VKEY_CAPITAL) {
          remapped_key = &kIsoLevel5ShiftMod3Remap;
        }
      }
      break;
    case DomKey::ALT_GRAPH_LATCH:
      // Rewrite to AltGraph. When this key is used like a regular modifier,
      // the web-exposed result looks like a use of the regular modifier.
      // When it's used as a latch, the web-exposed result is a vacuous
      // modifier press-and-release, which should be harmless, but preserves
      // the event for applications using the |code| (e.g. remoting).
      altgr_latch_ = true;

      remapped_key = &kAltGraphRemap;
      break;
  }

  switch (event.code()) {
    // On Chrome OS, Caps_Lock with Mod3Mask is sent when Caps Lock is pressed
    // (with one exception: when IsISOLevel5ShiftUsedByCurrentInputMethod() is
    // true, the key generates XK_ISO_Level3_Shift with Mod3Mask, not
    // Caps_Lock).
    case DomCode::CAPS_LOCK:
      // This key is already remapped to Mod3 in remapping based on DomKey. Skip
      // more remapping.
      if (remapped_key) {
        break;
      }

      remapped_key = GetRemappedKey(mojom::ModifierKey::kCapsLock, device_id);
      break;
    case DomCode::META_LEFT:
    case DomCode::META_RIGHT:
      remapped_key = GetRemappedKey(mojom::ModifierKey::kMeta, device_id);
      break;
    case DomCode::CONTROL_LEFT:
    case DomCode::CONTROL_RIGHT:
      remapped_key = GetRemappedKey(mojom::ModifierKey::kControl, device_id);
      break;
    case DomCode::ALT_RIGHT:
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
        break;
      }
      [[fallthrough]];
    case DomCode::ALT_LEFT:
      remapped_key = GetRemappedKey(mojom::ModifierKey::kAlt, device_id);
      break;
    case DomCode::ESCAPE:
      remapped_key = GetRemappedKey(mojom::ModifierKey::kEscape, device_id);
      break;
    case DomCode::BACKSPACE:
      remapped_key = GetRemappedKey(mojom::ModifierKey::kBackspace, device_id);
      break;
    case DomCode::LAUNCH_ASSISTANT:
      remapped_key = GetRemappedKey(mojom::ModifierKey::kAssistant, device_id);
      break;
    default:
      break;
  }

  if (!remapped_key) {
    return nullptr;
  }

  // Adjust left/right modifier key positions.
  RemappedKey relocated_remapped_key = *remapped_key;
  relocated_remapped_key.code =
      RelocateDomCode(event.code(), remapped_key->code);
  relocated_remapped_key.key_code =
      RelocateKeyboardCode(event.code(), remapped_key->key_code);

  if (pressed_modifier_keys_
          .insert_or_assign(internal::PhysicalKey{event.code(), device_id},
                            relocated_remapped_key)
          .second) {
    // Flip capslock state if needed. Note: do not on repeated events.
    if (relocated_remapped_key.code == DomCode::CAPS_LOCK) {
      ime_keyboard_->SetCapsLockEnabled(!ime_keyboard_->IsCapsLockEnabled());
    }
  }
  return BuildRewrittenEvent(event, relocated_remapped_key);
}

std::unique_ptr<Event> KeyboardModifierEventRewriter::RewriteReleaseKeyEvent(
    const KeyEvent& event) {
  int device_id = GetKeyboardDeviceIdProperty(event);
  auto it = pressed_modifier_keys_.find(
      internal::PhysicalKey{event.code(), device_id});
  if (it == pressed_modifier_keys_.end()) {
    return nullptr;
  }
  auto rewritten_event = BuildRewrittenEvent(event, it->second);
  pressed_modifier_keys_.erase(it);
  return rewritten_event;
}

std::unique_ptr<KeyEvent> KeyboardModifierEventRewriter::BuildRewrittenEvent(
    const KeyEvent& event,
    const RemappedKey& remapped) {
  if (remapped.key_code == event.key_code() && remapped.code == event.code() &&
      remapped.flags == event.flags() && remapped.key == event.GetDomKey()) {
    // Nothing is rewritten.
    return nullptr;
  }

  auto rewritten_event = std::make_unique<KeyEvent>(
      event.type(), remapped.key_code, remapped.code.value_or(event.code()),
      remapped.flags, remapped.key, event.time_stamp());
  rewritten_event->set_scan_code(event.scan_code());
  rewritten_event->set_source_device_id(event.source_device_id());
  if (const auto* properties = event.properties()) {
    rewritten_event->SetProperties(*properties);
  }
  return rewritten_event;
}

int KeyboardModifierEventRewriter::RewriteModifierFlags(int flags) const {
  // Bit mask of modifier flags to be rewritten.
  constexpr int kTargetModifierFlags = EF_CONTROL_DOWN | EF_ALT_DOWN |
                                       EF_COMMAND_DOWN | EF_ALTGR_DOWN |
                                       EF_MOD3_DOWN;
  flags &= ~kTargetModifierFlags;

  // Recalculate modifier flags from the currently pressed keys.
  for (const auto& [unused, pressed_modifier_key] : pressed_modifier_keys_) {
    flags |= pressed_modifier_key.flags;
  }

  // Update CapsLock.
  flags &= ~EF_CAPS_LOCK_ON;
  if (ime_keyboard_->IsCapsLockEnabled()) {
    flags |= EF_CAPS_LOCK_ON;
  }

  // Update latched ALTGR modifier.
  if (altgr_latch_) {
    flags |= EF_ALTGR_DOWN;
  }

  return flags;
}

const KeyboardModifierEventRewriter::RemappedKey*
KeyboardModifierEventRewriter::GetRemappedKey(mojom::ModifierKey modifier_key,
                                              int device_id) const {
  std::string_view pref_name;
  switch (modifier_key) {
    case mojom::ModifierKey::kMeta:
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

    case mojom::ModifierKey::kControl:
      pref_name = prefs::kLanguageRemapControlKeyTo;
      break;
    case mojom::ModifierKey::kAlt:
      pref_name = prefs::kLanguageRemapAltKeyTo;
      break;
    case mojom::ModifierKey::kVoid:
      LOG(FATAL) << "No pref_name for kVoid";
    case mojom::ModifierKey::kCapsLock:
      pref_name = prefs::kLanguageRemapCapsLockKeyTo;
      break;
    case mojom::ModifierKey::kEscape:
      pref_name = prefs::kLanguageRemapEscapeKeyTo;
      break;
    case mojom::ModifierKey::kBackspace:
      pref_name = prefs::kLanguageRemapBackspaceKeyTo;
      break;
    case mojom::ModifierKey::kAssistant:
      pref_name = prefs::kLanguageRemapAssistantKeyTo;
      break;
    case mojom::ModifierKey::kIsoLevel5ShiftMod3:
      LOG(FATAL) << "No pref_name for kIsoLevel5ShiftMod3";
  }
  CHECK(!pref_name.empty());

  auto modifier_value = delegate_->GetKeyboardRemappedModifierValue(
      device_id, modifier_key, std::string(pref_name));
  auto* it = kRemappedKeyMap.find(modifier_value.value_or(modifier_key));
  CHECK(it != kRemappedKeyMap.end());
  return &it->second;
}

}  // namespace ui
