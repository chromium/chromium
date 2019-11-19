// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_KEYBOARD_CONTROLLER_H_
#define UI_BASE_IME_INPUT_METHOD_KEYBOARD_CONTROLLER_H_

#include "base/component_export.h"

namespace ui {

class InputMethodKeyboardControllerObserver;

// This class provides functionality to display the on screen keyboard and
// add observers to observe changes in it.
class COMPONENT_EXPORT(UI_BASE_IME) InputMethodKeyboardController {
 public:
  virtual ~InputMethodKeyboardController() = default;

  // Attempt to display the keyboard.
  virtual bool DisplayVirtualKeyboard() = 0;

  // Attempt to dismiss the keyboard
  virtual void DismissVirtualKeyboard() = 0;

  // Adds a registered observer.
  virtual void AddObserver(InputMethodKeyboardControllerObserver* observer) = 0;

  // Removes a registered observer.
  virtual void RemoveObserver(
      InputMethodKeyboardControllerObserver* observer) = 0;

  // Returns true if the virtual keyboard is currently visible.
  virtual bool IsKeyboardVisible() = 0;

 protected:
  InputMethodKeyboardController() = default;
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_KEYBOARD_CONTROLLER_H_
