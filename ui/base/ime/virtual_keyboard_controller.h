// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_VIRTUAL_KEYBOARD_CONTROLLER_H_
#define UI_BASE_IME_VIRTUAL_KEYBOARD_CONTROLLER_H_

#include "base/component_export.h"

namespace ui {

class VirtualKeyboardControllerObserver;

// This class provides functionality to display the on screen keyboard and
// add observers to observe changes in it.
class COMPONENT_EXPORT(UI_BASE_IME) VirtualKeyboardController {
 public:
  virtual ~VirtualKeyboardController() = default;

  // Attempt to display the keyboard.
  virtual bool DisplayVirtualKeyboard() = 0;

  // Attempt to dismiss the keyboard
  virtual void DismissVirtualKeyboard() = 0;

  // Adds a registered observer.
  virtual void AddObserver(VirtualKeyboardControllerObserver* observer) = 0;

  // Removes a registered observer.
  virtual void RemoveObserver(VirtualKeyboardControllerObserver* observer) = 0;

  // Returns true if the virtual keyboard is currently visible.
  virtual bool IsKeyboardVisible() = 0;

 protected:
  VirtualKeyboardController() = default;
};

}  // namespace ui

#endif  // UI_BASE_IME_VIRTUAL_KEYBOARD_CONTROLLER_H_
