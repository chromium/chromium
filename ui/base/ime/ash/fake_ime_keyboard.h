// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_FAKE_IME_KEYBOARD_H_
#define UI_BASE_IME_ASH_FAKE_IME_KEYBOARD_H_

#include <string>

#include "base/component_export.h"
#include "ui/base/ime/ash/ime_keyboard.h"

namespace ash {
namespace input_method {

class COMPONENT_EXPORT(UI_BASE_IME_ASH) FakeImeKeyboard : public ImeKeyboard {
 public:
  FakeImeKeyboard();

  FakeImeKeyboard(const FakeImeKeyboard&) = delete;
  FakeImeKeyboard& operator=(const FakeImeKeyboard&) = delete;

  ~FakeImeKeyboard() override;

  void SetCurrentKeyboardLayoutByName(
      const std::string& layout_name,
      base::OnceCallback<void(bool)> callback) override;
  bool SetAutoRepeatRate(const AutoRepeatRate& rate) override;
  void SetAutoRepeatEnabled(bool enabled) override;
  bool GetAutoRepeatEnabled() override;

  int set_current_keyboard_layout_by_name_count_;
  AutoRepeatRate last_auto_repeat_rate_;
  // TODO(yusukes): Add more variables for counting the numbers of the API calls
  bool auto_repeat_is_enabled_;
};

}  // namespace input_method
}  // namespace ash

#endif  // UI_BASE_IME_ASH_FAKE_IME_KEYBOARD_H_
