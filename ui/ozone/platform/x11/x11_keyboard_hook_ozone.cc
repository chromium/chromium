// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_keyboard_hook_ozone.h"

namespace ui {

X11KeyboardHookOzone::X11KeyboardHookOzone(
    absl::optional<base::flat_set<DomCode>> dom_codes,
    BaseKeyboardHook::KeyEventCallback callback,
    gfx::AcceleratedWidget accelerated_widget)
    : BaseKeyboardHook(std::move(dom_codes), std::move(callback)),
      XKeyboardHook(accelerated_widget) {
  RegisterHook(this->dom_codes());
}

X11KeyboardHookOzone::~X11KeyboardHookOzone() = default;

bool X11KeyboardHookOzone::IsKeyLocked(DomCode dom_code) const {
  return BaseKeyboardHook::IsKeyLocked(dom_code);
}

}  // namespace ui
