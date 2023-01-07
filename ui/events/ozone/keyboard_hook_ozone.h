// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_KEYBOARD_HOOK_OZONE_H_
#define UI_EVENTS_OZONE_KEYBOARD_HOOK_OZONE_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/keyboard_hook.h"

namespace ui {

enum class PlatformKeyboardHookTypes;
class PlatformKeyboardHook;

// Ozone-specific implementation of the KeyboardHook interface.
//
// Wraps the platform implementation spawned by the current Ozone platform.
class COMPONENT_EXPORT(KEYBOARD_HOOK) KeyboardHookOzone : public KeyboardHook {
 public:
  KeyboardHookOzone(PlatformKeyboardHookTypes type,
                    KeyEventCallback callback,
                    absl::optional<base::flat_set<DomCode>> dom_codes,
                    gfx::AcceleratedWidget widget);
  KeyboardHookOzone(const KeyboardHookOzone&) = delete;
  KeyboardHookOzone& operator=(const KeyboardHookOzone&) = delete;
  ~KeyboardHookOzone() override;

  // KeyboardHook:
  bool IsKeyLocked(DomCode dom_code) const override;

 private:
  // The platform implementation.
  std::unique_ptr<PlatformKeyboardHook> platform_keyboard_hook_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_KEYBOARD_HOOK_OZONE_H_
