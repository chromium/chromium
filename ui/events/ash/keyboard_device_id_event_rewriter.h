// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_KEYBOARD_DEVICE_ID_EVENT_REWRITER_H_
#define UI_EVENTS_ASH_KEYBOARD_DEVICE_ID_EVENT_REWRITER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_rewriter.h"

namespace ui {
class Event;
class KeyboardCapability;

// This rewriter adjusts the KeyEvent's device id (e.g. for synthesized
// repeated event), and if the adjusted id is different from the original one,
// annotates it to the rewritten event, while preserving the original
// `source_device_id` which is used in various purposes than event rewriting.
// Also, this annotates last KeyEvent's (possibly adjusted) device id to
// Mouse/Touch events, which will be used for modifier rewriting in later
// stages.
// In later stages, device id to be used for rewriting can be taken by
// looking at the property, or `source_device_id` if it is missing.
class KeyboardDeviceIdEventRewriter : public EventRewriter {
 public:
  explicit KeyboardDeviceIdEventRewriter(
      KeyboardCapability* keyboard_capability);
  KeyboardDeviceIdEventRewriter(const KeyboardDeviceIdEventRewriter&) = delete;
  KeyboardDeviceIdEventRewriter operator=(
      const KeyboardDeviceIdEventRewriter&) = delete;
  ~KeyboardDeviceIdEventRewriter() override;

  // EventRewriter:
  EventDispatchDetails RewriteEvent(const Event& event,
                                    const Continuation continuation) override;

  // This is exposed to share the code with EventRewriterAsh for transition.
  // TODO(hidehiko): hide this into private and/or merge into
  // GetKeyboardDeviceIdInternal().
  static int GetKeyboardDeviceId(int keyboard_device_id,
                                 int last_keyboard_device_id,
                                 KeyboardCapability* keyboard_capability);

 private:
  // Returns the fixed-up keyboard device id.
  // The returned id should be used in the later event rewriting stages.
  std::optional<int> GetKeyboardDeviceIdInternal(const Event& event) const;

  const raw_ptr<KeyboardCapability> keyboard_capability_;
  int last_keyboard_device_id_ = ED_UNKNOWN_DEVICE;
};

}  // namespace ui

#endif  // UI_EVENTS_ASH_KEYBOARD_DEVICE_ID_EVENT_REWRITER_H_
