// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_KEYBOARD_HOOK_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_KEYBOARD_HOOK_OZONE_H_

#include "ui/base/x/x11_keyboard_hook.h"
#include "ui/ozone/common/base_keyboard_hook.h"

namespace ui {

class X11KeyboardHookOzone : public BaseKeyboardHook, public XKeyboardHook {
 public:
  X11KeyboardHookOzone(absl::optional<base::flat_set<DomCode>> dom_codes,
                       BaseKeyboardHook::KeyEventCallback callback,
                       gfx::AcceleratedWidget accelerated_widget);
  X11KeyboardHookOzone(const X11KeyboardHookOzone&) = delete;
  X11KeyboardHookOzone& operator=(const X11KeyboardHookOzone&) = delete;
  ~X11KeyboardHookOzone() override;

 protected:
  // PlatformKeyboardHook:
  bool IsKeyLocked(DomCode dom_code) const override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_KEYBOARD_HOOK_OZONE_H_
