// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/mock_component_extension_ime_manager_delegate.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"

namespace chromeos {
namespace input_method {

MockComponentExtensionIMEManagerDelegate::
    MockComponentExtensionIMEManagerDelegate() {}

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
}  // namespace input_method
}  // namespace chromeos
