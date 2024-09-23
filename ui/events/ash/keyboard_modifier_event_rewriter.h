// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_KEYBOARD_MODIFIER_EVENT_REWRITER_H_
#define UI_EVENTS_ASH_KEYBOARD_MODIFIER_EVENT_REWRITER_H_

#include <map>
#include <memory>
#include <variant>

#include "base/memory/raw_ptr.h"
#include "ui/events/ash/event_rewriter_utils.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/ash/mojom/modifier_key.mojom.h"
#include "ui/events/event_rewriter.h"

namespace ash::input_method {
class ImeKeyboard;
}  // namespace ash::input_method

namespace ui {

class KeyboardCapability;
class KeyboardLayoutEngine;

// This rewriter remaps modifier key events based on settings/preferences.
// Also, updates modifier flags along with the remapping not only
// for KeyEvent instances but also motion Event instances, such as mouse
// events and touch events.
class KeyboardModifierEventRewriter : public EventRewriter {
 public:
  // UnmappedCode are keys which do not have a corresponding DomCode for their
  // meaning which are used for modifier remapping.
  enum class UnmappedCode {
    kRightAlt,
  };

  using PhysicalCode = std::variant<DomCode, UnmappedCode>;
  struct RemappedKey {
    PhysicalCode code;
    DomKey key;
    KeyboardCode key_code;
  };

  // Delegate interface to inject chrome dependencies.
  class Delegate {
   public:
    virtual ~Delegate() = default;

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

    // Returns true only if the the key event was rewritten to ALTGR. For most
    // cases, it is expected that this function returns false as most key events
    // do not involve ALTGR. Returns false if SuppressModifierKeyRewrites was
    // called to suppress modifier rewrites.
    virtual bool RewriteModifierKeys() = 0;
  };

  KeyboardModifierEventRewriter(std::unique_ptr<Delegate> delegate,
                                KeyboardLayoutEngine* keyboard_layout_engine,
                                KeyboardCapability* keyboard_capability,
                                ash::input_method::ImeKeyboard* ime_keyboard);
  KeyboardModifierEventRewriter(const KeyboardModifierEventRewriter&) = delete;
  KeyboardModifierEventRewriter& operator=(
      const KeyboardModifierEventRewriter&) = delete;
  ~KeyboardModifierEventRewriter() override;

  // EventRewriter:
  EventDispatchDetails RewriteEvent(const Event& event,
                                    const Continuation continuation) override;

 private:
  std::unique_ptr<Event> RewritePressKeyEvent(const KeyEvent& event);
  std::optional<RemappedKey> RemapPressKey(const KeyEvent& event);
  std::unique_ptr<Event> RewriteReleaseKeyEvent(const KeyEvent& event);
  std::unique_ptr<KeyEvent> BuildRewrittenEvent(const KeyEvent& event,
                                                const RemappedKey& remapped);
  int RewriteModifierFlags(int flags) const;

  std::optional<PhysicalCode> GetRemappedPhysicalCode(DomCode code,
                                                      int device_id) const;

  std::unique_ptr<Delegate> delegate_;
  const raw_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  const raw_ptr<KeyboardCapability> keyboard_capability_;
  const raw_ptr<ash::input_method::ImeKeyboard> ime_keyboard_;

  // Map from physical keys to the affected modifiers on press.
  std::map<internal::PhysicalKey, RemappedKey> remapped_keys_;
  std::map<internal::PhysicalKey, EventFlags> pressed_modifier_keys_;
  bool altgr_latch_ = false;
};

}  // namespace ui

#endif  // UI_EVENTS_ASH_KEYBOARD_MODIFIER_EVENT_REWRITER_H_
