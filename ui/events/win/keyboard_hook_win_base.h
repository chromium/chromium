// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_WIN_KEYBOARD_HOOK_WIN_BASE_H_
#define UI_EVENTS_WIN_KEYBOARD_HOOK_WIN_BASE_H_

#include <windows.h>

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/threading/thread_checker.h"
#include "ui/events/event.h"
#include "ui/events/keyboard_hook_base.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ui {

// Exposes a method to drive the Windows KeyboardHook implementation by feeding
// it key event data.  This method is used by both the low-level keyboard hook
// and by unit tests which simulate the hooked behavior w/o actually installing
// a hook (doing so would cause problems with test parallelization).
class COMPONENT_EXPORT(KEYBOARD_HOOK) KeyboardHookWinBase
    : public KeyboardHookBase {
 public:
  KeyboardHookWinBase(std::optional<base::flat_set<DomCode>> dom_codes,
                      KeyEventCallback callback,
                      bool enable_hook_registration);

  KeyboardHookWinBase(const KeyboardHookWinBase&) = delete;
  KeyboardHookWinBase& operator=(const KeyboardHookWinBase&) = delete;

  ~KeyboardHookWinBase() override;

  // Create a KeyboardHookWinBase instance which does not register a
  // low-level hook and captures modifier keys.
  static std::unique_ptr<KeyboardHookWinBase>
  CreateModifierKeyboardHookForTesting(
      std::optional<base::flat_set<DomCode>> dom_codes,
      KeyEventCallback callback);

  // Create a KeyboardHookWinBase instance which does not register a
  // low-level hook and captures media keys.
  static std::unique_ptr<KeyboardHookWinBase> CreateMediaKeyboardHookForTesting(
      KeyEventCallback callback);

  // Called when a key event message is delivered via the low-level hook.
  // Exposed here to allow for testing w/o engaging the low-level hook.
  // Returns true if the message was handled.
  virtual bool ProcessKeyEventMessage(WPARAM w_param,
                                      DWORD vk,
                                      DWORD scan_code,
                                      DWORD time_stamp) = 0;

 protected:
  bool Register(HOOKPROC hook_proc);
  bool enable_hook_registration() const { return enable_hook_registration_; }

  static LRESULT CALLBACK ProcessKeyEvent(KeyboardHookWinBase* instance,
                                          int code,
                                          WPARAM w_param,
                                          LPARAM l_param);

 private:
  const bool enable_hook_registration_ = true;
  HHOOK hook_ = nullptr;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace ui

#endif  // UI_EVENTS_WIN_KEYBOARD_HOOK_WIN_BASE_H_
