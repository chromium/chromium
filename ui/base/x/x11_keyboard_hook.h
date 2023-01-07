// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_KEYBOARD_HOOK_H_
#define UI_BASE_X_X11_KEYBOARD_HOOK_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"

namespace ui {

enum class DomCode;
class KeyEvent;

// X11-specific implementation of the class that intercepts keyboard input.
class COMPONENT_EXPORT(UI_BASE_X) XKeyboardHook {
 public:
  using KeyEventCallback = base::RepeatingCallback<void(KeyEvent* event)>;

  explicit XKeyboardHook(gfx::AcceleratedWidget accelerated_widget);
  XKeyboardHook(const XKeyboardHook&) = delete;
  XKeyboardHook& operator=(const XKeyboardHook&) = delete;
  virtual ~XKeyboardHook();

 protected:
  bool RegisterHook(const absl::optional<base::flat_set<DomCode>>& dom_codes);

 private:
  // Helper methods for setting up key event capture.
  void CaptureAllKeys();
  void CaptureSpecificKeys(
      const absl::optional<base::flat_set<DomCode>>& dom_codes);
  void CaptureKeyForDomCode(DomCode dom_code);

  THREAD_CHECKER(thread_checker_);

  // Tracks the keys that were grabbed.
  std::vector<int> grabbed_keys_;

  // The x11 default connection and the owner's native window.
  const raw_ptr<x11::Connection> connection_ = nullptr;
  const x11::Window x_window_ = x11::Window::None;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_KEYBOARD_HOOK_H_
