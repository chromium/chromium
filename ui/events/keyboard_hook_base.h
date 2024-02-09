// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYBOARD_HOOK_BASE_H_
#define UI_EVENTS_KEYBOARD_HOOK_BASE_H_

#include "ui/events/keyboard_hook.h"

namespace ui {

enum class DomCode : uint32_t;
class KeyEvent;

class KeyboardHookBase : public KeyboardHook {
 public:
  KeyboardHookBase(std::optional<base::flat_set<DomCode>> dom_codes,
                   KeyEventCallback callback);

  KeyboardHookBase(const KeyboardHookBase&) = delete;
  KeyboardHookBase& operator=(const KeyboardHookBase&) = delete;

  ~KeyboardHookBase() override;

  // KeyboardHook implementation.
  bool IsKeyLocked(DomCode dom_code) const override;

  virtual bool RegisterHook();

 protected:
  // Indicates whether |dom_code| should be intercepted by the keyboard hook.
  bool ShouldCaptureKeyEvent(DomCode dom_code) const;

  // Forwards the key event using |key_event_callback_|.
  // |event| is owned by the calling method and will live until this method
  // returns.
  void ForwardCapturedKeyEvent(KeyEvent* event);

  const std::optional<base::flat_set<DomCode>>& dom_codes() {
    return dom_codes_;
  }

 private:
  // Used to forward key events.
  KeyEventCallback key_event_callback_;

  // The set of keys which should be intercepted by the keyboard hook.
  std::optional<base::flat_set<DomCode>> dom_codes_;
};

}  // namespace ui

#endif  // UI_EVENTS_KEYBOARD_HOOK_BASE_H_
