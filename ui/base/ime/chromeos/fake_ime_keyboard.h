// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_FAKE_IME_KEYBOARD_H_
#define UI_BASE_IME_CHROMEOS_FAKE_IME_KEYBOARD_H_

#include "ui/base/ime/chromeos/ime_keyboard.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"

namespace chromeos {
namespace input_method {

class COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) FakeImeKeyboard
    : public ImeKeyboard {
 public:
  FakeImeKeyboard();
  ~FakeImeKeyboard() override;

  bool SetCurrentKeyboardLayoutByName(const std::string& layout_name) override;
  bool SetAutoRepeatRate(const AutoRepeatRate& rate) override;
  bool SetAutoRepeatEnabled(bool enabled) override;
  bool GetAutoRepeatEnabled() override;
  bool ReapplyCurrentKeyboardLayout() override;
  void ReapplyCurrentModifierLockStatus() override;
  void DisableNumLock() override;
  bool IsISOLevel5ShiftAvailable() const override;
  bool IsAltGrAvailable() const override;

  int set_current_keyboard_layout_by_name_count_;
  AutoRepeatRate last_auto_repeat_rate_;
  // TODO(yusukes): Add more variables for counting the numbers of the API calls
  bool auto_repeat_is_enabled_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeImeKeyboard);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_FAKE_IME_KEYBOARD_H_
