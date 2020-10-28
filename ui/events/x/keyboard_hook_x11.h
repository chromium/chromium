// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_X_KEYBOARD_HOOK_X11_H_
#define UI_EVENTS_X_KEYBOARD_HOOK_X11_H_

#include <vector>

#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "ui/events/keyboard_hook_base.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"

namespace ui {

// A default implementation for the X11 platform.
class KeyboardHookX11 : public KeyboardHookBase {
 public:
  KeyboardHookX11(base::Optional<base::flat_set<DomCode>> dom_codes,
                  gfx::AcceleratedWidget accelerated_widget,
                  KeyEventCallback callback);
  KeyboardHookX11(const KeyboardHookX11&) = delete;
  KeyboardHookX11& operator=(const KeyboardHookX11&) = delete;
  ~KeyboardHookX11() override;

  // KeyboardHookBase:
  bool RegisterHook() override;

 private:
  // Helper methods for setting up key event capture.
  void CaptureAllKeys();
  void CaptureSpecificKeys();
  void CaptureKeyForDomCode(DomCode dom_code);

  THREAD_CHECKER(thread_checker_);

  // The x11 default connection and the owner's native window.
  x11::Connection* const connection_ = nullptr;
  const x11::Window x_window_ = x11::Window::None;

  // Tracks the keys that were grabbed.
  std::vector<int> grabbed_keys_;
};

}  // namespace ui

#endif  // UI_EVENTS_X_KEYBOARD_HOOK_X11_H_
