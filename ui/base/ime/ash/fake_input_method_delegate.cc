// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/fake_input_method_delegate.h"

namespace ash {
namespace input_method {

FakeInputMethodDelegate::FakeInputMethodDelegate()
    : active_locale_("en") {
}

FakeInputMethodDelegate::~FakeInputMethodDelegate() = default;

std::string FakeInputMethodDelegate::GetHardwareKeyboardLayouts() const {
  return hardware_keyboard_layout_;
}

std::u16string FakeInputMethodDelegate::GetLocalizedString(
    int resource_id) const {
  if (!get_localized_string_callback_.is_null())
    return get_localized_string_callback_.Run(resource_id);
  return std::u16string();
}

std::u16string FakeInputMethodDelegate::GetDisplayLanguageName(
    const std::string& language_code) const {
  if (!get_display_language_name_callback_.is_null())
    return get_display_language_name_callback_.Run(language_code);
  return std::u16string();
}

void FakeInputMethodDelegate::SetHardwareKeyboardLayoutForTesting(
    const std::string& layout) {
  set_hardware_keyboard_layout(layout);
}

}  // namespace input_method
}  // namespace ash
