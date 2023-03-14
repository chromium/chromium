// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_FUCHSIA_VIRTUAL_KEYBOARD_CONTROLLER_FUCHSIA_H_
#define UI_BASE_IME_FUCHSIA_VIRTUAL_KEYBOARD_CONTROLLER_FUCHSIA_H_

#include <fuchsia/input/virtualkeyboard/cpp/fidl.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>

#include "base/component_export.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/virtual_keyboard_controller.h"

namespace ui {

// Manages visibility of the onscreen keyboard.
class COMPONENT_EXPORT(UI_BASE_IME) VirtualKeyboardControllerFuchsia
    : public VirtualKeyboardController {
 public:
  // |input_method| must outlive |this|.
  VirtualKeyboardControllerFuchsia(fuchsia::ui::views::ViewRef view_ref,
                                   ui::InputMethodBase* input_method);
  ~VirtualKeyboardControllerFuchsia() override;

  VirtualKeyboardControllerFuchsia(VirtualKeyboardControllerFuchsia&) = delete;
  VirtualKeyboardControllerFuchsia operator=(
      VirtualKeyboardControllerFuchsia&) = delete;

  void UpdateTextType();

  // VirtualKeyboardController implementation.
  bool DisplayVirtualKeyboard() override;
  void DismissVirtualKeyboard() override;
  void AddObserver(VirtualKeyboardControllerObserver* observer) override;
  void RemoveObserver(VirtualKeyboardControllerObserver* observer) override;
  bool IsKeyboardVisible() override;

 private:
  // Initiates a "hanging get" request for virtual keyboard visibility.
  void WatchVisibility();

  // Handles the visibility change response from the service.
  void OnVisibilityChange(bool is_visible);

  // Gets the Fuchsia TextType corresponding to the currently focused field.
  fuchsia::input::virtualkeyboard::TextType GetFocusedTextType() const;

  ui::InputMethodBase* const input_method_;
  fuchsia::input::virtualkeyboard::TextType requested_type_ =
      fuchsia::input::virtualkeyboard::TextType::ALPHANUMERIC;
  fuchsia::input::virtualkeyboard::ControllerPtr controller_service_;
  bool keyboard_visible_ = false;
  bool requested_visible_ = false;
};

}  // namespace ui

#endif  // UI_BASE_IME_FUCHSIA_VIRTUAL_KEYBOARD_CONTROLLER_FUCHSIA_H_
