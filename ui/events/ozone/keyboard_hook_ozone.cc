// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>

#include "ui/events/event.h"
#include "ui/events/keyboard_hook.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_keyboard_hook.h"

namespace ui {

namespace {

// Ozone-specific implementation of the ui::KeyboardHook interface.
// Wraps the object provided by the current Ozone platform.
class KeyboardHookOzone final : public KeyboardHook {
 public:
  explicit KeyboardHookOzone(
      std::unique_ptr<PlatformKeyboardHook> platform_hook)
      : platform_hook_(std::move(platform_hook)) {}

  KeyboardHookOzone(const KeyboardHookOzone&) = delete;
  KeyboardHookOzone& operator=(const KeyboardHookOzone&) = delete;

  ~KeyboardHookOzone() final = default;

  // KeyboardHook:
  bool IsKeyLocked(DomCode dom_code) const final {
    return platform_hook_->IsKeyLocked(dom_code);
  }

 private:
  // The platform implementation.
  std::unique_ptr<PlatformKeyboardHook> platform_hook_;
};

}  // namespace

// static
std::unique_ptr<KeyboardHook> KeyboardHook::CreateModifierKeyboardHook(
    std::optional<base::flat_set<DomCode>> dom_codes,
    gfx::AcceleratedWidget accelerated_widget,
    KeyEventCallback callback) {
  if (auto platform_hook = OzonePlatform::GetInstance()->CreateKeyboardHook(
          PlatformKeyboardHookTypes::kModifier, std::move(callback),
          std::move(dom_codes), accelerated_widget)) {
    return std::make_unique<KeyboardHookOzone>(std::move(platform_hook));
  }
  return nullptr;
}

// static
std::unique_ptr<KeyboardHook> KeyboardHook::CreateMediaKeyboardHook(
    KeyEventCallback callback) {
  return nullptr;
}

}  // namespace ui
