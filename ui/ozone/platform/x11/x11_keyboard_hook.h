// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_KEYBOARD_HOOK_H_
#define UI_OZONE_PLATFORM_X11_X11_KEYBOARD_HOOK_H_

#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"
#include "ui/ozone/common/base_keyboard_hook.h"

namespace ui {

class X11KeyboardHook final : public BaseKeyboardHook {
 public:
  X11KeyboardHook(std::optional<base::flat_set<DomCode>> dom_codes,
                  BaseKeyboardHook::KeyEventCallback callback,
                  gfx::AcceleratedWidget accelerated_widget);
  X11KeyboardHook(const X11KeyboardHook&) = delete;
  X11KeyboardHook& operator=(const X11KeyboardHook&) = delete;
  ~X11KeyboardHook() final;

 private:
  // Helper methods for setting up key event capture.
  void RegisterHook(const std::optional<base::flat_set<DomCode>>& dom_codes);
  void CaptureAllKeys();
  void CaptureSpecificKeys(
      const std::optional<base::flat_set<DomCode>>& dom_codes);
  void CaptureKeyForDomCode(DomCode dom_code);

  THREAD_CHECKER(thread_checker_);

  // Tracks the keys that were grabbed.
  std::vector<int> grabbed_keys_;

  // The x11 default connection and the owner's native window.
  const raw_ptr<x11::Connection> connection_ = nullptr;
  const x11::Window window_ = x11::Window::None;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_KEYBOARD_HOOK_H_
