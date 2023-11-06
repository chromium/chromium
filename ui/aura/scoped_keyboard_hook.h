// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SCOPED_KEYBOARD_HOOK_H_
#define UI_AURA_SCOPED_KEYBOARD_HOOK_H_

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "ui/aura/aura_export.h"

namespace ui {
enum class DomCode : uint32_t;
}

namespace aura {

class WindowTreeHost;

// Destroying an instance of this class will clean up the KeyboardHook instance
// owned by WindowTreeHost and prevent future system key events from being
// captured.  If the KeyboardHook or WindowTreeHost instances were already
// destroyed, then destroying this instance is a noop.
class AURA_EXPORT ScopedKeyboardHook {
 public:
  explicit ScopedKeyboardHook(base::WeakPtr<WindowTreeHost> weak_ptr);

  ScopedKeyboardHook(const ScopedKeyboardHook&) = delete;
  ScopedKeyboardHook& operator=(const ScopedKeyboardHook&) = delete;

  virtual ~ScopedKeyboardHook();

  // True if |dom_code| is reserved for an active KeyboardLock request.
  virtual bool IsKeyLocked(ui::DomCode dom_code);

 protected:
  ScopedKeyboardHook();

 private:
  THREAD_CHECKER(thread_checker_);

  base::WeakPtr<WindowTreeHost> window_tree_host_;
};

}  // namespace aura

#endif  // UI_AURA_SCOPED_KEYBOARD_HOOK_H_
