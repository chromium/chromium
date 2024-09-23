// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_IME_KEYBOARD_IMPL_H_
#define UI_BASE_IME_ASH_IME_KEYBOARD_IMPL_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/ime/ash/ime_keyboard.h"

namespace ui {
class InputController;
}  // namespace ui

namespace ash {
namespace input_method {

// Version of ImeKeyboard used when chrome is run on device.
class COMPONENT_EXPORT(UI_BASE_IME_ASH) ImeKeyboardImpl : public ImeKeyboard {
 public:
  ImeKeyboardImpl(ui::InputController* input_controller);

  ImeKeyboardImpl(const ImeKeyboardImpl&) = delete;
  ImeKeyboardImpl& operator=(const ImeKeyboardImpl&) = delete;

  ~ImeKeyboardImpl() override;

  // ImeKeyboard:
  void SetCurrentKeyboardLayoutByName(
      const std::string& layout_name,
      base::OnceCallback<void(bool)> callback) override;
  bool SetAutoRepeatRate(const AutoRepeatRate& rate) override;
  void SetAutoRepeatEnabled(bool enabled) override;
  bool GetAutoRepeatEnabled() override;
  void SetCapsLockEnabled(bool enable_caps_lock) override;
  bool IsCapsLockEnabled() override;

 private:
  const raw_ptr<ui::InputController> input_controller_;
};

}  // namespace input_method
}  // namespace ash

#endif  // UI_BASE_IME_ASH_IME_KEYBOARD_IMPL_H_
