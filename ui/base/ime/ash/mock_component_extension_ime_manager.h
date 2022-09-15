// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_MOCK_COMPONENT_EXTENSION_IME_MANAGER_H_
#define UI_BASE_IME_ASH_MOCK_COMPONENT_EXTENSION_IME_MANAGER_H_

#include "base/component_export.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"

namespace ash {

class COMPONENT_EXPORT(UI_BASE_IME_ASH) MockComponentExtensionIMEManager
    : public ComponentExtensionIMEManager {
 public:
  input_method::InputMethodDescriptors GetXkbIMEAsInputMethodDescriptor()
      override;
};

}  // namespace ash

#endif  // UI_BASE_IME_ASH_MOCK_COMPONENT_EXTENSION_IME_MANAGER_H_
