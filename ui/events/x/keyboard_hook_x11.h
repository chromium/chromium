// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_X_KEYBOARD_HOOK_X11_H_
#define UI_EVENTS_X_KEYBOARD_HOOK_X11_H_

#include "base/threading/thread_checker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/x/x11_keyboard_hook.h"
#include "ui/events/keyboard_hook_base.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"

namespace ui {

// A default implementation for the X11 platform.
class KeyboardHookX11 : public KeyboardHookBase, public XKeyboardHook {
 public:
  KeyboardHookX11(absl::optional<base::flat_set<DomCode>> dom_codes,
                  gfx::AcceleratedWidget accelerated_widget,
                  KeyboardHookBase::KeyEventCallback callback);
  KeyboardHookX11(const KeyboardHookX11&) = delete;
  KeyboardHookX11& operator=(const KeyboardHookX11&) = delete;
  ~KeyboardHookX11() override;

  // KeyboardHookBase:
  bool RegisterHook() override;
};

}  // namespace ui

#endif  // UI_EVENTS_X_KEYBOARD_HOOK_X11_H_
