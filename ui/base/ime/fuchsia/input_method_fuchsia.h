// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_FUCHSIA_INPUT_METHOD_FUCHSIA_H_
#define UI_BASE_IME_FUCHSIA_INPUT_METHOD_FUCHSIA_H_

#include <fidl/fuchsia.ui.views/cpp/common_types.h>
#include <lib/fidl/cpp/binding.h>

#include <optional>

#include "base/component_export.h"
#include "ui/base/ime/fuchsia/virtual_keyboard_controller_fuchsia.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/events/fuchsia/input_event_sink.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

// Handles input from physical keyboards and the IME service.
class COMPONENT_EXPORT(UI_BASE_IME_FUCHSIA) InputMethodFuchsia
    : public InputMethodBase {
 public:
  InputMethodFuchsia(bool enable_virtual_keyboard,
                     ImeKeyEventDispatcher* ime_key_event_dispatcher,
                     fuchsia_ui_views::ViewRef view_ref);
  ~InputMethodFuchsia() override;

  InputMethodFuchsia(InputMethodFuchsia&) = delete;
  InputMethodFuchsia operator=(InputMethodFuchsia&) = delete;

  // InputMethodBase interface implementation.
  VirtualKeyboardController* GetVirtualKeyboardController() final;
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) final;
  void CancelComposition(const TextInputClient* client) final;
  void OnTextInputTypeChanged(TextInputClient* client) final;
  void OnCaretBoundsChanged(const TextInputClient* client) final;
  bool IsCandidatePopupOpen() const final;

 private:
  std::optional<VirtualKeyboardControllerFuchsia> virtual_keyboard_controller_;
};

}  // namespace ui

#endif  // UI_BASE_IME_FUCHSIA_INPUT_METHOD_FUCHSIA_H_
