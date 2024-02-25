// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_FAKE_INPUT_METHOD_DELEGATE_H_
#define UI_BASE_IME_ASH_FAKE_INPUT_METHOD_DELEGATE_H_

#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "ui/base/ime/ash/input_method_delegate.h"

namespace ash {
namespace input_method {

class COMPONENT_EXPORT(UI_BASE_IME_ASH) FakeInputMethodDelegate
    : public InputMethodDelegate {
 public:
  using GetLocalizedStringCallback =
      base::RepeatingCallback<std::u16string(int)>;

  FakeInputMethodDelegate();

  FakeInputMethodDelegate(const FakeInputMethodDelegate&) = delete;
  FakeInputMethodDelegate& operator=(const FakeInputMethodDelegate&) = delete;

  ~FakeInputMethodDelegate() override;

  // InputMethodDelegate implementation:
  std::string GetHardwareKeyboardLayouts() const override;
  std::u16string GetLocalizedString(int resource_id) const override;
  void SetHardwareKeyboardLayoutForTesting(const std::string& layout) override;

  void set_get_localized_string_callback(
      const GetLocalizedStringCallback& callback) {
    get_localized_string_callback_ = callback;
  }

 private:
  std::string hardware_keyboard_layout_;
  GetLocalizedStringCallback get_localized_string_callback_;
};

}  // namespace input_method
}  // namespace ash

#endif  // UI_BASE_IME_ASH_FAKE_INPUT_METHOD_DELEGATE_H_
