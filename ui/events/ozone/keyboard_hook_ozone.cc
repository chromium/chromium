// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/keyboard_hook_ozone.h"

#include <utility>

#include "base/callback.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_keyboard_hook.h"

namespace ui {

KeyboardHookOzone::KeyboardHookOzone(
    PlatformKeyboardHookTypes type,
    KeyEventCallback callback,
    absl::optional<base::flat_set<DomCode>> dom_codes,
    gfx::AcceleratedWidget widget) {
  platform_keyboard_hook_ =
      ui::OzonePlatform::GetInstance()->CreateKeyboardHook(
          type, std::move(callback), std::move(dom_codes), widget);
}

KeyboardHookOzone::~KeyboardHookOzone() = default;

bool KeyboardHookOzone::IsKeyLocked(DomCode dom_code) const {
  return platform_keyboard_hook_->IsKeyLocked(dom_code);
}

// static
std::unique_ptr<KeyboardHook> KeyboardHook::CreateModifierKeyboardHook(
    absl::optional<base::flat_set<DomCode>> dom_codes,
    gfx::AcceleratedWidget accelerated_widget,
    KeyEventCallback callback) {
  return std::make_unique<KeyboardHookOzone>(
      PlatformKeyboardHookTypes::kModifier, std::move(callback),
      std::move(dom_codes), accelerated_widget);
}

// static
std::unique_ptr<KeyboardHook> KeyboardHook::CreateMediaKeyboardHook(
    KeyEventCallback callback) {
  return nullptr;
}

}  // namespace ui
