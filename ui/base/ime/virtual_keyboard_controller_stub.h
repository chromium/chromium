// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_VIRTUAL_KEYBOARD_CONTROLLER_STUB_H_
#define UI_BASE_IME_VIRTUAL_KEYBOARD_CONTROLLER_STUB_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/ime/virtual_keyboard_controller.h"

namespace ui {

// This class provides a stub VirtualKeyboardController.
class COMPONENT_EXPORT(UI_BASE_IME) VirtualKeyboardControllerStub final
    : public VirtualKeyboardController {
 public:
  VirtualKeyboardControllerStub();
  ~VirtualKeyboardControllerStub() override;

  // VirtualKeyboardController overrides.
  bool DisplayVirtualKeyboard() override;
  void DismissVirtualKeyboard() override;
  void AddObserver(VirtualKeyboardControllerObserver* observer) override;
  void RemoveObserver(VirtualKeyboardControllerObserver* observer) override;
  bool IsKeyboardVisible() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardControllerStub);
};

}  // namespace ui

#endif  // UI_BASE_IME_VIRTUAL_KEYBOARD_CONTROLLER_STUB_H_
