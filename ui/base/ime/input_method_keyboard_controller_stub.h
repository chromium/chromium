// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_KEYBOARD_CONTROLLER_STUB_H_
#define UI_BASE_IME_INPUT_METHOD_KEYBOARD_CONTROLLER_STUB_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/ime/input_method_keyboard_controller.h"

namespace ui {

// This class provides a stub InputMethodKeyboardController.
class COMPONENT_EXPORT(UI_BASE_IME) InputMethodKeyboardControllerStub final
    : public InputMethodKeyboardController {
 public:
  InputMethodKeyboardControllerStub();
  ~InputMethodKeyboardControllerStub() override;

  // InputMethodKeyboardController overrides.
  bool DisplayVirtualKeyboard() override;
  void DismissVirtualKeyboard() override;
  void AddObserver(InputMethodKeyboardControllerObserver* observer) override;
  void RemoveObserver(InputMethodKeyboardControllerObserver* observer) override;
  bool IsKeyboardVisible() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodKeyboardControllerStub);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_KEYBOARD_CONTROLLER_STUB_H_
