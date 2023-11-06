// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_KEYBOARD_HOOK_H_
#define UI_OZONE_PUBLIC_PLATFORM_KEYBOARD_HOOK_H_

#include "base/component_export.h"
#include "base/functional/callback.h"

namespace ui {

enum class DomCode : uint32_t;
class KeyEvent;

// Supported hook types.
enum class PlatformKeyboardHookTypes { kModifier, kMedia };

// Interface for Ozone implementations of KeyboardHook.
class COMPONENT_EXPORT(OZONE_BASE) PlatformKeyboardHook {
 public:
  using KeyEventCallback = base::RepeatingCallback<void(KeyEvent* event)>;

  PlatformKeyboardHook();
  PlatformKeyboardHook(const PlatformKeyboardHook&) = delete;
  PlatformKeyboardHook& operator=(const PlatformKeyboardHook&) = delete;
  virtual ~PlatformKeyboardHook();

  // KeyboardHook:
  virtual bool IsKeyLocked(DomCode dom_code) const = 0;
};

}  // namespace ui
#endif  // UI_OZONE_PUBLIC_PLATFORM_KEYBOARD_HOOK_H_
