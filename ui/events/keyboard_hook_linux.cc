// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keyboard_hook.h"

#include <memory>

#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/native_widget_types.h"

#if defined(USE_X11)
#include "ui/events/x/keyboard_hook_x11.h"
#endif

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#include "ui/events/ozone/keyboard_hook_ozone.h"
#include "ui/ozone/public/platform_keyboard_hook.h"
#endif

namespace ui {

// static
std::unique_ptr<KeyboardHook> KeyboardHook::CreateModifierKeyboardHook(
    absl::optional<base::flat_set<DomCode>> dom_codes,
    gfx::AcceleratedWidget accelerated_widget,
    KeyEventCallback callback) {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    return std::make_unique<KeyboardHookOzone>(
        PlatformKeyboardHookTypes::kModifier, std::move(callback),
        std::move(dom_codes), accelerated_widget);
  }
#endif
#if defined(USE_X11)
  auto keyboard_hook_x11 = std::make_unique<KeyboardHookX11>(
      std::move(dom_codes), accelerated_widget, std::move(callback));
  if (!keyboard_hook_x11->RegisterHook())
    return nullptr;
  return keyboard_hook_x11;
#else
  NOTREACHED();
  return nullptr;
#endif
}

// static
std::unique_ptr<KeyboardHook> KeyboardHook::CreateMediaKeyboardHook(
    KeyEventCallback callback) {
  return nullptr;
}

}  // namespace ui
