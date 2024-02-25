// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SCOPED_SIMPLE_KEYBOARD_HOOK_H_
#define UI_AURA_SCOPED_SIMPLE_KEYBOARD_HOOK_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "ui/aura/scoped_keyboard_hook.h"

namespace ui {
enum class DomCode : uint32_t;
}

namespace aura {

// This subclass of ScopedKeyboardHook will not set up a system-level keyboard
// hook or call into any WindowTreeHost methods for lock state or cleanup.
// It allows for disabling system-level keyboard lock functionality while
// continuing to support browser-level keyboard lock.
// TODO(joedow): Remove this class after 'system-keyboard-lock' is removed.
class ScopedSimpleKeyboardHook : public ScopedKeyboardHook {
 public:
  explicit ScopedSimpleKeyboardHook(
      std::optional<base::flat_set<ui::DomCode>> dom_codes);

  ScopedSimpleKeyboardHook(const ScopedSimpleKeyboardHook&) = delete;
  ScopedSimpleKeyboardHook& operator=(const ScopedSimpleKeyboardHook&) = delete;

  ~ScopedSimpleKeyboardHook() override;

  // ScopedKeyboardHook override.
  bool IsKeyLocked(ui::DomCode dom_code) override;

 private:
  std::optional<base::flat_set<ui::DomCode>> dom_codes_;
};

}  // namespace aura

#endif  // UI_AURA_SCOPED_SIMPLE_KEYBOARD_HOOK_H_
