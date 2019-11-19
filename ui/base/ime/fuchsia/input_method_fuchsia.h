// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_FUCHSIA_INPUT_METHOD_FUCHSIA_H_
#define UI_BASE_IME_FUCHSIA_INPUT_METHOD_FUCHSIA_H_

#include <fuchsia/ui/input/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/ime/fuchsia/input_method_keyboard_controller_fuchsia.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/events/fuchsia/input_event_dispatcher.h"
#include "ui/events/fuchsia/input_event_dispatcher_delegate.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

// Handles input from physical keyboards and the IME service.
class COMPONENT_EXPORT(UI_BASE_IME_FUCHSIA) InputMethodFuchsia
    : public InputMethodBase,
      public InputEventDispatcherDelegate,
      public fuchsia::ui::input::InputMethodEditorClient {
 public:
  explicit InputMethodFuchsia(internal::InputMethodDelegate* delegate);
  ~InputMethodFuchsia() override;

  fuchsia::ui::input::ImeService* ime_service() const {
    return ime_service_.get();
  }

  // InputMethodBase interface implementation.
  InputMethodKeyboardController* GetInputMethodKeyboardController() override;
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) override;
  void OnCaretBoundsChanged(const TextInputClient* client) override;
  void CancelComposition(const TextInputClient* client) override;
  bool IsCandidatePopupOpen() const override;
  void OnFocus() override;
  void OnBlur() override;

 private:
  // Establishes a connection to the input service and starts receiving input
  // events from hard and soft keyboards.
  void ConnectInputService();

  // Terminates the connection to the input services, which stops receiving
  // input events.
  void DisconnectInputService();

  // InputEventDispatcherDelegate interface implementation.
  void DispatchEvent(ui::Event* event) override;

  // InputMethodEditorClient interface implementation.
  void DidUpdateState(
      fuchsia::ui::input::TextInputState state,
      std::unique_ptr<fuchsia::ui::input::InputEvent> input_event) override;
  void OnAction(fuchsia::ui::input::InputMethodAction action) override;

  InputEventDispatcher event_converter_;
  fidl::Binding<fuchsia::ui::input::InputMethodEditorClient>
      ime_client_binding_;
  fuchsia::ui::input::ImeServicePtr ime_service_;
  fuchsia::ui::input::InputMethodEditorPtr ime_;
  fuchsia::ui::input::ImeVisibilityServicePtr ime_visibility_;
  InputMethodKeyboardControllerFuchsia virtual_keyboard_controller_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodFuchsia);
};

}  // namespace ui

#endif  // UI_BASE_IME_FUCHSIA_INPUT_METHOD_FUCHSIA_H_
