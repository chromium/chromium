// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_FUCHSIA_VIRTUAL_KEYBOARD_CONTROLLER_FUCHSIA_H_
#define UI_BASE_IME_FUCHSIA_VIRTUAL_KEYBOARD_CONTROLLER_FUCHSIA_H_

#include <fuchsia/ui/input/cpp/fidl.h>

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/ime/virtual_keyboard_controller.h"

namespace ui {

// Manages visibility of the onscreen keyboard.
class COMPONENT_EXPORT(UI_BASE_IME) VirtualKeyboardControllerFuchsia
    : public VirtualKeyboardController {
 public:
  // |ime_service| must outlive |this|.
  explicit VirtualKeyboardControllerFuchsia(
      fuchsia::ui::input::ImeService* ime_service);

  ~VirtualKeyboardControllerFuchsia() override;

  // VirtualKeyboardController implementation.
  bool DisplayVirtualKeyboard() override;
  void DismissVirtualKeyboard() override;
  void AddObserver(VirtualKeyboardControllerObserver* observer) override;
  void RemoveObserver(VirtualKeyboardControllerObserver* observer) override;
  bool IsKeyboardVisible() override;

 private:
  fuchsia::ui::input::ImeService* const ime_service_;
  fuchsia::ui::input::ImeVisibilityServicePtr ime_visibility_;
  bool keyboard_visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardControllerFuchsia);
};

}  // namespace ui

#endif  // UI_BASE_IME_FUCHSIA_VIRTUAL_KEYBOARD_CONTROLLER_FUCHSIA_H_
