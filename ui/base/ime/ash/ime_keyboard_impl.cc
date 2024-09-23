// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/ime_keyboard_impl.h"

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/ozone/public/input_controller.h"

namespace ash {
namespace input_method {

ImeKeyboardImpl::ImeKeyboardImpl(ui::InputController* input_controller)
    : input_controller_(input_controller) {}

ImeKeyboardImpl::~ImeKeyboardImpl() = default;

void ImeKeyboardImpl::SetCurrentKeyboardLayoutByName(
    const std::string& layout_name,
    base::OnceCallback<void(bool)> callback) {
  const bool result =
      ImeKeyboard::SetCurrentKeyboardLayoutByNameImpl(layout_name);
  if (!result) {
    std::move(callback).Run(false);
    return;
  }

  input_controller_->SetCurrentLayoutByName(layout_name, std::move(callback));
}

bool ImeKeyboardImpl::SetAutoRepeatRate(const AutoRepeatRate& rate) {
  input_controller_->SetAutoRepeatRate(rate.initial_delay,
                                       rate.repeat_interval);
  return true;
}

void ImeKeyboardImpl::SetAutoRepeatEnabled(bool enabled) {
  input_controller_->SetAutoRepeatEnabled(enabled);
}

bool ImeKeyboardImpl::GetAutoRepeatEnabled() {
  return input_controller_->IsAutoRepeatEnabled();
}

void ImeKeyboardImpl::SetCapsLockEnabled(bool enable_caps_lock) {
  // Inform ImeKeyboard of caps lock state.
  ImeKeyboard::SetCapsLockEnabled(enable_caps_lock);
  // Inform Ozone InputController input of caps lock state.
  input_controller_->SetCapsLockEnabled(enable_caps_lock);
}

bool ImeKeyboardImpl::IsCapsLockEnabled() {
  return input_controller_->IsCapsLockEnabled();
}

}  // namespace input_method
}  // namespace ash
