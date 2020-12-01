// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/mock_component_extension_ime_manager_delegate.h"
#include "chromeos/ime/input_methods.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"

namespace chromeos {
namespace input_method {

MockComponentExtensionIMEManagerDelegate::
    MockComponentExtensionIMEManagerDelegate() {
  for (const auto& input_method : input_method::kInputMethods) {
    if (input_method.is_login_keyboard) {
      login_layout_set_.insert(input_method.xkb_layout_id);
    }
  }
}

MockComponentExtensionIMEManagerDelegate::
    ~MockComponentExtensionIMEManagerDelegate() = default;

std::vector<ComponentExtensionIME>
MockComponentExtensionIMEManagerDelegate::ListIME() {
  return ime_list_;
}

void MockComponentExtensionIMEManagerDelegate::Load(
    Profile* profile,
    const std::string& extension_id,
    const std::string& manifest,
    const base::FilePath& path) {}

bool MockComponentExtensionIMEManagerDelegate::IsInLoginLayoutAllowlist(
    const std::vector<std::string>& layouts) {
  for (const auto& layout : layouts) {
    if (login_layout_set_.find(layout) != login_layout_set_.end()) {
      return true;
    }
  }
  return false;
}

}  // namespace input_method
}  // namespace chromeos
