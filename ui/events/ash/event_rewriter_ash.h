// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_EVENT_REWRITER_ASH_H_
#define UI_EVENTS_ASH_EVENT_REWRITER_ASH_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/ash/event_rewriter_utils.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/mojom/extended_fkeys_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/dom/dom_key.h"

namespace ash {
namespace input_method {
class ImeKeyboard;
}
}  // namespace ash

namespace ui {

enum class DomCode : uint32_t;
struct KeyboardDevice;

// EventRewriterAsh makes various changes to keyboard-related events,
// including KeyEvents and some other events with keyboard modifier flags:
// - maps certain non-character keys according to user preferences
//   (Control, Alt, Search, Caps Lock, Escape, Backspace, Diamond);
// - maps Command to Control on Apple keyboards;
// - converts numeric pad editing keys to their numeric forms;
// - converts top-row function keys to special keys where necessary;
// - handles various key combinations like Search+Backspace -> Delete
//   and Search+number to Fnumber;
// - handles key/pointer combinations like Alt+Button1 -> Button3.
class EventRewriterAsh : public EventRewriter {
 public:
  // Things that keyboard-related rewriter phases can change about an Event.
  struct MutableKeyState {
    constexpr MutableKeyState() = default;
    explicit MutableKeyState(const KeyEvent* key_event);
    constexpr MutableKeyState(int input_flags,
                              DomCode input_code,
                              DomKey::Base input_key,
                              KeyboardCode input_key_code)
        : flags(input_flags),
          code(input_code),
          key(input_key),
          key_code(input_key_code) {}

    friend bool operator==(const MutableKeyState& lhs,
                           const MutableKeyState& rhs) {
      return lhs.flags == rhs.flags && lhs.code == rhs.code &&
             lhs.key == rhs.key && lhs.key_code == rhs.key_code;
    }

    friend bool operator!=(const MutableKeyState& lhs,
                           const MutableKeyState& rhs) {
      return !(lhs == rhs);
    }

    int flags = 0;
    DomCode code = DomCode::NONE;
    DomKey::Base key = 0;
    KeyboardCode key_code = KeyboardCode::VKEY_NONAME;
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns true only if the the key event was rewritten to ALTGR. For most
    // cases, it is expected that this function returns false as most key events
    // do not involve ALTGR. Returns false if SuppressModifierKeyRewrites was
    // called to suppress modifier rewrites.
    virtual bool RewriteModifierKeys() = 0;

    // Suppresses all modifier key rewrites and makes |RewriteModifierKeys|
    // always return false if |should_suppress| is true.
    virtual void SuppressModifierKeyRewrites(bool should_suppress) = 0;

    // Returns whether or not Meta + Top Row Keys should be rewritten. Should
    // return correctly with respect to the values set in
    // |SuppressMetaTopRowKeyRewrites|. If per-device settings are enabled, it
    // should instead return the correct setting for the given `device_id`.
    virtual bool RewriteMetaTopRowKeyComboEvents(int device_id) const = 0;

    // Set whether or not Meta + Top Row Keys key events should be rewritten.
    virtual void SuppressMetaTopRowKeyComboRewrites(bool should_suppress) = 0;

    // If per-device settings is disabled, returns the remapped modifier value
    // from prefs by looking up the given |pref_name|. If per-device settings is
    // enabled, returns the remapped modifier value for |device_id| and
    // |modifier_key|.
    // TODO(dpad): Remove |pref_name| once fully transitioned to per-device
    // settings.
    virtual std::optional<mojom::ModifierKey> GetKeyboardRemappedModifierValue(
        int device_id,
        mojom::ModifierKey modifier_key,
        const std::string& pref_name) const = 0;

    // Returns true if the target would prefer to receive raw
    // function keys instead of having them rewritten into back, forward,
    // brightness, volume, etc. or if the user has specified that they desire
    // top-row keys to be treated as function keys globally.
    virtual bool TopRowKeysAreFunctionKeys(int device_id) const = 0;

    // Returns true if the |key_code| and |flags| have been resgistered for
    // extensions and EventRewriterAsh will not rewrite the event.
    virtual bool IsExtensionCommandRegistered(KeyboardCode key_code,
                                              int flags) const = 0;

    // Returns true if search key accelerator is reserved for current active
    // window and EventRewriterAsh will not rewrite the event.
    virtual bool IsSearchKeyAcceleratorReserved() const = 0;

    // Used to send a notification about Alt-Click being deprecated.
    // The notification is only sent once per user session, and this function
    // returns true if the notification was shown.
    virtual bool NotifyDeprecatedRightClickRewrite() = 0;

    // Used to send a notification about a Six Pack (PageUp, PageDown, Home,
    // End, Insert, Delete) key rewrite being deprecated. The notification
    // is only sent once per user session, and this function returns true if
    // the notification was shown.
    virtual bool NotifyDeprecatedSixPackKeyRewrite(KeyboardCode key_code) = 0;

    // Used to record when either Alt+Click or Search+Click is remapped to a
    // right click event. The `kEventRemappedToRightClick` pref will be used
    // to determine the default behavior for simulating a right click.
    virtual void RecordEventRemappedToRightClick(
        bool alt_based_right_click) = 0;

    // Used to record Alt/Search based key event rewrites for Six Pack keys.
    // `alt_based` tells us whether this "six pack" event was produced by an
    // Alt or Search/Launcher based keyboard shortcut. The corresponding
    // "six pack" key pref will be incremented when the Alt variant is used and
    // decremented when the Search/Launcher variant is used. This information
    // will determine the default behavior for rewriting a key event to a
    // "six pack" key.
    virtual void RecordSixPackEventRewrite(KeyboardCode key_code,
                                           bool alt_based) = 0;

    // Returns the modifier (Alt/Search) that must be pressed when remapping
    // an event to right click for `device_id` or `std::nullopt` if settings
    // for the device are unable to be retrieved. If the return value is
    // `SimulateRightClickModifier::kNone` or `std::nullopt`, the event
    // will not be rewritten to a right click.
    virtual std::optional<ui::mojom::SimulateRightClickModifier>
    GetRemapRightClickModifier(int device_id) = 0;

    // Returns whether the Alt or Search based shortcut variant must be used
    // to perform a Six Pack (PageUp, PageDown, Home, End, Insert, Delete) key
    // action for `device_id`. The key event will not be rewritten if the
    // return value is either std::nullopt (settings for `device_id`
    // weren't found) or the key is mapped to `SixPackShortcutModifier::kNone`.
    // `key_code` is used to look up the correct modifier for the Six Pack key.
    virtual std::optional<ui::mojom::SixPackShortcutModifier>
    GetShortcutModifierForSixPackKey(int device_id,
                                     ui::KeyboardCode key_code) = 0;

    // Used to send a notification when an incoming event would have been
    // remapped to a right click but either the user's setting is inconsistent
    // with the matched modifier key or remapping to right click is disabled.
    virtual void NotifyRightClickRewriteBlockedBySetting(
        ui::mojom::SimulateRightClickModifier blocked_modifier,
        ui::mojom::SimulateRightClickModifier active_modifier) = 0;

    // Used to send a notification when an incoming event would have been
    // remapped to a Six Pack key action but either the user's setting is
    // inconsistent with the matched modifier key or remapping to right click
    // is disabled. `key_code` is used to lookup the correct Six Pack key and
    // the `device_id` is provided to route the user to the correct remap keys
    // subpage when the notification is clicked on.
    virtual void NotifySixPackRewriteBlockedBySetting(
        ui::KeyboardCode key_code,
        ui::mojom::SixPackShortcutModifier blocked_modifier,
        ui::mojom::SixPackShortcutModifier active_modifier,
        int device_id) = 0;

    // Returns the modifier for rewriting key events to F11/F12 for ChromeOS
    // keyboards with less than 12 top row keys. `key_code` must be either
    // `ui::KeyboardCode::VKEY_F11` or `ui::KeyboardCode::VKEY_F12` and is used
    // used to determine if the setting for F11 or F12 should be retrieved for
    // the keyboard with the given `device_id`. The key event will not be
    // rewritten if the return value is either std::nullopt (settings for
    // `device_id` weren't found) or if an invalid `key_code` was passed in.
    virtual std::optional<ui::mojom::ExtendedFkeysModifier>
    GetExtendedFkeySetting(int device_id, ui::KeyboardCode key_code) = 0;

    // Used to send a notification when a income event is a shortcut with
    // arrow key and search key but could not find a matched remapped event,
    // and it's a split modifier keyboard.
    virtual void NotifySixPackRewriteBlockedByFnKey(
        ui::KeyboardCode key_code,
        ui::mojom::SixPackShortcutModifier modifier) = 0;

    // Used to send a notification when a income event is a shortcut with
    // top row key and search key but could not find a matched remapped event,
    // and it's a split modifier keyboard.
    virtual void NotifyTopRowRewriteBlockedByFnKey() = 0;
  };

  // Does not take ownership of the |sticky_keys_controller|, which may also be
  // nullptr (for testing without ash), in which case sticky key operations
  // don't happen.
  EventRewriterAsh(Delegate* delegate,
                   KeyboardCapability* keyboard_capability,
                   EventRewriter* sticky_keys_controller,
                   bool privacy_screen_supported);

  // Only explicitly use this constructor for tests. Does not take ownership of
  // |ime_keyboard|.
  EventRewriterAsh(Delegate* delegate,
                   KeyboardCapability* keyboard_capability,
                   EventRewriter* sticky_keys_controller,
                   bool privacy_screen_supported,
                   ash::input_method::ImeKeyboard* ime_keyboard);
  EventRewriterAsh(const EventRewriterAsh&) = delete;
  EventRewriterAsh& operator=(const EventRewriterAsh&) = delete;
  ~EventRewriterAsh() override;

  // Reset the internal rewriter state so that next set of tests can be ran on
  // the same rewriter, if needed.
  void ResetStateForTesting();

  // Calls RewriteMouseEvent().
  void RewriteMouseButtonEventForTesting(const MouseEvent& event,
                                         const Continuation continuation);

  void set_last_keyboard_device_id_for_testing(int device_id) {
    last_keyboard_device_id_ = device_id;
  }

  void set_privacy_screen_for_testing(bool supported) {
    privacy_screen_supported_ = supported;
  }

  // Enable/disable alt + key or mouse event remapping. For Alt + left click
  // mapping to the right click, it only applies if the feature
  // `chromeos::features::kUseSearchClickForRightClick` is not enabled.
  void set_alt_down_remapping_enabled(bool enabled) {
    is_alt_down_remapping_enabled_ = enabled;
  }

  // EventRewriter overrides:
  EventDispatchDetails RewriteEvent(const Event& event,
                                    const Continuation continuation) override;

  // Generate a new key event from an original key event and the replacement
  // state determined by a key rewriter.
  static void BuildRewrittenKeyEvent(const KeyEvent& key_event,
                                     const MutableKeyState& state,
                                     std::unique_ptr<Event>* rewritten_event);

  // Given a keyboard device, returns true if we get back the Assistant key
  // property without getting an error. Property value is stored in
  // |has_assistant_key|.
  static bool HasAssistantKeyOnKeyboard(const KeyboardDevice& keyboard_device,
                                        bool* has_assistant_key);

  // Part of rewrite phases below. These methods are public only so that
  // SpokenFeedbackRewriter can ask for rewritten modifier and function keys.

  // Returns true when the input |state| has key |DomKey::ALT_GRAPH_LATCH| and
  // is remapped.
  // TODO(crbug.com/40265877): Remove this function.
  bool RewriteModifierKeys(const KeyEvent& event, MutableKeyState* state) {
    return RewriteModifierKeys(event, last_keyboard_device_id_, state);
  }
  void RewriteFunctionKeys(const KeyEvent& event, MutableKeyState* state) {
    return RewriteFunctionKeys(event, last_keyboard_device_id_, state);
  }

 private:
  // By default the top row (F1-F12) keys are system keys for back, forward,
  // brightness, volume, etc. However, windows for v2 apps can optionally
  // request raw function keys for these keys.
  bool ForceTopRowAsFunctionKeys(int device_id) const;

  // Returns true if |device_id| is Hotrod remote.
  bool IsHotrodRemote(int device_id) const;

  // Given modifier flags |original_flags|, returns the remapped modifiers
  // according to user preferences and/or event properties.
  int GetRemappedModifierMasks(int device_id, int original_flags) const;

  // Returns true if this event should be remapped to a right-click.
  // |matched_mask| will be set to the variant (Alt+Click or Search+Click)
  // that was used to match based on flag/feature settings. |matched_mask|
  // only has a valid value when returning true. However, Alt+Click will not
  // be remapped if |is_alt_left_click_remapping_enabled_| is false.
  // |matched_alt_deprecation| is set to true if the alt variant has been
  // deprecated but otherwise would have been remapped. This is used to
  // show a deprecation notification.
  //
  // TODO(zentaro): This function can be removed once the deprecation for
  // Alt-rewrites is complete.
  bool ShouldRemapToRightClick(const MouseEvent& mouse_event,
                               int flags,
                               int* matched_mask,
                               bool* matched_alt_deprecation) const;

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
  bool RewriteModifierKeys(const KeyEvent& event,
                           int device_id,
                           MutableKeyState* state);
  void RewriteNumPadKeys(const KeyEvent& event, MutableKeyState* state);
  void RewriteFunctionKeys(const KeyEvent& event,
                           int device_id,
                           MutableKeyState* state);
  void RewriteExtendedKeys(const KeyEvent& event, MutableKeyState* state);
  int RewriteLocatedEvent(const Event& event);
  int RewriteModifierClick(const MouseEvent& event, int* flags);

  // Handle Function <-> Action key remapping for new CrOS keyboards that
  // support supplying a custom layout via sysfs.
  bool RewriteTopRowKeysForCustomLayout(const ui::KeyEvent& key_event,
                                        int device_id,
                                        bool flip_remapping,
                                        EventFlags flip_remapping_flag,
                                        MutableKeyState* state);

  // Handle Fn/Action key remapping for Wilco keyboard layout.
  bool RewriteTopRowKeysForLayoutWilco(
      const KeyEvent& key_event,
      int device_id,
      bool flip_remapping,
      EventFlags flip_remapping_flag,
      MutableKeyState* state,
      KeyboardCapability::KeyboardTopRowLayout layout);

  bool RewriteTopRowKeysForStandardLayouts(
      const KeyEvent& key_event,
      int device_id,
      bool flip_remapping,
      EventFlags flip_remapping_flag,
      bool rewrite_modifier_is_pressed,
      MutableKeyState* state,
      KeyboardCapability::KeyboardTopRowLayout layout);

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
  //
  // As the variable name suggests, we only care about
  // left-button-remapped-to-right events here.
  //
  // With the help of this variable, we are able to eliminate two types of edge
  // cases. (1) is that when we have more than one input sources, such as a
  // touchpad and a mouse. (2) is that when we deal with modifier(Alt or Search)
  // induced left to right button remapping.
  //
  // This variable works closely with
  // EventRewriterAsh::ShouldRemapToRightClick(). As of this writing we
  // don't have a product feature that would rewrite a mouse non-left button
  // event to a mouse left button event. This is why we only have
  // `pressed_as_right_button_device_ids_` here without a "left" counterpart.
  std::set<int> pressed_as_right_button_device_ids_;

  // The |source_device_id()| of the most recent keyboard event,
  // used to interpret modifiers on pointer events.
  int last_keyboard_device_id_;

  const raw_ptr<Delegate, DanglingUntriaged> delegate_;

  base::flat_map<internal::PhysicalKey, MutableKeyState> pressed_physical_keys_;

  // For each pair, the first element is the rewritten key state and the second
  // one is the original key state. If no key event rewriting happens, the first
  // element and the second element are identical.
  std::list<std::pair<MutableKeyState, MutableKeyState>> pressed_key_states_;

  // The sticky keys controller is not owned here;
  // at time of writing it is a singleton in ash::Shell.
  const raw_ptr<EventRewriter> sticky_keys_controller_;

  // Some drallion devices have digital privacy screens and a corresponding
  // privacy screen toggle key in the top row.
  bool privacy_screen_supported_;

  // Some keyboard layouts have 'latching' keys, which either apply
  // a modifier while held down (like normal modifiers), or, if no
  // non-modifier is pressed while the latching key is down, apply the
  // modifier to the next non-modifier keypress. Under Ozone the stateless
  // layout model requires this to be handled explicitly. See crbug.com/518237
  // Pragmatically this, like the Diamond key, is handled here in
  // EventRewriterAsh, but modifier state management is scattered between
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

  // If a non-modifier key has been remapped to a modifier key,
  // e.g. ESCAPE -> ALT, this stores the DomCode on the KeyPress event
  // along with its associated previous modifier remap.
  // Handles the case in which the original key's remap is no longer mapped to a
  // modifier but there needs to be a way to reset the stickied modifier
  // latches. See b/216049965 for more details.
  base::flat_map<DomCode, ui::EventFlags> previous_non_modifier_latches_;

  const raw_ptr<KeyboardCapability, DanglingUntriaged> keyboard_capability_;
  const raw_ptr<ash::input_method::ImeKeyboard> ime_keyboard_;

  // True if alt + key and mouse event remapping is allowed. In some scenario,
  // such as clicking a button in the Alt-Tab UI, this remapping undesirably
  // prevents button clicking when alt + left turns into right click. Also,
  // user needs to be able to use an up arrow key to navigate and focus
  // different component, but remapping can turn alt + up arrow into PageUp.
  bool is_alt_down_remapping_enabled_ = true;
};

}  // namespace ui

#endif  // UI_EVENTS_ASH_EVENT_REWRITER_ASH_H_
