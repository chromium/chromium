// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_FAKE_INPUT_METHOD_DELEGATE_H_
#define UI_BASE_IME_CHROMEOS_FAKE_INPUT_METHOD_DELEGATE_H_

#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/ime/chromeos/input_method_delegate.h"

namespace chromeos {
namespace input_method {

class COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) FakeInputMethodDelegate
    : public InputMethodDelegate {
 public:
  typedef base::RepeatingCallback<base::string16(
      const std::string& language_code)>
      LanguageNameLocalizationCallback;
  typedef base::RepeatingCallback<base::string16(int resource_id)>
      GetLocalizedStringCallback;

  FakeInputMethodDelegate();
  ~FakeInputMethodDelegate() override;

  // InputMethodDelegate implementation:
  std::string GetHardwareKeyboardLayouts() const override;
  base::string16 GetLocalizedString(int resource_id) const override;
  void SetHardwareKeyboardLayoutForTesting(const std::string& layout) override;
  base::string16 GetDisplayLanguageName(
      const std::string& language_code) const override;

  void set_hardware_keyboard_layout(const std::string& value) {
    hardware_keyboard_layout_ = value;
  }

  void set_active_locale(const std::string& value) {
    active_locale_ = value;
  }

  void set_get_display_language_name_callback(
      const LanguageNameLocalizationCallback& callback) {
    get_display_language_name_callback_ = callback;
  }

  void set_get_localized_string_callback(
      const GetLocalizedStringCallback& callback) {
    get_localized_string_callback_ = callback;
  }

 private:
  std::string hardware_keyboard_layout_;
  std::string active_locale_;
  LanguageNameLocalizationCallback get_display_language_name_callback_;
  GetLocalizedStringCallback get_localized_string_callback_;
  DISALLOW_COPY_AND_ASSIGN(FakeInputMethodDelegate);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_FAKE_INPUT_METHOD_DELEGATE_H_
