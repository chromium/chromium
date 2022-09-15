// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/keyboard_hook_x11.h"

namespace ui {

KeyboardHookX11::KeyboardHookX11(
    absl::optional<base::flat_set<DomCode>> dom_codes,
    gfx::AcceleratedWidget accelerated_widget,
    KeyboardHookBase::KeyEventCallback callback)
    : KeyboardHookBase(std::move(dom_codes), std::move(callback)),
      XKeyboardHook(accelerated_widget) {}

KeyboardHookX11::~KeyboardHookX11() = default;

bool KeyboardHookX11::RegisterHook() {
  return XKeyboardHook::RegisterHook(dom_codes());
}

}  // namespace ui
