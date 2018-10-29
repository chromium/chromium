// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_WIN_KEYBOARD_HOOK_WIN_H_
#define UI_EVENTS_WIN_KEYBOARD_HOOK_WIN_H_

#include <memory>

#include <windows.h>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/events/event.h"
#include "ui/events/events_export.h"
#include "ui/events/keyboard_hook_base.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ui {

// Exposes a method to drive the Windows KeyboardHook implementation by feeding
// it key event data.  This method is used by both the low-level keyboard hook
// and by unit tests which simulate the hooked behavior w/o actually installing
// a hook (doing so would cause problems with test parallelization).
class EVENTS_EXPORT KeyboardHookWin : public KeyboardHookBase {
 public:
  KeyboardHookWin(base::Optional<base::flat_set<DomCode>> dom_codes,
                  KeyEventCallback callback);
  ~KeyboardHookWin() override;

  // Create a KeyboardHookWin instance which does not register a low-level hook.
  static std::unique_ptr<KeyboardHookWin> CreateForTesting(
      base::Optional<base::flat_set<DomCode>> dom_codes,
      KeyEventCallback callback);

  // Called when a key event message is delivered via the low-level hook.
  // Exposed here to allow for testing w/o engaging the low-level hook.
  // Returns true if the message was handled.
  virtual bool ProcessKeyEventMessage(WPARAM w_param,
                                      DWORD vk,
                                      DWORD scan_code,
                                      DWORD time_stamp) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardHookWin);
};

}  // namespace ui

#endif  // UI_EVENTS_WIN_KEYBOARD_HOOK_WIN_H_
