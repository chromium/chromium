// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/fake_ime_keyboard.h"

#include "base/functional/callback.h"

namespace ash {
namespace input_method {

FakeImeKeyboard::FakeImeKeyboard()
    : set_current_keyboard_layout_by_name_count_(0),
      auto_repeat_is_enabled_(false) {}

FakeImeKeyboard::~FakeImeKeyboard() = default;

void FakeImeKeyboard::SetCurrentKeyboardLayoutByName(
    const std::string& layout_name,
    base::OnceCallback<void(bool)> callback) {
  ++set_current_keyboard_layout_by_name_count_;
  std::move(callback).Run(
      ImeKeyboard::SetCurrentKeyboardLayoutByNameImpl(layout_name));
}

bool FakeImeKeyboard::SetAutoRepeatRate(const AutoRepeatRate& rate) {
  last_auto_repeat_rate_ = rate;
  return true;
}

void FakeImeKeyboard::SetAutoRepeatEnabled(bool enabled) {
  auto_repeat_is_enabled_ = enabled;
}

bool FakeImeKeyboard::GetAutoRepeatEnabled() {
  return auto_repeat_is_enabled_;
}

}  // namespace input_method
}  // namespace ash
