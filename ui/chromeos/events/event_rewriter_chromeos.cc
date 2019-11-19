// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/events/event_rewriter_chromeos.h"

#include <fcntl.h>
#include <stddef.h>

#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "device/udev_linux/scoped_udev.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/chromeos/events/modifier_key.h"
#include "ui/chromeos/events/pref_names.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ui {

namespace {

// Hotrod controller vendor/product ids.
const int kHotrodRemoteVendorId = 0x0471;
const int kHotrodRemoteProductId = 0x21cc;
const int kUnknownVendorId = -1;
const int kUnknownProductId = -1;

// Flag masks for remapping alt+click or search+click to right click.
constexpr int kAltLeftButton = (ui::EF_ALT_DOWN | ui::EF_LEFT_MOUSE_BUTTON);
constexpr int kSearchLeftButton =
    (ui::EF_COMMAND_DOWN | ui::EF_LEFT_MOUSE_BUTTON);

// Table of properties of remappable keys and/or remapping targets (not
// strictly limited to "modifiers").
//
// This is used in two distinct ways: for rewriting key up/down events,
// and for rewriting modifier EventFlags on any kind of event.
//
// For the first case, rewriting key up/down events, |RewriteModifierKeys()|
// determines the preference name |prefs::kLanguageRemap...KeyTo| for the
// incoming key and, using |GetRemappedKey()|, gets the user preference
// value |input_method::k...Key| for the incoming key, and finally finds that
// value in this table to obtain the |result| properties of the target key.
//
// For the second case, rewriting modifier EventFlags,
// |GetRemappedModifierMasks()| processes every table entry whose |flag|
// is set in the incoming event. Using the |pref_name| in the table entry,
// it likewise uses |GetRemappedKey()| to find the properties of the
// user preference target key, and replaces the flag accordingly.
const struct ModifierRemapping {
  int flag;
  ui::chromeos::ModifierKey remap_to;
  const char* pref_name;
  EventRewriterChromeOS::MutableKeyState result;
} kModifierRemappings[] = {
    {ui::EF_CONTROL_DOWN,
     ui::chromeos::ModifierKey::kControlKey,
     prefs::kLanguageRemapControlKeyTo,
     {ui::EF_CONTROL_DOWN, ui::DomCode::CONTROL_LEFT, ui::DomKey::CONTROL,
      ui::VKEY_CONTROL}},
    {// kModifierRemappingNeoMod3 references this entry by index.
     ui::EF_MOD3_DOWN | ui::EF_ALTGR_DOWN,
     ui::chromeos::ModifierKey::kNumModifierKeys,
     nullptr,
     {ui::EF_MOD3_DOWN | ui::EF_ALTGR_DOWN, ui::DomCode::CAPS_LOCK,
      ui::DomKey::ALT_GRAPH, ui::VKEY_ALTGR}},
    {ui::EF_COMMAND_DOWN,
     ui::chromeos::ModifierKey::kSearchKey,
     prefs::kLanguageRemapSearchKeyTo,
     {ui::EF_COMMAND_DOWN, ui::DomCode::META_LEFT, ui::DomKey::META,
      ui::VKEY_LWIN}},
    {ui::EF_ALT_DOWN,
     ui::chromeos::ModifierKey::kAltKey,
     prefs::kLanguageRemapAltKeyTo,
     {ui::EF_ALT_DOWN, ui::DomCode::ALT_LEFT, ui::DomKey::ALT, ui::VKEY_MENU}},
    {ui::EF_NONE,
     ui::chromeos::ModifierKey::kVoidKey,
     nullptr,
     {ui::EF_NONE, ui::DomCode::NONE, ui::DomKey::NONE, ui::VKEY_UNKNOWN}},
    {ui::EF_MOD3_DOWN,
     ui::chromeos::ModifierKey::kCapsLockKey,
     prefs::kLanguageRemapCapsLockKeyTo,
     {ui::EF_MOD3_DOWN, ui::DomCode::CAPS_LOCK, ui::DomKey::CAPS_LOCK,
      ui::VKEY_CAPITAL}},
    {ui::EF_NONE,
     ui::chromeos::ModifierKey::kEscapeKey,
     prefs::kLanguageRemapEscapeKeyTo,
     {ui::EF_NONE, ui::DomCode::ESCAPE, ui::DomKey::ESCAPE, ui::VKEY_ESCAPE}},
    {ui::EF_NONE,
     ui::chromeos::ModifierKey::kBackspaceKey,
     prefs::kLanguageRemapBackspaceKeyTo,
     {ui::EF_NONE, ui::DomCode::BACKSPACE, ui::DomKey::BACKSPACE,
      ui::VKEY_BACK}},
    {ui::EF_NONE,
     ui::chromeos::ModifierKey::kAssistantKey,
     prefs::kLanguageRemapAssistantKeyTo,
     {ui::EF_NONE, ui::DomCode::LAUNCH_ASSISTANT, ui::DomKey::LAUNCH_ASSISTANT,
      ui::VKEY_ASSISTANT}}};

const ModifierRemapping* kModifierRemappingNeoMod3 = &kModifierRemappings[1];

// Gets a remapped key for |pref_name| key. For example, to find out which
// key Ctrl is currently remapped to, call the function with
// prefs::kLanguageRemapControlKeyTo.
// Note: For the Search key, call GetSearchRemappedKey().
const ModifierRemapping* GetRemappedKey(
    const std::string& pref_name,
    EventRewriterChromeOS::Delegate* delegate) {
  if (!delegate)
    return nullptr;

  int value = -1;
  if (!delegate->GetKeyboardRemappedPrefValue(pref_name, &value))
    return nullptr;

  for (auto& remapping : kModifierRemappings) {
    if (value == static_cast<int>(remapping.remap_to))
      return &remapping;
  }

  return nullptr;
}

// Gets a remapped key for the Search key based on the |keyboard_type| of the
// last event. Internal Search key, Command key on external Apple keyboards, and
// Meta key (either Search or Windows) on external non-Apple keyboards can all
// be remapped separately.
const ModifierRemapping* GetSearchRemappedKey(
    EventRewriterChromeOS::Delegate* delegate,
    EventRewriterChromeOS::DeviceType keyboard_type) {
  std::string pref_name;
  switch (keyboard_type) {
    case EventRewriterChromeOS::kDeviceAppleKeyboard:
      pref_name = prefs::kLanguageRemapExternalCommandKeyTo;
      break;

    case EventRewriterChromeOS::kDeviceExternalNonAppleKeyboard:
    case EventRewriterChromeOS::kDeviceExternalUnknown:
      pref_name = prefs::kLanguageRemapExternalMetaKeyTo;
      break;

    default:
      // Use the preference for internal Search key remapping.
      pref_name = prefs::kLanguageRemapSearchKeyTo;
      break;
  }

  return GetRemappedKey(pref_name, delegate);
}

bool IsISOLevel5ShiftUsedByCurrentInputMethod() {
  // Since both German Neo2 XKB layout and Caps Lock depend on Mod3Mask,
  // it's not possible to make both features work. For now, we don't remap
  // Mod3Mask when Neo2 is in use.
  // TODO(yusukes): Remove the restriction.
  ::chromeos::input_method::InputMethodManager* manager =
      ::chromeos::input_method::InputMethodManager::Get();
  return manager->IsISOLevel5ShiftUsedByCurrentInputMethod();
}

struct KeyboardRemapping {
  // MatchKeyboardRemapping() succeeds if the tested has all of the specified
  // flags (and possibly other flags), and either the key_code matches or the
  // condition's key_code is VKEY_UNKNOWN.
  struct Condition {
    int flags;
    ui::KeyboardCode key_code;
  } condition;
  // ApplyRemapping(), which is the primary user of this structure,
  // conditionally sets the output fields from the |result| here.
  // - |dom_code| is set if |result.dom_code| is not NONE.
  // - |dom_key| and |character| are set if |result.dom_key| is not NONE.
  // -|key_code| is set if |result.key_code| is not VKEY_UNKNOWN.
  // - |flags| are always set from |result.flags|, but this can be |EF_NONE|.
  EventRewriterChromeOS::MutableKeyState result;
};

bool MatchKeyboardRemapping(
    const EventRewriterChromeOS::MutableKeyState& suspect,
    const KeyboardRemapping::Condition& test) {
  return ((test.key_code == ui::VKEY_UNKNOWN) ||
          (test.key_code == suspect.key_code)) &&
         ((suspect.flags & test.flags) == test.flags);
}

void ApplyRemapping(const EventRewriterChromeOS::MutableKeyState& changes,
                    EventRewriterChromeOS::MutableKeyState* state) {
  state->flags |= changes.flags;
  if (changes.code != ui::DomCode::NONE)
    state->code = changes.code;
  if (changes.key != ui::DomKey::NONE)
    state->key = changes.key;
  if (changes.key_code != ui::VKEY_UNKNOWN)
    state->key_code = changes.key_code;
}

// Given a set of KeyboardRemapping structs, finds a matching struct
// if possible, and updates the remapped event values. Returns true if a
// remapping was found and remapped values were updated.
bool RewriteWithKeyboardRemappings(
    const KeyboardRemapping* mappings,
    size_t num_mappings,
    const EventRewriterChromeOS::MutableKeyState& input_state,
    EventRewriterChromeOS::MutableKeyState* remapped_state) {
  for (size_t i = 0; i < num_mappings; ++i) {
    const KeyboardRemapping& map = mappings[i];
    if (MatchKeyboardRemapping(input_state, map.condition)) {
      remapped_state->flags = (input_state.flags & ~map.condition.flags);
      ApplyRemapping(map.result, remapped_state);
      return true;
    }
  }
  return false;
}

void SetMeaningForLayout(ui::EventType type,
                         EventRewriterChromeOS::MutableKeyState* state) {
  // Currently layout is applied by creating a temporary key event with the
  // current physical state, and extracting the layout results.
  ui::KeyEvent key(type, state->key_code, state->code, state->flags);
  state->key = key.GetDomKey();
}

ui::DomCode RelocateModifier(ui::DomCode code, ui::DomKeyLocation location) {
  bool right = (location == ui::DomKeyLocation::RIGHT);
  switch (code) {
    case ui::DomCode::CONTROL_LEFT:
    case ui::DomCode::CONTROL_RIGHT:
      return right ? ui::DomCode::CONTROL_RIGHT : ui::DomCode::CONTROL_LEFT;
    case ui::DomCode::SHIFT_LEFT:
    case ui::DomCode::SHIFT_RIGHT:
      return right ? ui::DomCode::SHIFT_RIGHT : ui::DomCode::SHIFT_LEFT;
    case ui::DomCode::ALT_LEFT:
    case ui::DomCode::ALT_RIGHT:
      return right ? ui::DomCode::ALT_RIGHT : ui::DomCode::ALT_LEFT;
    case ui::DomCode::META_LEFT:
    case ui::DomCode::META_RIGHT:
      return right ? ui::DomCode::META_RIGHT : ui::DomCode::META_LEFT;
    default:
      break;
  }
  return code;
}

// Returns true if |mouse_event| was generated from a touchpad device.
bool IsFromTouchpadDevice(const ui::MouseEvent& mouse_event) {
  for (const ui::InputDevice& touchpad :
       ui::DeviceDataManager::GetInstance()->GetTouchpadDevices()) {
    if (touchpad.id == mouse_event.source_device_id())
      return true;
  }

  return false;
}

// Returns true if |value| is replaced with the specific device property value
// without getting an error.
bool GetDeviceProperty(const base::FilePath& device_path,
                       const char* key,
                       std::string* value) {
  device::ScopedUdevPtr udev(device::udev_new());
  if (!udev.get())
    return false;

  device::ScopedUdevDevicePtr device(device::udev_device_new_from_syspath(
      udev.get(), device_path.value().c_str()));
  if (!device.get())
    return false;

  *value = device::UdevDeviceGetPropertyValue(device.get(), key);
  return true;
}

// Returns whether |key_code| appears as one of the key codes that might be
// remapped by table mappings.
bool IsKeyCodeInMappings(ui::KeyboardCode key_code,
                         const KeyboardRemapping* mappings,
                         size_t num_mappings) {
  for (size_t i = 0; i < num_mappings; ++i) {
    const KeyboardRemapping& map = mappings[i];
    if (key_code == map.condition.key_code) {
      return true;
    }
  }
  return false;
}

// Returns true if all bits in |flag_mask| are set in |flags|.
bool AreFlagsSet(int flags, int flag_mask) {
  return (flags & flag_mask) == flag_mask;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////

EventRewriterChromeOS::MutableKeyState::MutableKeyState()
    : MutableKeyState(0, ui::DomCode::NONE, 0, ui::KeyboardCode::VKEY_NONAME) {}

EventRewriterChromeOS::MutableKeyState::MutableKeyState(
    const ui::KeyEvent* key_event)
    : MutableKeyState(key_event->flags(),
                      key_event->code(),
                      key_event->GetDomKey(),
                      key_event->key_code()) {}

EventRewriterChromeOS::MutableKeyState::MutableKeyState(
    int input_flags,
    ui::DomCode input_code,
    ui::DomKey::Base input_key,
    ui::KeyboardCode input_key_code)
    : flags(input_flags),
      code(input_code),
      key(input_key),
      key_code(input_key_code) {}

///////////////////////////////////////////////////////////////////////////////

EventRewriterChromeOS::EventRewriterChromeOS(
    Delegate* delegate,
    ui::EventRewriter* sticky_keys_controller)
    : last_keyboard_device_id_(ui::ED_UNKNOWN_DEVICE),
      ime_keyboard_for_testing_(nullptr),
      delegate_(delegate),
      sticky_keys_controller_(sticky_keys_controller),
      pressed_modifier_latches_(ui::EF_NONE),
      latched_modifier_latches_(ui::EF_NONE),
      used_modifier_latches_(ui::EF_NONE) {}

EventRewriterChromeOS::~EventRewriterChromeOS() {}

// static
EventRewriterChromeOS::DeviceType EventRewriterChromeOS::GetDeviceType(
    const ui::InputDevice& keyboard_device) {
  if (keyboard_device.vendor_id == kHotrodRemoteVendorId &&
      keyboard_device.product_id == kHotrodRemoteProductId) {
    return EventRewriterChromeOS::kDeviceHotrodRemote;
  }

  if (base::LowerCaseEqualsASCII(keyboard_device.name,
                                 "virtual core keyboard")) {
    return EventRewriterChromeOS::kDeviceVirtualCoreKeyboard;
  }

  const std::vector<std::string> tokens =
      base::SplitString(keyboard_device.name, " .", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  // If the |device_name| contains the two words, "apple" and "keyboard", treat
  // it as an Apple keyboard.
  bool found_apple = false;
  bool found_keyboard = false;
  for (size_t i = 0; i < tokens.size(); ++i) {
    if (!found_apple && base::LowerCaseEqualsASCII(tokens[i], "apple"))
      found_apple = true;
    if (!found_keyboard && base::LowerCaseEqualsASCII(tokens[i], "keyboard"))
      found_keyboard = true;
    if (found_apple && found_keyboard)
      return EventRewriterChromeOS::kDeviceAppleKeyboard;
  }

  if (!found_apple && (keyboard_device.type == INPUT_DEVICE_USB ||
                       keyboard_device.type == INPUT_DEVICE_BLUETOOTH)) {
    // ui::InputDevice is a generic input device, and we're not sure if it's
    // actually a keyboard.
    return found_keyboard
               ? EventRewriterChromeOS::kDeviceExternalNonAppleKeyboard
               : EventRewriterChromeOS::kDeviceExternalUnknown;
  }

  return EventRewriterChromeOS::kDeviceUnknown;
}

void EventRewriterChromeOS::KeyboardDeviceAddedForTesting(
    int device_id,
    const std::string& device_name,
    KeyboardTopRowLayout layout,
    InputDeviceType device_type) {
  // Tests must avoid XI2 reserved device IDs.
  DCHECK((device_id < 0) || (device_id > 1));
  InputDevice keyboard_device(device_id, device_type, device_name);
  keyboard_device.vendor_id = kUnknownVendorId;
  keyboard_device.product_id = kUnknownProductId;
  KeyboardDeviceAddedInternal(device_id, GetDeviceType(keyboard_device),
                              layout);
}

void EventRewriterChromeOS::RewriteMouseButtonEventForTesting(
    const ui::MouseEvent& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  RewriteMouseButtonEvent(event, rewritten_event);
}

ui::EventRewriteStatus EventRewriterChromeOS::RewriteEvent(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  if ((event.type() == ui::ET_KEY_PRESSED) ||
      (event.type() == ui::ET_KEY_RELEASED)) {
    ui::EventRewriteStatus status =
        RewriteKeyEvent(*((&event)->AsKeyEvent()), rewritten_event);
    RewriteKeyEventInContext(*((&event)->AsKeyEvent()), rewritten_event,
                             &status);
    return status;
  }
  if ((event.type() == ui::ET_MOUSE_PRESSED) ||
      (event.type() == ui::ET_MOUSE_RELEASED)) {
    return RewriteMouseButtonEvent(static_cast<const ui::MouseEvent&>(event),
                                   rewritten_event);
  }
  if (event.type() == ui::ET_MOUSEWHEEL) {
    return RewriteMouseWheelEvent(
        static_cast<const ui::MouseWheelEvent&>(event), rewritten_event);
  }
  if ((event.type() == ui::ET_TOUCH_PRESSED) ||
      (event.type() == ui::ET_TOUCH_RELEASED)) {
    return RewriteTouchEvent(static_cast<const ui::TouchEvent&>(event),
                             rewritten_event);
  }
  if (event.IsScrollEvent()) {
    return RewriteScrollEvent(static_cast<const ui::ScrollEvent&>(event),
                              rewritten_event);
  }
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus EventRewriterChromeOS::NextDispatchEvent(
    const ui::Event& last_event,
    std::unique_ptr<ui::Event>* new_event) {
  if (dispatched_key_events_.size()) {
    *new_event = std::move(dispatched_key_events_.back());
    dispatched_key_events_.pop_back();

    if (dispatched_key_events_.size())
      return ui::EVENT_REWRITE_DISPATCH_ANOTHER;
    return ui::EVENT_REWRITE_REWRITTEN;
  }

  if (sticky_keys_controller_) {
    // In the case of sticky keys, we know what the events obtained here are:
    // modifier key releases that match the ones previously discarded. So, we
    // know that they don't have to be passed through the post-sticky key
    // rewriting phases, |RewriteExtendedKeys()| and |RewriteFunctionKeys()|,
    // because those phases do nothing with modifier key releases.
    return sticky_keys_controller_->NextDispatchEvent(last_event, new_event);
  }
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

void EventRewriterChromeOS::BuildRewrittenKeyEvent(
    const ui::KeyEvent& key_event,
    const MutableKeyState& state,
    std::unique_ptr<ui::Event>* rewritten_event) {
  ui::KeyEvent* rewritten_key_event =
      new ui::KeyEvent(key_event.type(), state.key_code, state.code,
                       state.flags, state.key, key_event.time_stamp());
  rewritten_event->reset(rewritten_key_event);
}

// static
bool EventRewriterChromeOS::GetKeyboardTopRowLayout(
    const base::FilePath& device_path,
    KeyboardTopRowLayout* out_layout) {
  const char kLayoutProperty[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
  std::string layout;
  if (!GetDeviceProperty(device_path, kLayoutProperty, &layout))
    return false;

  if (layout.empty()) {
    *out_layout = EventRewriterChromeOS::kKbdTopRowLayoutDefault;
    return true;
  }

  int layout_id;
  if (!base::StringToInt(layout, &layout_id)) {
    LOG(WARNING) << "Failed to parse " << kLayoutProperty << " value '"
                 << layout << "'";
    return false;
  }
  if (layout_id < EventRewriterChromeOS::kKbdTopRowLayoutMin ||
      layout_id > EventRewriterChromeOS::kKbdTopRowLayoutMax) {
    LOG(WARNING) << "Invalid " << kLayoutProperty << " '" << layout << "'";
    return false;
  }
  *out_layout =
      static_cast<EventRewriterChromeOS::KeyboardTopRowLayout>(layout_id);
  return true;
}

bool EventRewriterChromeOS::HasAssistantKeyOnKeyboard(
    const base::FilePath& device_path,
    bool* has_assistant_key) {
  const char kDevNameProperty[] = "DEVNAME";
  std::string dev_name;
  if (!GetDeviceProperty(device_path, kDevNameProperty, &dev_name))
    return false;

  base::ScopedFD fd(open(dev_name.c_str(), O_RDONLY));
  if (fd.get() < 0) {
    LOG(ERROR) << "Cannot open " << dev_name.c_str() << " : " << errno;
    return false;
  }

  ui::EventDeviceInfo devinfo;
  if (!devinfo.Initialize(fd.get(), device_path)) {
    LOG(ERROR) << "Failed to get device information for "
               << device_path.value();
    return false;
  }

  *has_assistant_key = devinfo.HasKeyEvent(KEY_ASSISTANT);
  return true;
}

bool EventRewriterChromeOS::RewriteModifierKeys(const ui::KeyEvent& key_event,
                                                MutableKeyState* state) {
  DCHECK(key_event.type() == ui::ET_KEY_PRESSED ||
         key_event.type() == ui::ET_KEY_RELEASED);

  if (!delegate_ || !delegate_->RewriteModifierKeys())
    return false;

  // Preserve a copy of the original before rewriting |state| based on
  // user preferences, device configuration, and certain IME properties.
  MutableKeyState incoming = *state;
  state->flags = ui::EF_NONE;
  int characteristic_flag = ui::EF_NONE;
  bool exact_event = false;

  // First, remap the key code.
  const ModifierRemapping* remapped_key = nullptr;
  // Remapping based on DomKey.
  switch (incoming.key) {
    case ui::DomKey::ALT_GRAPH:
      // The Neo2 codes modifiers such that CapsLock appears as VKEY_ALTGR,
      // but AltGraph (right Alt) also appears as VKEY_ALTGR in Neo2,
      // as it does in other layouts. Neo2's "Mod3" is represented in
      // EventFlags by a combination of AltGr+Mod3, while its "Mod4" is
      // AltGr alone.
      if (IsISOLevel5ShiftUsedByCurrentInputMethod()) {
        if (incoming.code == ui::DomCode::CAPS_LOCK) {
          characteristic_flag = ui::EF_ALTGR_DOWN | ui::EF_MOD3_DOWN;
          remapped_key =
              GetRemappedKey(prefs::kLanguageRemapCapsLockKeyTo, delegate_);
        } else {
          characteristic_flag = ui::EF_ALTGR_DOWN;
          remapped_key = GetSearchRemappedKey(delegate_, GetLastKeyboardType());
        }
      }
      if (remapped_key && remapped_key->result.key_code == ui::VKEY_CAPITAL)
        remapped_key = kModifierRemappingNeoMod3;
      break;
    case ui::DomKey::ALT_GRAPH_LATCH:
      if (key_event.type() == ui::ET_KEY_PRESSED) {
        pressed_modifier_latches_ |= ui::EF_ALTGR_DOWN;
      } else {
        pressed_modifier_latches_ &= ~ui::EF_ALTGR_DOWN;
        if (used_modifier_latches_ & ui::EF_ALTGR_DOWN)
          used_modifier_latches_ &= ~ui::EF_ALTGR_DOWN;
        else
          latched_modifier_latches_ |= ui::EF_ALTGR_DOWN;
      }
      // Rewrite to AltGraph. When this key is used like a regular modifier,
      // the web-exposed result looks like a use of the regular modifier.
      // When it's used as a latch, the web-exposed result is a vacuous
      // modifier press-and-release, which should be harmless, but preserves
      // the event for applications using the |code| (e.g. remoting).
      state->key = ui::DomKey::ALT_GRAPH;
      state->key_code = ui::VKEY_ALTGR;
      exact_event = true;
      break;
    default:
      break;
  }

  // Remapping based on DomCode.
  switch (incoming.code) {
    // On Chrome OS, Caps_Lock with Mod3Mask is sent when Caps Lock is pressed
    // (with one exception: when IsISOLevel5ShiftUsedByCurrentInputMethod() is
    // true, the key generates XK_ISO_Level3_Shift with Mod3Mask, not
    // Caps_Lock).
    case ui::DomCode::CAPS_LOCK:
      // This key is already remapped to Mod3 in remapping based on DomKey. Skip
      // more remapping.
      if (IsISOLevel5ShiftUsedByCurrentInputMethod() && remapped_key)
        break;

      characteristic_flag = ui::EF_CAPS_LOCK_ON;
      remapped_key =
          GetRemappedKey(prefs::kLanguageRemapCapsLockKeyTo, delegate_);
      break;
    case ui::DomCode::META_LEFT:
    case ui::DomCode::META_RIGHT:
      characteristic_flag = ui::EF_COMMAND_DOWN;
      remapped_key = GetSearchRemappedKey(delegate_, GetLastKeyboardType());
      // Default behavior is Super key, hence don't remap the event if the pref
      // is unavailable.
      break;
    case ui::DomCode::CONTROL_LEFT:
    case ui::DomCode::CONTROL_RIGHT:
      characteristic_flag = ui::EF_CONTROL_DOWN;
      remapped_key =
          GetRemappedKey(prefs::kLanguageRemapControlKeyTo, delegate_);
      break;
    case ui::DomCode::ALT_LEFT:
    case ui::DomCode::ALT_RIGHT:
      // ALT key
      characteristic_flag = ui::EF_ALT_DOWN;
      remapped_key = GetRemappedKey(prefs::kLanguageRemapAltKeyTo, delegate_);
      break;
    case ui::DomCode::ESCAPE:
      remapped_key =
          GetRemappedKey(prefs::kLanguageRemapEscapeKeyTo, delegate_);
      break;
    case ui::DomCode::BACKSPACE:
      remapped_key =
          GetRemappedKey(prefs::kLanguageRemapBackspaceKeyTo, delegate_);
      break;
    case ui::DomCode::LAUNCH_ASSISTANT:
      remapped_key =
          GetRemappedKey(prefs::kLanguageRemapAssistantKeyTo, delegate_);
      break;
    default:
      break;
  }

  if (remapped_key) {
    state->key_code = remapped_key->result.key_code;
    state->code = remapped_key->result.code;
    state->key = remapped_key->result.key;
    incoming.flags |= characteristic_flag;
    characteristic_flag = remapped_key->flag;
    if (incoming.key_code == ui::VKEY_CAPITAL) {
      // Caps Lock is rewritten to another key event, remove EF_CAPS_LOCK_ON
      // flag to prevent the keyboard's Caps Lock state being synced to the
      // rewritten key event's flag in InputMethodChromeOS.
      incoming.flags &= ~ui::EF_CAPS_LOCK_ON;
    }
    if (remapped_key->remap_to == ui::chromeos::ModifierKey::kCapsLockKey)
      characteristic_flag |= ui::EF_CAPS_LOCK_ON;
    state->code = RelocateModifier(
        state->code, ui::KeycodeConverter::DomCodeToLocation(incoming.code));
  }

  // Next, remap modifier bits.
  state->flags |= GetRemappedModifierMasks(key_event, incoming.flags);

  // If the DomKey is not a modifier before remapping but is after, set the
  // modifier latches for the later non-modifier key's modifier states.
  bool non_modifier_to_modifier =
      !ui::KeycodeConverter::IsDomKeyForModifier(incoming.key) &&
      ui::KeycodeConverter::IsDomKeyForModifier(state->key);
  if (key_event.type() == ui::ET_KEY_PRESSED) {
    state->flags |= characteristic_flag;
    if (non_modifier_to_modifier)
      pressed_modifier_latches_ |= characteristic_flag;
  } else {
    state->flags &= ~characteristic_flag;
    if (non_modifier_to_modifier)
      pressed_modifier_latches_ &= ~characteristic_flag;
  }

  if (key_event.type() == ui::ET_KEY_PRESSED) {
    if (!ui::KeycodeConverter::IsDomKeyForModifier(state->key)) {
      used_modifier_latches_ |= pressed_modifier_latches_;
      latched_modifier_latches_ = ui::EF_NONE;
    }
  }

  // Implement the Caps Lock modifier here, rather than in the
  // AcceleratorController, so that the event is visible to apps (see
  // crbug.com/775743).
  if (key_event.type() == ui::ET_KEY_RELEASED &&
      state->key_code == ui::VKEY_CAPITAL) {
    ::chromeos::input_method::ImeKeyboard* ime_keyboard =
        ime_keyboard_for_testing_
            ? ime_keyboard_for_testing_
            : ::chromeos::input_method::InputMethodManager::Get()
                  ->GetImeKeyboard();
    ime_keyboard->SetCapsLockEnabled(!ime_keyboard->CapsLockIsEnabled());
  }
  return exact_event;
}

void EventRewriterChromeOS::DeviceKeyPressedOrReleased(int device_id) {
  const auto iter = device_id_to_info_.find(device_id);
  DeviceType type;
  if (iter != device_id_to_info_.end())
    type = iter->second.type;
  else
    type = KeyboardDeviceAdded(device_id);

  // Ignore virtual Xorg keyboard (magic that generates key repeat
  // events). Pretend that the previous real keyboard is the one that is still
  // in use.
  if (type == kDeviceVirtualCoreKeyboard)
    return;

  last_keyboard_device_id_ = device_id;
}

bool EventRewriterChromeOS::IsHotrodRemote() const {
  return IsLastKeyboardOfType(kDeviceHotrodRemote);
}

bool EventRewriterChromeOS::IsLastKeyboardOfType(DeviceType device_type) const {
  return GetLastKeyboardType() == device_type;
}

EventRewriterChromeOS::DeviceType EventRewriterChromeOS::GetLastKeyboardType()
    const {
  if (last_keyboard_device_id_ == ui::ED_UNKNOWN_DEVICE)
    return kDeviceUnknown;

  const auto iter = device_id_to_info_.find(last_keyboard_device_id_);
  if (iter == device_id_to_info_.end()) {
    LOG(ERROR) << "Device ID " << last_keyboard_device_id_ << " is unknown.";
    return kDeviceUnknown;
  }

  return iter->second.type;
}

int EventRewriterChromeOS::GetRemappedModifierMasks(const ui::Event& event,
                                                    int original_flags) const {
  int unmodified_flags = original_flags;
  int rewritten_flags = pressed_modifier_latches_ | latched_modifier_latches_;
  for (size_t i = 0; unmodified_flags && (i < base::size(kModifierRemappings));
       ++i) {
    const ModifierRemapping* remapped_key = nullptr;
    if (!(unmodified_flags & kModifierRemappings[i].flag))
      continue;
    switch (kModifierRemappings[i].flag) {
      case ui::EF_COMMAND_DOWN:
        remapped_key = GetSearchRemappedKey(delegate_, GetLastKeyboardType());
        break;
      case ui::EF_MOD3_DOWN:
        // If EF_MOD3_DOWN is used by the current input method, leave it alone;
        // it is not remappable.
        if (IsISOLevel5ShiftUsedByCurrentInputMethod())
          continue;
        // Otherwise, Mod3Mask is set on X events when the Caps Lock key
        // is down, but, if Caps Lock is remapped, CapsLock is NOT set,
        // because pressing the key does not invoke caps lock. So, the
        // kModifierRemappings[] table uses EF_MOD3_DOWN for the Caps
        // Lock remapping.
        break;
      case ui::EF_MOD3_DOWN | ui::EF_ALTGR_DOWN:
        if ((original_flags & ui::EF_ALTGR_DOWN) &&
            IsISOLevel5ShiftUsedByCurrentInputMethod()) {
          remapped_key = kModifierRemappingNeoMod3;
        }
        break;
      default:
        break;
    }
    if (!remapped_key && kModifierRemappings[i].pref_name) {
      remapped_key =
          GetRemappedKey(kModifierRemappings[i].pref_name, delegate_);
    }
    if (remapped_key) {
      unmodified_flags &= ~kModifierRemappings[i].flag;
      rewritten_flags |= remapped_key->flag;
    }
  }
  return rewritten_flags | unmodified_flags;
}

bool EventRewriterChromeOS::ShouldRemapToRightClick(
    const ui::MouseEvent& mouse_event,
    int flags,
    int* matched_mask) const {
  *matched_mask = 0;
  if (base::FeatureList::IsEnabled(
          ::chromeos::features::kUseSearchClickForRightClick)) {
    if (AreFlagsSet(flags, kSearchLeftButton)) {
      *matched_mask = kSearchLeftButton;
    }
  } else {
    if (AreFlagsSet(flags, kAltLeftButton)) {
      *matched_mask = kAltLeftButton;
    }
  }

  return (*matched_mask != 0) &&
         ((mouse_event.type() == ui::ET_MOUSE_PRESSED) ||
          pressed_device_ids_.count(mouse_event.source_device_id())) &&
         IsFromTouchpadDevice(mouse_event);
}

ui::EventRewriteStatus EventRewriterChromeOS::RewriteKeyEvent(
    const ui::KeyEvent& key_event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  if (delegate_ && delegate_->IsExtensionCommandRegistered(key_event.key_code(),
                                                           key_event.flags()))
    return ui::EVENT_REWRITE_CONTINUE;
  if (key_event.source_device_id() != ui::ED_UNKNOWN_DEVICE)
    DeviceKeyPressedOrReleased(key_event.source_device_id());

  // Drop repeated keys from Hotrod remote.
  if ((key_event.flags() & ui::EF_IS_REPEAT) &&
      (key_event.type() == ui::ET_KEY_PRESSED) && IsHotrodRemote() &&
      key_event.key_code() != ui::VKEY_BACK) {
    return ui::EVENT_REWRITE_DISCARD;
  }

  MutableKeyState state = {key_event.flags(), key_event.code(),
                           key_event.GetDomKey(), key_event.key_code()};

  // Do not rewrite an event sent by ui_controls::SendKeyPress(). See
  // crbug.com/136465.
  if (!(key_event.flags() & ui::EF_FINAL)) {
    if (RewriteModifierKeys(key_event, &state)) {
      // Early exit with completed event.
      BuildRewrittenKeyEvent(key_event, state, rewritten_event);
      return ui::EVENT_REWRITE_REWRITTEN;
    }
    RewriteNumPadKeys(key_event, &state);
  }

  ui::EventRewriteStatus status = ui::EVENT_REWRITE_CONTINUE;
  bool is_sticky_key_extension_command = false;
  if (sticky_keys_controller_) {
    auto tmp_event = key_event;
    tmp_event.set_key_code(state.key_code);
    tmp_event.set_flags(state.flags);
    std::unique_ptr<ui::Event> output_event;
    status = sticky_keys_controller_->RewriteEvent(tmp_event, &output_event);
    if (status == ui::EVENT_REWRITE_REWRITTEN ||
        status == ui::EVENT_REWRITE_DISPATCH_ANOTHER)
      state.flags = output_event->flags();
    if (status == ui::EVENT_REWRITE_DISCARD)
      return ui::EVENT_REWRITE_DISCARD;
    is_sticky_key_extension_command =
        delegate_ &&
        delegate_->IsExtensionCommandRegistered(state.key_code, state.flags);
  }

  // If flags have changed, this may change the interpretation of the key,
  // so reapply layout.
  if (state.flags != key_event.flags())
    SetMeaningForLayout(key_event.type(), &state);

  // If sticky key rewrites the event, and it matches an extension command, do
  // not further rewrite the event since it won't match the extension command
  // thereafter.
  if (!is_sticky_key_extension_command && !(key_event.flags() & ui::EF_FINAL)) {
    RewriteExtendedKeys(key_event, &state);
    RewriteFunctionKeys(key_event, &state);
  }
  if ((key_event.flags() == state.flags) &&
      (key_event.key_code() == state.key_code) &&
      (status == ui::EVENT_REWRITE_CONTINUE)) {
    return ui::EVENT_REWRITE_CONTINUE;
  }
  // Sticky keys may have returned a result other than |EVENT_REWRITE_CONTINUE|,
  // in which case we need to preserve that return status. Alternatively, we
  // might be here because key_event changed, in which case we need to
  // return |EVENT_REWRITE_REWRITTEN|.
  if (status == ui::EVENT_REWRITE_CONTINUE)
    status = ui::EVENT_REWRITE_REWRITTEN;
  BuildRewrittenKeyEvent(key_event, state, rewritten_event);
  return status;
}

ui::EventRewriteStatus EventRewriterChromeOS::RewriteMouseButtonEvent(
    const ui::MouseEvent& mouse_event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  int flags = mouse_event.flags();
  RewriteLocatedEvent(mouse_event, &flags);
  ui::EventRewriteStatus status = ui::EVENT_REWRITE_CONTINUE;
  if (sticky_keys_controller_) {
    auto tmp_event = mouse_event;
    tmp_event.set_flags(flags);
    std::unique_ptr<ui::Event> output_event;
    status = sticky_keys_controller_->RewriteEvent(tmp_event, &output_event);
    if (status == ui::EVENT_REWRITE_REWRITTEN ||
        status == ui::EVENT_REWRITE_DISPATCH_ANOTHER)
      flags = output_event->flags();
  }
  int changed_button = ui::EF_NONE;
  if ((mouse_event.type() == ui::ET_MOUSE_PRESSED) ||
      (mouse_event.type() == ui::ET_MOUSE_RELEASED)) {
    changed_button = RewriteModifierClick(mouse_event, &flags);
  }
  if ((mouse_event.flags() == flags) &&
      (status == ui::EVENT_REWRITE_CONTINUE)) {
    return ui::EVENT_REWRITE_CONTINUE;
  }
  if (status == ui::EVENT_REWRITE_CONTINUE)
    status = ui::EVENT_REWRITE_REWRITTEN;
  ui::MouseEvent* rewritten_mouse_event = new ui::MouseEvent(mouse_event);
  rewritten_event->reset(rewritten_mouse_event);
  rewritten_mouse_event->set_flags(flags);
  if (changed_button != ui::EF_NONE)
    rewritten_mouse_event->set_changed_button_flags(changed_button);
  return status;
}

ui::EventRewriteStatus EventRewriterChromeOS::RewriteMouseWheelEvent(
    const ui::MouseWheelEvent& wheel_event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  if (!sticky_keys_controller_)
    return ui::EVENT_REWRITE_CONTINUE;
  int flags = wheel_event.flags();
  RewriteLocatedEvent(wheel_event, &flags);
  auto tmp_event = wheel_event;
  tmp_event.set_flags(flags);
  ui::EventRewriteStatus status =
      sticky_keys_controller_->RewriteEvent(tmp_event, rewritten_event);

  switch (status) {
    case ui::EVENT_REWRITE_REWRITTEN:
    case ui::EVENT_REWRITE_DISPATCH_ANOTHER:
      // whell event has been rewritten and stored in |rewritten_event|.
      break;
    case ui::EVENT_REWRITE_CONTINUE:
      if (flags != wheel_event.flags()) {
        *rewritten_event = std::make_unique<ui::MouseWheelEvent>(wheel_event);
        (*rewritten_event)->set_flags(flags);
        status = ui::EVENT_REWRITE_REWRITTEN;
      }
      break;
    case ui::EVENT_REWRITE_DISCARD:
      NOTREACHED();
      break;
  }

  return status;
}

ui::EventRewriteStatus EventRewriterChromeOS::RewriteTouchEvent(
    const ui::TouchEvent& touch_event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  int flags = touch_event.flags();
  RewriteLocatedEvent(touch_event, &flags);
  if (touch_event.flags() == flags)
    return ui::EVENT_REWRITE_CONTINUE;
  ui::TouchEvent* rewritten_touch_event = new ui::TouchEvent(touch_event);
  rewritten_event->reset(rewritten_touch_event);
  rewritten_touch_event->set_flags(flags);
  return ui::EVENT_REWRITE_REWRITTEN;
}

ui::EventRewriteStatus EventRewriterChromeOS::RewriteScrollEvent(
    const ui::ScrollEvent& scroll_event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  if (!sticky_keys_controller_)
    return ui::EVENT_REWRITE_CONTINUE;
  ui::EventRewriteStatus status =
      sticky_keys_controller_->RewriteEvent(scroll_event, rewritten_event);
  // Scroll event shouldn't be discarded.
  DCHECK_NE(status, ui::EVENT_REWRITE_DISCARD);
  return status;
}

void EventRewriterChromeOS::RewriteNumPadKeys(const ui::KeyEvent& key_event,
                                              MutableKeyState* state) {
  DCHECK(key_event.type() == ui::ET_KEY_PRESSED ||
         key_event.type() == ui::ET_KEY_RELEASED);
  static const struct NumPadRemapping {
    ui::KeyboardCode input_key_code;
    MutableKeyState result;
  } kNumPadRemappings[] = {
      {ui::VKEY_DELETE,
       {ui::EF_NONE, ui::DomCode::NONE, ui::DomKey::Constant<'.'>::Character,
        ui::VKEY_DECIMAL}},
      {ui::VKEY_INSERT,
       {ui::EF_NONE, ui::DomCode::NONE, ui::DomKey::Constant<'0'>::Character,
        ui::VKEY_NUMPAD0}},
      {ui::VKEY_END,
       {ui::EF_NONE, ui::DomCode::NONE, ui::DomKey::Constant<'1'>::Character,
        ui::VKEY_NUMPAD1}},
      {ui::VKEY_DOWN,
       {ui::EF_NONE, ui::DomCode::NONE, ui::DomKey::Constant<'2'>::Character,
        ui::VKEY_NUMPAD2}},
      {ui::VKEY_NEXT,
       {ui::EF_NONE, ui::DomCode::NONE, ui::DomKey::Constant<'3'>::Character,
        ui::VKEY_NUMPAD3}},
      {ui::VKEY_LEFT,
       {ui::EF_NONE, ui::DomCode::NONE, ui::DomKey::Constant<'4'>::Character,
        ui::VKEY_NUMPAD4}},
      {ui::VKEY_CLEAR,
       {ui::EF_NONE, ui::DomCode::NONE, ui::DomKey::Constant<'5'>::Character,
        ui::VKEY_NUMPAD5}},
      {ui::VKEY_RIGHT,
       {ui::EF_NONE, ui::DomCode::NONE, ui::DomKey::Constant<'6'>::Character,
        ui::VKEY_NUMPAD6}},
      {ui::VKEY_HOME,
       {ui::EF_NONE, ui::DomCode::NONE, ui::DomKey::Constant<'7'>::Character,
        ui::VKEY_NUMPAD7}},
      {ui::VKEY_UP,
       {ui::EF_NONE, ui::DomCode::NONE, ui::DomKey::Constant<'8'>::Character,
        ui::VKEY_NUMPAD8}},
      {ui::VKEY_PRIOR,
       {ui::EF_NONE, ui::DomCode::NONE, ui::DomKey::Constant<'9'>::Character,
        ui::VKEY_NUMPAD9}}};
  for (const auto& map : kNumPadRemappings) {
    if (state->key_code == map.input_key_code) {
      if (ui::KeycodeConverter::DomCodeToLocation(state->code) ==
          ui::DomKeyLocation::NUMPAD) {
        ApplyRemapping(map.result, state);
      }
      return;
    }
  }
}

void EventRewriterChromeOS::RewriteExtendedKeys(const ui::KeyEvent& key_event,
                                                MutableKeyState* state) {
  DCHECK(key_event.type() == ui::ET_KEY_PRESSED ||
         key_event.type() == ui::ET_KEY_RELEASED);
  MutableKeyState incoming = *state;

  if ((incoming.flags & (ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN)) ==
      (ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN)) {
    // Allow Search to avoid rewriting extended keys.
    // For these, we only remove the EF_COMMAND_DOWN flag.
    static const KeyboardRemapping::Condition kAvoidRemappings[] = {
        {// Alt+Backspace
         ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::VKEY_BACK},
        {// Control+Alt+Up
         ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN,
         ui::VKEY_UP},
        {// Control+Alt+Down
         ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN,
         ui::VKEY_DOWN}};
    for (const auto& condition : kAvoidRemappings) {
      if (MatchKeyboardRemapping(*state, condition)) {
        state->flags = incoming.flags & ~ui::EF_COMMAND_DOWN;
        return;
      }
    }
  }

  if (incoming.flags & ui::EF_COMMAND_DOWN) {
    static const KeyboardRemapping kSearchRemappings[] = {
        {// Search+BackSpace -> Delete
         {ui::EF_COMMAND_DOWN, ui::VKEY_BACK},
         {ui::EF_NONE, ui::DomCode::DEL, ui::DomKey::DEL, ui::VKEY_DELETE}},
        {// Search+Left -> Home
         {ui::EF_COMMAND_DOWN, ui::VKEY_LEFT},
         {ui::EF_NONE, ui::DomCode::HOME, ui::DomKey::HOME, ui::VKEY_HOME}},
        {// Search+Up -> Prior (aka PageUp)
         {ui::EF_COMMAND_DOWN, ui::VKEY_UP},
         {ui::EF_NONE, ui::DomCode::PAGE_UP, ui::DomKey::PAGE_UP,
          ui::VKEY_PRIOR}},
        {// Search+Right -> End
         {ui::EF_COMMAND_DOWN, ui::VKEY_RIGHT},
         {ui::EF_NONE, ui::DomCode::END, ui::DomKey::END, ui::VKEY_END}},
        {// Search+Down -> Next (aka PageDown)
         {ui::EF_COMMAND_DOWN, ui::VKEY_DOWN},
         {ui::EF_NONE, ui::DomCode::PAGE_DOWN, ui::DomKey::PAGE_DOWN,
          ui::VKEY_NEXT}},
        {// Search+Period -> Insert
         {ui::EF_COMMAND_DOWN, ui::VKEY_OEM_PERIOD},
         {ui::EF_NONE, ui::DomCode::INSERT, ui::DomKey::INSERT,
          ui::VKEY_INSERT}}};
    bool skip_search_key_remapping =
        delegate_ && delegate_->IsSearchKeyAcceleratorReserved();
    if (!skip_search_key_remapping &&
        RewriteWithKeyboardRemappings(kSearchRemappings,
                                      base::size(kSearchRemappings), incoming,
                                      state)) {
      return;
    }
  }

  if (incoming.flags & ui::EF_ALT_DOWN) {
    static const KeyboardRemapping kNonSearchRemappings[] = {
        {// Alt+BackSpace -> Delete
         {ui::EF_ALT_DOWN, ui::VKEY_BACK},
         {ui::EF_NONE, ui::DomCode::DEL, ui::DomKey::DEL, ui::VKEY_DELETE}},
        {// Control+Alt+Up -> Home
         {ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::VKEY_UP},
         {ui::EF_NONE, ui::DomCode::HOME, ui::DomKey::HOME, ui::VKEY_HOME}},
        {// Alt+Up -> Prior (aka PageUp)
         {ui::EF_ALT_DOWN, ui::VKEY_UP},
         {ui::EF_NONE, ui::DomCode::PAGE_UP, ui::DomKey::PAGE_UP,
          ui::VKEY_PRIOR}},
        {// Control+Alt+Down -> End
         {ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::VKEY_DOWN},
         {ui::EF_NONE, ui::DomCode::END, ui::DomKey::END, ui::VKEY_END}},
        {// Alt+Down -> Next (aka PageDown)
         {ui::EF_ALT_DOWN, ui::VKEY_DOWN},
         {ui::EF_NONE, ui::DomCode::PAGE_DOWN, ui::DomKey::PAGE_DOWN,
          ui::VKEY_NEXT}}};
    if (RewriteWithKeyboardRemappings(kNonSearchRemappings,
                                      base::size(kNonSearchRemappings),
                                      incoming, state)) {
      return;
    }
  }
}

void EventRewriterChromeOS::RewriteFunctionKeys(const ui::KeyEvent& key_event,
                                                MutableKeyState* state) {
  CHECK(key_event.type() == ui::ET_KEY_PRESSED ||
        key_event.type() == ui::ET_KEY_RELEASED);

  // Some action key codes are mapped to standard VKEY and DomCode values
  // during event to KeyEvent translation. However, in Chrome, different VKEY
  // combinations trigger those actions. This table maps event VKEYs to the
  // right action VKEYs.
  // TODO(dtor): Either add proper accelerators for VKEY_ZOOM or move
  // from VKEY_MEDIA_LAUNCH_APP2 to VKEY_ZOOM.
  static const KeyboardRemapping kActionToActionKeys[] = {
      // Zoom toggle is actually through VKEY_MEDIA_LAUNCH_APP2.
      {{ui::EF_NONE, ui::VKEY_ZOOM},
       {ui::EF_NONE, ui::DomCode::ZOOM_TOGGLE, ui::DomKey::ZOOM_TOGGLE,
        ui::VKEY_MEDIA_LAUNCH_APP2}},
  };

  // Map certain action keys to the right VKey and modifier.
  RewriteWithKeyboardRemappings(kActionToActionKeys,
                                base::size(kActionToActionKeys), *state, state);

  // Some key codes have a Dom code but no VKEY value assigned. They're mapped
  // to VKEY values here.
  if (state->key_code == ui::VKEY_UNKNOWN) {
    if (state->code == ui::DomCode::SHOW_ALL_WINDOWS) {
      // Show all windows is through VKEY_MEDIA_LAUNCH_APP1.
      state->key_code = ui::VKEY_MEDIA_LAUNCH_APP1;
      state->key = ui::DomKey::F4;
    } else if (state->code == ui::DomCode::DISPLAY_TOGGLE_INT_EXT) {
      // Display toggle is through control + VKEY_MEDIA_LAUNCH_APP2.
      state->flags |= ui::EF_CONTROL_DOWN;
      state->key_code = ui::VKEY_MEDIA_LAUNCH_APP2;
      state->key = ui::DomKey::F12;
    }
  }

  const auto iter = device_id_to_info_.find(key_event.source_device_id());
  KeyboardTopRowLayout layout = kKbdTopRowLayoutDefault;
  if (iter != device_id_to_info_.end()) {
    layout = iter->second.top_row_layout;
  }

  const bool search_is_pressed = (state->flags & ui::EF_COMMAND_DOWN) != 0;
  if (layout == kKbdTopRowLayoutWilco) {
    if (RewriteTopRowKeysForLayoutWilco(key_event, search_is_pressed, state)) {
      return;
    }
  } else if ((state->key_code >= ui::VKEY_F1) &&
             (state->key_code <= ui::VKEY_F12)) {
    //  Search? Top Row   Result
    //  ------- --------  ------
    //  No      Fn        Unchanged
    //  No      System    Fn -> System
    //  Yes     Fn        Fn -> System
    //  Yes     System    Search+Fn -> Fn
    if (ForceTopRowAsFunctionKeys() == search_is_pressed) {
      // Rewrite the F1-F12 keys on a Chromebook keyboard to system keys.
      // This is the original Chrome OS layout.
      static const KeyboardRemapping kFkeysToSystemKeys1[] = {
          {{ui::EF_NONE, ui::VKEY_F1},
           {ui::EF_NONE, ui::DomCode::BROWSER_BACK, ui::DomKey::BROWSER_BACK,
            ui::VKEY_BROWSER_BACK}},
          {{ui::EF_NONE, ui::VKEY_F2},
           {ui::EF_NONE, ui::DomCode::BROWSER_FORWARD,
            ui::DomKey::BROWSER_FORWARD, ui::VKEY_BROWSER_FORWARD}},
          {{ui::EF_NONE, ui::VKEY_F3},
           {ui::EF_NONE, ui::DomCode::BROWSER_REFRESH,
            ui::DomKey::BROWSER_REFRESH, ui::VKEY_BROWSER_REFRESH}},
          {{ui::EF_NONE, ui::VKEY_F4},
           {ui::EF_NONE, ui::DomCode::ZOOM_TOGGLE, ui::DomKey::ZOOM_TOGGLE,
            ui::VKEY_MEDIA_LAUNCH_APP2}},
          {{ui::EF_NONE, ui::VKEY_F5},
           {ui::EF_NONE, ui::DomCode::SELECT_TASK,
            ui::DomKey::LAUNCH_MY_COMPUTER, ui::VKEY_MEDIA_LAUNCH_APP1}},
          {{ui::EF_NONE, ui::VKEY_F6},
           {ui::EF_NONE, ui::DomCode::BRIGHTNESS_DOWN,
            ui::DomKey::BRIGHTNESS_DOWN, ui::VKEY_BRIGHTNESS_DOWN}},
          {{ui::EF_NONE, ui::VKEY_F7},
           {ui::EF_NONE, ui::DomCode::BRIGHTNESS_UP, ui::DomKey::BRIGHTNESS_UP,
            ui::VKEY_BRIGHTNESS_UP}},
          {{ui::EF_NONE, ui::VKEY_F8},
           {ui::EF_NONE, ui::DomCode::VOLUME_MUTE,
            ui::DomKey::AUDIO_VOLUME_MUTE, ui::VKEY_VOLUME_MUTE}},
          {{ui::EF_NONE, ui::VKEY_F9},
           {ui::EF_NONE, ui::DomCode::VOLUME_DOWN,
            ui::DomKey::AUDIO_VOLUME_DOWN, ui::VKEY_VOLUME_DOWN}},
          {{ui::EF_NONE, ui::VKEY_F10},
           {ui::EF_NONE, ui::DomCode::VOLUME_UP, ui::DomKey::AUDIO_VOLUME_UP,
            ui::VKEY_VOLUME_UP}},
      };
      // The new layout with forward button removed and play/pause added.
      static const KeyboardRemapping kFkeysToSystemKeys2[] = {
          {{ui::EF_NONE, ui::VKEY_F1},
           {ui::EF_NONE, ui::DomCode::BROWSER_BACK, ui::DomKey::BROWSER_BACK,
            ui::VKEY_BROWSER_BACK}},
          {{ui::EF_NONE, ui::VKEY_F2},
           {ui::EF_NONE, ui::DomCode::BROWSER_REFRESH,
            ui::DomKey::BROWSER_REFRESH, ui::VKEY_BROWSER_REFRESH}},
          {{ui::EF_NONE, ui::VKEY_F3},
           {ui::EF_NONE, ui::DomCode::ZOOM_TOGGLE, ui::DomKey::ZOOM_TOGGLE,
            ui::VKEY_MEDIA_LAUNCH_APP2}},
          {{ui::EF_NONE, ui::VKEY_F4},
           {ui::EF_NONE, ui::DomCode::SELECT_TASK,
            ui::DomKey::LAUNCH_MY_COMPUTER, ui::VKEY_MEDIA_LAUNCH_APP1}},
          {{ui::EF_NONE, ui::VKEY_F5},
           {ui::EF_NONE, ui::DomCode::BRIGHTNESS_DOWN,
            ui::DomKey::BRIGHTNESS_DOWN, ui::VKEY_BRIGHTNESS_DOWN}},
          {{ui::EF_NONE, ui::VKEY_F6},
           {ui::EF_NONE, ui::DomCode::BRIGHTNESS_UP, ui::DomKey::BRIGHTNESS_UP,
            ui::VKEY_BRIGHTNESS_UP}},
          {{ui::EF_NONE, ui::VKEY_F7},
           {ui::EF_NONE, ui::DomCode::MEDIA_PLAY_PAUSE,
            ui::DomKey::MEDIA_PLAY_PAUSE, ui::VKEY_MEDIA_PLAY_PAUSE}},
          {{ui::EF_NONE, ui::VKEY_F8},
           {ui::EF_NONE, ui::DomCode::VOLUME_MUTE,
            ui::DomKey::AUDIO_VOLUME_MUTE, ui::VKEY_VOLUME_MUTE}},
          {{ui::EF_NONE, ui::VKEY_F9},
           {ui::EF_NONE, ui::DomCode::VOLUME_DOWN,
            ui::DomKey::AUDIO_VOLUME_DOWN, ui::VKEY_VOLUME_DOWN}},
          {{ui::EF_NONE, ui::VKEY_F10},
           {ui::EF_NONE, ui::DomCode::VOLUME_UP, ui::DomKey::AUDIO_VOLUME_UP,
            ui::VKEY_VOLUME_UP}},
      };

      const KeyboardRemapping* mapping = nullptr;
      size_t mappingSize = 0u;
      switch (layout) {
        case kKbdTopRowLayout2:
          mapping = kFkeysToSystemKeys2;
          mappingSize = base::size(kFkeysToSystemKeys2);
          break;
        case kKbdTopRowLayout1:
        default:
          mapping = kFkeysToSystemKeys1;
          mappingSize = base::size(kFkeysToSystemKeys1);
          break;
      }

      MutableKeyState incoming_without_command = *state;
      incoming_without_command.flags &= ~ui::EF_COMMAND_DOWN;
      if (RewriteWithKeyboardRemappings(mapping, mappingSize,
                                        incoming_without_command, state)) {
        return;
      }
    } else if (search_is_pressed) {
      // Allow Search to avoid rewriting F1-F12.
      state->flags &= ~ui::EF_COMMAND_DOWN;
      return;
    }
  }

  if (state->flags & ui::EF_COMMAND_DOWN) {
    // Remap Search+<number> to F<number>.
    // We check the DOM3 |code| here instead of the VKEY, as these keys may
    // have different |KeyboardCode|s when modifiers are pressed, such as shift.
    static const struct {
      ui::DomCode input_dom_code;
      MutableKeyState result;
    } kNumberKeysToFkeys[] = {
        {ui::DomCode::DIGIT1,
         {ui::EF_NONE, ui::DomCode::F1, ui::DomKey::F1, ui::VKEY_F1}},
        {ui::DomCode::DIGIT2,
         {ui::EF_NONE, ui::DomCode::F2, ui::DomKey::F2, ui::VKEY_F2}},
        {ui::DomCode::DIGIT3,
         {ui::EF_NONE, ui::DomCode::F3, ui::DomKey::F3, ui::VKEY_F3}},
        {ui::DomCode::DIGIT4,
         {ui::EF_NONE, ui::DomCode::F4, ui::DomKey::F4, ui::VKEY_F4}},
        {ui::DomCode::DIGIT5,
         {ui::EF_NONE, ui::DomCode::F5, ui::DomKey::F5, ui::VKEY_F5}},
        {ui::DomCode::DIGIT6,
         {ui::EF_NONE, ui::DomCode::F6, ui::DomKey::F6, ui::VKEY_F6}},
        {ui::DomCode::DIGIT7,
         {ui::EF_NONE, ui::DomCode::F7, ui::DomKey::F7, ui::VKEY_F7}},
        {ui::DomCode::DIGIT8,
         {ui::EF_NONE, ui::DomCode::F8, ui::DomKey::F8, ui::VKEY_F8}},
        {ui::DomCode::DIGIT9,
         {ui::EF_NONE, ui::DomCode::F9, ui::DomKey::F9, ui::VKEY_F9}},
        {ui::DomCode::DIGIT0,
         {ui::EF_NONE, ui::DomCode::F10, ui::DomKey::F10, ui::VKEY_F10}},
        {ui::DomCode::MINUS,
         {ui::EF_NONE, ui::DomCode::F11, ui::DomKey::F11, ui::VKEY_F11}},
        {ui::DomCode::EQUAL,
         {ui::EF_NONE, ui::DomCode::F12, ui::DomKey::F12, ui::VKEY_F12}}};
    for (const auto& map : kNumberKeysToFkeys) {
      if (state->code == map.input_dom_code) {
        state->flags &= ~ui::EF_COMMAND_DOWN;
        ApplyRemapping(map.result, state);
        return;
      }
    }
  }
}

void EventRewriterChromeOS::RewriteLocatedEvent(const ui::Event& event,
                                                int* flags) {
  if (!delegate_)
    return;
  *flags = GetRemappedModifierMasks(event, *flags);
}

int EventRewriterChromeOS::RewriteModifierClick(
    const ui::MouseEvent& mouse_event,
    int* flags) {
  // Note that this behavior is limited to mouse events coming from touchpad
  // devices. https://crbug.com/890648.

  // Remap either Alt+Button1 or Search+Button1 to Button3 based on
  // flag/setting.
  int matched_mask;
  if (ShouldRemapToRightClick(mouse_event, *flags, &matched_mask)) {
    *flags &= ~matched_mask;
    *flags |= ui::EF_RIGHT_MOUSE_BUTTON;
    if (mouse_event.type() == ui::ET_MOUSE_PRESSED) {
      pressed_device_ids_.insert(mouse_event.source_device_id());
      if (matched_mask == kSearchLeftButton) {
        base::RecordAction(
            base::UserMetricsAction("SearchClickMappedToRightClick"));
      } else {
        DCHECK(matched_mask == kAltLeftButton);
        base::RecordAction(
            base::UserMetricsAction("AltClickMappedToRightClick"));
      }
    } else {
      pressed_device_ids_.erase(mouse_event.source_device_id());
    }
    return ui::EF_RIGHT_MOUSE_BUTTON;
  }
  return ui::EF_NONE;
}

void EventRewriterChromeOS::RewriteKeyEventInContext(
    const ui::KeyEvent& key_event,
    std::unique_ptr<ui::Event>* rewritten_event,
    ui::EventRewriteStatus* status) {
  DCHECK(status);

  if (*status == EventRewriteStatus::EVENT_REWRITE_DISCARD)
    return;

  MutableKeyState current_key_state;
  auto key_state_comparator =
      [&current_key_state](
          const std::pair<MutableKeyState, MutableKeyState>& key_state) {
        return (current_key_state.code == key_state.first.code) &&
               (current_key_state.key == key_state.first.key) &&
               (current_key_state.key_code == key_state.first.key_code);
      };

  const int mapped_flag = ModifierDomKeyToEventFlag(key_event.GetDomKey());

  if (key_event.type() == ET_KEY_PRESSED) {
    current_key_state = MutableKeyState(
        rewritten_event->get()
            ? static_cast<const ui::KeyEvent*>(rewritten_event->get())
            : &key_event);
    MutableKeyState original_key_state(&key_event);
    auto iter = std::find_if(pressed_key_states_.begin(),
                             pressed_key_states_.end(), key_state_comparator);

    // When a key is pressed, store |current_key_state| if it is not stored
    // before.
    if (iter == pressed_key_states_.end()) {
      pressed_key_states_.push_back(
          std::make_pair(current_key_state, original_key_state));
    }

    return;
  }

  DCHECK_EQ(key_event.type(), ET_KEY_RELEASED);

  if (mapped_flag != EF_NONE) {
    // The released key is a modifier

    DomKey::Base current_key = key_event.GetDomKey();
    auto key_state_iter = pressed_key_states_.begin();
    int event_flags = rewritten_event->get() ? (*rewritten_event)->flags()
                                             : key_event.flags();
    rewritten_event->reset();

    // Iterate the keys being pressed. Release the key events which satisfy one
    // of the following conditions:
    // (1) the key event's original key code (before key event rewriting if
    // any) is the same with the key to be released.
    // (2) the key event is rewritten and its original flags are influenced by
    // the key to be released.
    // Example: Press the Launcher button, Press the Up Arrow button, Release
    // the Launcher button. When Launcher is released: the key event whose key
    // code is Launcher should be released because it satisfies the condition 1;
    // the key event whose key code is PageUp should be released because it
    // satisfies the condition 2.
    while (key_state_iter != pressed_key_states_.end()) {
      const bool is_rewritten =
          (key_state_iter->first.key != key_state_iter->second.key);
      const bool flag_affected = key_state_iter->second.flags & mapped_flag;
      const bool should_release = key_state_iter->second.key == current_key ||
                                  (flag_affected && is_rewritten);

      if (should_release) {
        // If the key should be released, create a key event for it.
        std::unique_ptr<ui::KeyEvent> dispatched_event =
            std::make_unique<ui::KeyEvent>(
                key_event.type(), key_state_iter->first.key_code,
                key_state_iter->first.code, event_flags,
                key_state_iter->first.key, key_event.time_stamp());
        if (!rewritten_event->get())
          *rewritten_event = std::move(dispatched_event);
        else
          dispatched_key_events_.push_back(std::move(dispatched_event));

        key_state_iter = pressed_key_states_.erase(key_state_iter);
        continue;
      }
      key_state_iter++;
    }

    if (dispatched_key_events_.empty()) {
      *status = rewritten_event->get()
                    ? EventRewriteStatus::EVENT_REWRITE_REWRITTEN
                    : EventRewriteStatus::EVENT_REWRITE_DISCARD;
    } else {
      *status = EventRewriteStatus::EVENT_REWRITE_DISPATCH_ANOTHER;
    }
  } else {
    // The released key is not a modifier

    current_key_state = MutableKeyState(
        rewritten_event->get()
            ? static_cast<const ui::KeyEvent*>(rewritten_event->get())
            : &key_event);
    auto iter = std::find_if(pressed_key_states_.begin(),
                             pressed_key_states_.end(), key_state_comparator);
    if (iter != pressed_key_states_.end()) {
      pressed_key_states_.erase(iter);
    } else {
      // Event rewriting may create a meaningless key event.
      // For example: press the Up Arrow button, press the Launcher button,
      // release the Up Arrow. When the Up Arrow button is released, key event
      // rewriting happens. However, the rewritten event is not among
      // |pressed_key_states_|. So it should be blocked and the original event
      // should be propagated.
      rewritten_event->reset();
      *status = EventRewriteStatus::EVENT_REWRITE_CONTINUE;
    }
  }
}

// The keyboard layout for Wilco has a slightly different top-row layout, emits
// both Fn and action keys from kernel and has key events with Dom codes and no
// VKey value == VKEY_UNKNOWN. Depending on the state of the search key and
// force-function-key preference, function keys have to be mapped to action keys
// or vice versa.
//
//  Search  force function keys key code   Result
//  ------- ------------------- --------   ------
//  No        No                Function   Unchanged
//  Yes       No                Function   Fn -> Action
//  No        Yes               Function   Unchanged
//  Yes       Yes               Function   Fn -> Action
//  No        No                Action     Unchanged
//  Yes       No                Action     Action -> Fn
//  No        Yes               Action     Action -> Fn
//  Yes       Yes               Action     Unchanged
bool EventRewriterChromeOS::RewriteTopRowKeysForLayoutWilco(
    const ui::KeyEvent& key_event,
    bool search_is_pressed,
    ui::EventRewriterChromeOS::MutableKeyState* state) {
  // When the kernel issues an function key (Fn modifier help down) and the
  // search key is pressed, the function key needs to be mapped to its
  // corresponding action key. This table defines those function-to-action
  // mappings.
  static const KeyboardRemapping kFnkeysToActionKeys[] = {
      {{ui::EF_NONE, ui::VKEY_F1},
       {ui::EF_NONE, ui::DomCode::BROWSER_BACK, ui::DomKey::BROWSER_BACK,
        ui::VKEY_BROWSER_BACK}},
      {{ui::EF_NONE, ui::VKEY_F2},
       {ui::EF_NONE, ui::DomCode::BROWSER_REFRESH, ui::DomKey::BROWSER_REFRESH,
        ui::VKEY_BROWSER_REFRESH}},
      // Map F3 to VKEY_MEDIA_LAUNCH_APP2 + EF_NONE == toggle full screen:
      {{ui::EF_NONE, ui::VKEY_F3},
       {ui::EF_NONE, ui::DomCode::ZOOM_TOGGLE, ui::DomKey::ZOOM_TOGGLE,
        ui::VKEY_MEDIA_LAUNCH_APP2}},
      // Map F4 to VKEY_MEDIA_LAUNCH_APP1 + EF_NONE == overview:
      {{ui::EF_NONE, ui::VKEY_F4},
       {ui::EF_NONE, ui::DomCode::F4, ui::DomKey::F4,
        ui::VKEY_MEDIA_LAUNCH_APP1}},
      {{ui::EF_NONE, ui::VKEY_F5},
       {ui::EF_NONE, ui::DomCode::BRIGHTNESS_DOWN, ui::DomKey::BRIGHTNESS_DOWN,
        ui::VKEY_BRIGHTNESS_DOWN}},
      {{ui::EF_NONE, ui::VKEY_F6},
       {ui::EF_NONE, ui::DomCode::BRIGHTNESS_UP, ui::DomKey::BRIGHTNESS_UP,
        ui::VKEY_BRIGHTNESS_UP}},
      {{ui::EF_NONE, ui::VKEY_F7},
       {ui::EF_NONE, ui::DomCode::VOLUME_MUTE, ui::DomKey::AUDIO_VOLUME_MUTE,
        ui::VKEY_VOLUME_MUTE}},
      {{ui::EF_NONE, ui::VKEY_F8},
       {ui::EF_NONE, ui::DomCode::VOLUME_DOWN, ui::DomKey::AUDIO_VOLUME_DOWN,
        ui::VKEY_VOLUME_DOWN}},
      {{ui::EF_NONE, ui::VKEY_F9},
       {ui::EF_NONE, ui::DomCode::VOLUME_UP, ui::DomKey::AUDIO_VOLUME_UP,
        ui::VKEY_VOLUME_UP}},
      // Note: F10 and F11 are left as-is since no action is associated with
      // these keys.
      {{ui::EF_NONE, ui::VKEY_F10},
       {ui::EF_NONE, ui::DomCode::F10, ui::DomKey::F10, ui::VKEY_F10}},
      {{ui::EF_NONE, ui::VKEY_F11},
       {ui::EF_NONE, ui::DomCode::F11, ui::DomKey::F11, ui::VKEY_F11}},
      {{ui::EF_NONE, ui::VKEY_F12},
       // Map F12 to VKEY_MEDIA_LAUNCH_APP2 + EF_CONTROL_DOWN == toggle mirror
       // mode:
       {ui::EF_CONTROL_DOWN, ui::DomCode::F12, ui::DomKey::F12,
        ui::VKEY_MEDIA_LAUNCH_APP2}},
  };

  // When the kernel issues an action key (default mode) and the search key is
  // pressed, the action key needs to be mapped back to its corresponding
  // action key. This table defines those action-to-function mappings. Note:
  // this table is essentially the dual of kFnToActionLeys above.
  static const KeyboardRemapping kActionToFnKeys[] = {
      {{ui::EF_NONE, ui::VKEY_BROWSER_BACK},
       {ui::EF_NONE, ui::DomCode::F1, ui::DomKey::F1, ui::VKEY_F1}},
      {{ui::EF_NONE, ui::VKEY_BROWSER_REFRESH},
       {ui::EF_NONE, ui::DomCode::F2, ui::DomKey::F2, ui::VKEY_F2}},
      {{ui::EF_NONE, ui::VKEY_MEDIA_LAUNCH_APP1},
       {ui::EF_NONE, ui::DomCode::F4, ui::DomKey::F4, ui::VKEY_F4}},
      {{ui::EF_NONE, ui::VKEY_BRIGHTNESS_DOWN},
       {ui::EF_NONE, ui::DomCode::F5, ui::DomKey::F5, ui::VKEY_F5}},
      {{ui::EF_NONE, ui::VKEY_BRIGHTNESS_UP},
       {ui::EF_NONE, ui::DomCode::F6, ui::DomKey::F6, ui::VKEY_F6}},
      {{ui::EF_NONE, ui::VKEY_VOLUME_MUTE},
       {ui::EF_NONE, ui::DomCode::F7, ui::DomKey::F7, ui::VKEY_F7}},
      {{ui::EF_NONE, ui::VKEY_VOLUME_DOWN},
       {ui::EF_NONE, ui::DomCode::F8, ui::DomKey::F8, ui::VKEY_F8}},
      {{ui::EF_NONE, ui::VKEY_VOLUME_UP},
       {ui::EF_NONE, ui::DomCode::F9, ui::DomKey::F9, ui::VKEY_F9}},
      // Do not change the order of the next two entries. The remapping of
      // VKEY_MEDIA_LAUNCH_APP2 with Control held down must appear before
      // VKEY_MEDIA_LAUNCH_APP2 by itself to be considered.
      {{ui::EF_CONTROL_DOWN, ui::VKEY_MEDIA_LAUNCH_APP2},
       {ui::EF_NONE, ui::DomCode::F12, ui::DomKey::F12, ui::VKEY_F12}},
      {{ui::EF_NONE, ui::VKEY_MEDIA_LAUNCH_APP2},
       {ui::EF_NONE, ui::DomCode::F3, ui::DomKey::F3, ui::VKEY_F3}},
  };

  ui::EventRewriterChromeOS::MutableKeyState incoming_without_command = *state;
  incoming_without_command.flags &= ~ui::EF_COMMAND_DOWN;

  if ((state->key_code >= ui::VKEY_F1) && (state->key_code <= ui::VKEY_F12)) {
    // Incoming key code is a Fn key. Check if it needs to be mapped back to its
    // corresponding action key.
    if (search_is_pressed) {
      return RewriteWithKeyboardRemappings(kFnkeysToActionKeys,
                                           base::size(kFnkeysToActionKeys),
                                           incoming_without_command, state);
    }
    return true;
  } else if (IsKeyCodeInMappings(state->key_code, kActionToFnKeys,
                                 base::size(kActionToFnKeys))) {
    // Incoming key code is an action key. Check if it needs to be mapped back
    // to its corresponding function key.
    if (search_is_pressed != ForceTopRowAsFunctionKeys()) {
      return RewriteWithKeyboardRemappings(kActionToFnKeys,
                                           base::size(kActionToFnKeys),
                                           incoming_without_command, state);
    }

    // At this point we know search_is_pressed == ForceTopRowAsFunctionKeys().
    // If they're both true, they cancel each other. Thus we can clear the
    // search-key modifier flag.
    state->flags &= ~ui::EF_COMMAND_DOWN;

    return true;
  }

  return false;
}

void EventRewriterChromeOS::KeyboardDeviceAddedInternal(
    int device_id,
    DeviceType type,
    KeyboardTopRowLayout layout) {
  // Always overwrite the existing device_id since the X server may reuse a
  // device id for an unattached device.
  device_id_to_info_[device_id] = {type, layout};
}

bool EventRewriterChromeOS::ForceTopRowAsFunctionKeys() const {
  return delegate_ && delegate_->TopRowKeysAreFunctionKeys();
}

EventRewriterChromeOS::DeviceType EventRewriterChromeOS::KeyboardDeviceAdded(
    int device_id) {
  if (!ui::DeviceDataManager::HasInstance())
    return kDeviceUnknown;
  const std::vector<ui::InputDevice>& keyboard_devices =
      ui::DeviceDataManager::GetInstance()->GetKeyboardDevices();
  for (const auto& keyboard : keyboard_devices) {
    if (keyboard.id == device_id) {
      const DeviceType type = GetDeviceType(keyboard);
      if (type == kDeviceAppleKeyboard) {
        VLOG(1) << "Apple keyboard '" << keyboard.name << "' connected: "
                << "id=" << device_id;
      } else if (type == kDeviceHotrodRemote) {
        VLOG(1) << "Hotrod remote '" << keyboard.name << "' connected: "
                << "id=" << device_id;
      } else if (type == kDeviceVirtualCoreKeyboard) {
        VLOG(1) << "Xorg virtual '" << keyboard.name << "' connected: "
                << "id=" << device_id;
      } else {
        VLOG(1) << "Unknown keyboard '" << keyboard.name << "' connected: "
                << "id=" << device_id;
      }

      KeyboardTopRowLayout layout;
      if (GetKeyboardTopRowLayout(keyboard.sys_path, &layout)) {
        // Don't store a device info when an error occurred while reading from
        // udev. This gives a chance to reattempt reading from udev on
        // subsequent key events, rather than being stuck in a bad state until
        // next reboot. crbug.com/783166.
        KeyboardDeviceAddedInternal(keyboard.id, type, layout);
      }

      return type;
    }
  }
  return kDeviceUnknown;
}

}  // namespace ui
