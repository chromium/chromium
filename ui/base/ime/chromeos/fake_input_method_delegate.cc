// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/fake_input_method_delegate.h"

namespace chromeos {
namespace input_method {

FakeInputMethodDelegate::FakeInputMethodDelegate()
    : active_locale_("en") {
}

FakeInputMethodDelegate::~FakeInputMethodDelegate() = default;

std::string FakeInputMethodDelegate::GetHardwareKeyboardLayouts() const {
  return hardware_keyboard_layout_;
}

base::string16 FakeInputMethodDelegate::GetLocalizedString(
    int resource_id) const {
  if (!get_localized_string_callback_.is_null())
    return get_localized_string_callback_.Run(resource_id);
  return base::string16();
}

base::string16 FakeInputMethodDelegate::GetDisplayLanguageName(
    const std::string& language_code) const {
  if (!get_display_language_name_callback_.is_null())
    return get_display_language_name_callback_.Run(language_code);
  return base::string16();
}

void FakeInputMethodDelegate::SetHardwareKeyboardLayoutForTesting(
    const std::string& layout) {
  set_hardware_keyboard_layout(layout);
}

}  // namespace input_method
}  // namespace chromeos
