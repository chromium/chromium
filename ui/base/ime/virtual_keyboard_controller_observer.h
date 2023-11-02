// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_VIRTUAL_KEYBOARD_CONTROLLER_OBSERVER_H_
#define UI_BASE_IME_VIRTUAL_KEYBOARD_CONTROLLER_OBSERVER_H_

#include "base/component_export.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {

// This observer class provides a method to observe on screen
// keyboard changes.
class COMPONENT_EXPORT(UI_BASE_IME) VirtualKeyboardControllerObserver {
 public:
  // The |keyboard_rect| parameter contains the bounds of the keyboard in dips.
  virtual void OnKeyboardVisible(const gfx::Rect& keyboard_rect) = 0;
  virtual void OnKeyboardHidden() = 0;

 protected:
  virtual ~VirtualKeyboardControllerObserver() = default;
};

}  // namespace ui

#endif  // UI_BASE_IME_VIRTUAL_KEYBOARD_CONTROLLER_OBSERVER_H_
