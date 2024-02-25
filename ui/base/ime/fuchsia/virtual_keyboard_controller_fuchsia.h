// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_FUCHSIA_VIRTUAL_KEYBOARD_CONTROLLER_FUCHSIA_H_
#define UI_BASE_IME_FUCHSIA_VIRTUAL_KEYBOARD_CONTROLLER_FUCHSIA_H_

#include <fidl/fuchsia.input.virtualkeyboard/cpp/fidl.h>
#include <fidl/fuchsia.ui.views/cpp/fidl.h>

#include "base/component_export.h"
#include "base/fuchsia/fidl_event_handler.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/virtual_keyboard_controller.h"

namespace ui {

// Manages visibility of the onscreen keyboard.
class COMPONENT_EXPORT(UI_BASE_IME) VirtualKeyboardControllerFuchsia
    : public VirtualKeyboardController {
 public:
  // |input_method| must outlive |this|.
  VirtualKeyboardControllerFuchsia(fuchsia_ui_views::ViewRef view_ref,
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
  void OnVisibilityChange(
      const fidl::Result<
          fuchsia_input_virtualkeyboard::Controller::WatchVisibility>& result);

  // Gets the Fuchsia TextType corresponding to the currently focused field.
  fuchsia_input_virtualkeyboard::TextType GetFocusedTextType() const;

  ui::InputMethodBase* const input_method_;
  fuchsia_input_virtualkeyboard::TextType requested_type_ =
      fuchsia_input_virtualkeyboard::TextType::kAlphanumeric;
  fidl::Client<fuchsia_input_virtualkeyboard::Controller> controller_client_;
  base::FidlErrorEventLogger<fuchsia_input_virtualkeyboard::Controller>
      controller_error_logger_{"fuchsia.input.virtualkeyboard.Controller"};
  bool keyboard_visible_ = false;
  bool requested_visible_ = false;
};

}  // namespace ui

#endif  // UI_BASE_IME_FUCHSIA_VIRTUAL_KEYBOARD_CONTROLLER_FUCHSIA_H_
