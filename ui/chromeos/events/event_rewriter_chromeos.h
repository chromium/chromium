// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_EVENTS_EVENT_REWRITER_CHROMEOS_H_
#define UI_CHROMEOS_EVENTS_EVENT_REWRITER_CHROMEOS_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/dom/dom_key.h"

namespace chromeos {
namespace input_method {
class ImeKeyboard;
}  // namespace input_method
}  // namespace chromeos

namespace ui {

enum class DomCode;

// EventRewriterChromeOS makes various changes to keyboard-related events,
// including KeyEvents and some other events with keyboard modifier flags:
// - maps certain non-character keys according to user preferences
//   (Control, Alt, Search, Caps Lock, Escape, Backspace, Diamond);
// - maps Command to Control on Apple keyboards;
// - converts numeric pad editing keys to their numeric forms;
// - converts top-row function keys to special keys where necessary;
// - handles various key combinations like Search+Backspace -> Delete
//   and Search+number to Fnumber;
// - handles key/pointer combinations like Alt+Button1 -> Button3.
class EventRewriterChromeOS : public EventRewriter {
 public:
  enum DeviceType {
    kDeviceUnknown = 0,
    kDeviceInternalKeyboard,
    kDeviceExternalAppleKeyboard,
    kDeviceExternalChromeOsKeyboard,
    kDeviceExternalGenericKeyboard,
    kDeviceExternalUnknown,
    kDeviceHotrodRemote,
    kDeviceVirtualCoreKeyboard,  // X-server generated events.
  };

  enum KeyboardTopRowLayout {
    // The original Chrome OS Layout:
    // Browser Back, Browser Forward, Refresh, Full Screen, Overview,
    // Brightness Down, Brightness Up, Mute, Volume Down, Volume Up.
    kKbdTopRowLayout1 = 1,
    kKbdTopRowLayoutDefault = kKbdTopRowLayout1,
    kKbdTopRowLayoutMin = kKbdTopRowLayout1,
    // 2017 keyboard layout: Browser Forward is gone and Play/Pause
    // key is added between Brightness Up and Mute.
    kKbdTopRowLayout2 = 2,
    // Keyboard layout and handling for Wilco.
    kKbdTopRowLayoutWilco = 3,
    kKbdTopRowLayoutDrallion = 4,

    // Handling for all keyboards that support supplying a custom layout
    // via sysfs attribute (aka Vivaldi). See crbug.com/1076241
    kKbdTopRowLayoutCustom = 5,
    kKbdTopRowLayoutMax = kKbdTopRowLayoutCustom
  };

  // Things that keyboard-related rewriter phases can change about an Event.
  struct MutableKeyState {
    MutableKeyState();
    explicit MutableKeyState(const KeyEvent* key_event);
    MutableKeyState(int input_flags,
                    DomCode input_code,
                    DomKey::Base input_key,
                    KeyboardCode input_key_code);

    int flags;
    DomCode code;
    DomKey::Base key;
    KeyboardCode key_code;
  };

  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Returns true if we want to rewrite modifier keys.
    virtual bool RewriteModifierKeys() = 0;

    // Returns true if get keyboard remapped preference value successfully and
    // the value will be stored in |value|.
    virtual bool GetKeyboardRemappedPrefValue(const std::string& pref_name,
                                              int* value) const = 0;

    // Returns true if the target would prefer to receive raw
    // function keys instead of having them rewritten into back, forward,
    // brightness, volume, etc. or if the user has specified that they desire
    // top-row keys to be treated as function keys globally.
    virtual bool TopRowKeysAreFunctionKeys() const = 0;

    // Returns true if the |key_code| and |flags| have been resgistered for
    // extensions and EventRewriterChromeOS will not rewrite the event.
    virtual bool IsExtensionCommandRegistered(KeyboardCode key_code,
                                              int flags) const = 0;

    // Returns true if search key accelerator is reserved for current active
    // window and EventRewriterChromeOS will not rewrite the event.
    virtual bool IsSearchKeyAcceleratorReserved() const = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Does not take ownership of the |sticky_keys_controller|, which may also be
  // nullptr (for testing without ash), in which case sticky key operations
  // don't happen.
  EventRewriterChromeOS(Delegate* delegate,
                        EventRewriter* sticky_keys_controller,
                        bool privacy_screen_supported);
  ~EventRewriterChromeOS() override;

  // Calls KeyboardDeviceAdded.
  void KeyboardDeviceAddedForTesting(int device_id);

  // Reset the internal rewriter state so that next set of tests can be ran on
  // the same rewriter, if needed.
  void ResetStateForTesting();

  // Calls RewriteMouseEvent().
  void RewriteMouseButtonEventForTesting(const MouseEvent& event,
                                         const Continuation continuation);

  void set_last_keyboard_device_id_for_testing(int device_id) {
    last_keyboard_device_id_ = device_id;
  }
  void set_ime_keyboard_for_testing(
      ::chromeos::input_method::ImeKeyboard* ime_keyboard) {
    ime_keyboard_for_testing_ = ime_keyboard;
  }

  void set_privacy_screen_for_testing(bool supported) {
    privacy_screen_supported_ = supported;
  }

  // EventRewriter overrides:
  EventDispatchDetails RewriteEvent(const Event& event,
                                    const Continuation continuation) override;

  // Generate a new key event from an original key event and the replacement
  // state determined by a key rewriter.
  static void BuildRewrittenKeyEvent(const KeyEvent& key_event,
                                     const MutableKeyState& state,
                                     std::unique_ptr<Event>* rewritten_event);

  // Given a keyboard device, returns its type.
  static DeviceType GetDeviceType(const InputDevice& keyboard_device);

  // Given a keyboard device, returns its top row layout. Will return default
  // kKbdTopRowLayoutDefault if the device is not tagged with a specific
  // layout, or when failing to retrieve device layout from udev.
  static KeyboardTopRowLayout GetKeyboardTopRowLayout(
      const InputDevice& keyboard_device);

  // Given a keyboard device, returns true if we get back the Assistant key
  // property without getting an error. Property value is stored in
  // |has_assistant_key|.
  static bool HasAssistantKeyOnKeyboard(const InputDevice& keyboard_device,
                                        bool* has_assistant_key);

  // Part of rewrite phases below. These methods are public only so that
  // SpokenFeedbackRewriter can ask for rewritten modifier and function keys.

  // Returns true when the input |state| has key |DomKey::ALT_GRAPH_LATCH| and
  // is remapped.
  bool RewriteModifierKeys(const KeyEvent& event, MutableKeyState* state);
  void RewriteFunctionKeys(const KeyEvent& event, MutableKeyState* state);

 private:
  struct DeviceInfo {
    DeviceType type;
    KeyboardTopRowLayout top_row_layout;
  };

  void DeviceKeyPressedOrReleased(int device_id);

  // By default the top row (F1-F12) keys are system keys for back, forward,
  // brightness, volume, etc. However, windows for v2 apps can optionally
  // request raw function keys for these keys.
  bool ForceTopRowAsFunctionKeys() const;

  // Adds a device to |device_id_to_info_| only if no failure occurs in
  // identifying the keyboard, and returns the device type of this keyboard
  // even if it wasn't stored in |device_id_to_info_|.
  DeviceType KeyboardDeviceAdded(int device_id);

  // Returns true if |last_keyboard_device_id_| is Hotrod remote.
  bool IsHotrodRemote() const;
  // Returns true if |last_keyboard_device_id_| is of given |device_type|.
  bool IsLastKeyboardOfType(DeviceType device_type) const;
  // Returns the device type of |last_keyboard_device_id_|.
  DeviceType GetLastKeyboardType() const;

  // Given modifier flags |original_flags|, returns the remapped modifiers
  // according to user preferences and/or event properties.
  int GetRemappedModifierMasks(const Event& event, int original_flags) const;

  // Returns true if this event should be remapped to a right-click.
  // |matched_mask| will be set to the variant (Alt+Click or Search+Click)
  // that was used to match based on flag/feature settings. |matched_mask|
  // only has a valid value when returning true.
  bool ShouldRemapToRightClick(const MouseEvent& mouse_event,
                               int flags,
                               int* matched_mask) const;

  // Rewrite a particular kind of event.
  EventRewriteStatus RewriteKeyEvent(const KeyEvent& key_event,
                                     std::unique_ptr<Event>* rewritten_event);
  EventDispatchDetails RewriteMouseButtonEvent(const MouseEvent& mouse_event,
                                               const Continuation continuation);
  EventDispatchDetails RewriteMouseWheelEvent(
      const MouseWheelEvent& mouse_event,
      const Continuation continuation);
  EventDispatchDetails RewriteTouchEvent(const TouchEvent& touch_event,
                                         const Continuation continuation);
  EventDispatchDetails RewriteScrollEvent(const ScrollEvent& scroll_event,
                                          const Continuation continuation);

  // Rewriter phases. These can inspect the original |event|, but operate using
  // the current |state|, which may have been modified by previous phases.
  void RewriteNumPadKeys(const KeyEvent& event, MutableKeyState* state);
  void RewriteExtendedKeys(const KeyEvent& event, MutableKeyState* state);
  int RewriteLocatedEvent(const Event& event);
  int RewriteModifierClick(const MouseEvent& event, int* flags);

  // Reads the keyboard mapping for new CrOS keyboards that support
  // supplying a custom layout via sysfs and stores it mapped to
  // |keyboard_device| in |top_row_scan_code_map_|.
  bool StoreCustomTopRowMapping(const ui::InputDevice& keyboard_device);

  // Handle Function <-> Action key remapping for new CrOS keyboards that
  // support supplying a custom layout via sysfs.
  bool RewriteTopRowKeysForCustomLayout(
      int device_id,
      const ui::KeyEvent& key_event,
      bool search_is_pressed,
      ui::EventRewriterChromeOS::MutableKeyState* state);

  // Handle Fn/Action key remapping for Wilco keyboard layout.
  bool RewriteTopRowKeysForLayoutWilco(const KeyEvent& key_event,
                                       bool search_is_pressed,
                                       MutableKeyState* state,
                                       KeyboardTopRowLayout layout);

  // Take the keys being pressed into consideration, in contrast to
  // RewriteKeyEvent which computes the rewritten event and event rewrite
  // status in stateless way.
  EventDispatchDetails RewriteKeyEventInContext(
      const KeyEvent& event,
      std::unique_ptr<Event> rewritten_event,
      EventRewriteStatus status,
      const Continuation continuation);

  EventDispatchDetails SendStickyKeysReleaseEvents(
      std::unique_ptr<Event> rewritten_event,
      const Continuation continuation);

  // A set of device IDs whose press event has been rewritten.
  // This is to ensure that press and release events are rewritten consistently.
  std::set<int> pressed_device_ids_;

  std::map<int, DeviceInfo> device_id_to_info_;

  // Maps a device ID to a mapping of scan_code to MutableKeyState on keyboards
  // that supply it via a sysfs attribute.
  // eg. map<device_id, Map<scan_code, MutableKeyState>>.
  base::flat_map<int, base::flat_map<uint32_t, MutableKeyState>>
      top_row_scan_code_map_;

  // The |source_device_id()| of the most recent keyboard event,
  // used to interpret modifiers on pointer events.
  int last_keyboard_device_id_;

  ::chromeos::input_method::ImeKeyboard* ime_keyboard_for_testing_;

  Delegate* const delegate_;

  // For each pair, the first element is the rewritten key state and the second
  // one is the original key state. If no key event rewriting happens, the first
  // element and the second element are identical.
  std::list<std::pair<MutableKeyState, MutableKeyState>> pressed_key_states_;

  // The sticky keys controller is not owned here;
  // at time of writing it is a singleton in ash::Shell.
  EventRewriter* const sticky_keys_controller_;

  // Some drallion devices have digital privacy screens and a corresponding
  // privacy screen toggle key in the top row.
  bool privacy_screen_supported_;

  // Some keyboard layouts have 'latching' keys, which either apply
  // a modifier while held down (like normal modifiers), or, if no
  // non-modifier is pressed while the latching key is down, apply the
  // modifier to the next non-modifier keypress. Under Ozone the stateless
  // layout model requires this to be handled explicitly. See crbug.com/518237
  // Pragmatically this, like the Diamond key, is handled here in
  // EventRewriterChromeOS, but modifier state management is scattered between
  // here, sticky keys, and the system layer (Ozone), and could do with
  // refactoring.
  // - |pressed_modifier_latches_| records the latching keys currently pressed.
  //   It also records the active modifier flags for non-modifier keys that are
  //   remapped to modifiers, e.g. Diamond/F15.
  // - |latched_modifier_latches_| records the latching keys just released,
  //   to be applied to the next non-modifier key.
  // - |used_modifier_latches_| records the latching keys applied to a non-
  //   modifier while pressed, so that they do not get applied after release.
  int pressed_modifier_latches_;
  int latched_modifier_latches_;
  int used_modifier_latches_;

  DISALLOW_COPY_AND_ASSIGN(EventRewriterChromeOS);
};

}  // namespace ui

#endif  // UI_CHROMEOS_EVENTS_EVENT_REWRITER_CHROMEOS_H_
