// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_MOCK_CHROMEOS_COMPONENT_EXTENSION_IME_MANAGER_H_
#define UI_BASE_IME_MOCK_CHROMEOS_COMPONENT_EXTENSION_IME_MANAGER_H_

#include "base/component_export.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"

namespace chromeos {

class COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) MockComponentExtensionIMEManager
    : public ComponentExtensionIMEManager {
 public:
  input_method::InputMethodDescriptors GetXkbIMEAsInputMethodDescriptor()
      override;
};

}  // namespace chromeos

#endif  //  UI_BASE_IME_MOCK_CHROMEOS_COMPONENT_EXTENSION_IME_MANAGER_H_
