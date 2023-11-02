// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/ime_keyboard_impl.h"

#include "base/time/time.h"
#include "ui/ozone/public/input_controller.h"

namespace ash {
namespace input_method {

ImeKeyboardImpl::ImeKeyboardImpl(ui::InputController* input_controller)
    : input_controller_(input_controller) {}

ImeKeyboardImpl::~ImeKeyboardImpl() = default;

bool ImeKeyboardImpl::SetCurrentKeyboardLayoutByName(
    const std::string& layout_name) {
  ImeKeyboard::SetCurrentKeyboardLayoutByName(layout_name);
  last_layout_ = layout_name;
  input_controller_->SetCurrentLayoutByName(layout_name);
  return true;
}

bool ImeKeyboardImpl::SetAutoRepeatRate(const AutoRepeatRate& rate) {
  input_controller_->SetAutoRepeatRate(
      base::Milliseconds(rate.initial_delay_in_ms),
      base::Milliseconds(rate.repeat_interval_in_ms));
  return true;
}

bool ImeKeyboardImpl::SetAutoRepeatEnabled(bool enabled) {
  input_controller_->SetAutoRepeatEnabled(enabled);
  return true;
}

bool ImeKeyboardImpl::GetAutoRepeatEnabled() {
  return input_controller_->IsAutoRepeatEnabled();
}

bool ImeKeyboardImpl::ReapplyCurrentKeyboardLayout() {
  return SetCurrentKeyboardLayoutByName(last_layout_);
}

void ImeKeyboardImpl::ReapplyCurrentModifierLockStatus() {}

void ImeKeyboardImpl::DisableNumLock() {
  input_controller_->SetNumLockEnabled(false);
}

void ImeKeyboardImpl::SetCapsLockEnabled(bool enable_caps_lock) {
  // Inform ImeKeyboard of caps lock state.
  ImeKeyboard::SetCapsLockEnabled(enable_caps_lock);
  // Inform Ozone InputController input of caps lock state.
  input_controller_->SetCapsLockEnabled(enable_caps_lock);
}

bool ImeKeyboardImpl::CapsLockIsEnabled() {
  return input_controller_->IsCapsLockEnabled();
}

}  // namespace input_method
}  // namespace ash
