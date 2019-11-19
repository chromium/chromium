// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/fuchsia/input_method_keyboard_controller_fuchsia.h"

#include <lib/sys/cpp/component_context.h>
#include <utility>

#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"

namespace ui {

InputMethodKeyboardControllerFuchsia::InputMethodKeyboardControllerFuchsia(
    fuchsia::ui::input::ImeService* ime_service)
    : ime_service_(ime_service),
      ime_visibility_(
          base::fuchsia::ComponentContextForCurrentProcess()
              ->svc()
              ->Connect<fuchsia::ui::input::ImeVisibilityService>()) {
  DCHECK(ime_service_);

  ime_visibility_.set_error_handler([](zx_status_t status) {
    ZX_LOG(FATAL, status) << " ImeVisibilityService lost.";
  });

  ime_visibility_.events().OnKeyboardVisibilityChanged = [this](bool visible) {
    keyboard_visible_ = visible;
  };
}

InputMethodKeyboardControllerFuchsia::~InputMethodKeyboardControllerFuchsia() =
    default;

bool InputMethodKeyboardControllerFuchsia::DisplayVirtualKeyboard() {
  ime_service_->ShowKeyboard();
  return true;
}

void InputMethodKeyboardControllerFuchsia::DismissVirtualKeyboard() {
  ime_service_->HideKeyboard();
}

void InputMethodKeyboardControllerFuchsia::AddObserver(
    InputMethodKeyboardControllerObserver* observer) {
  NOTIMPLEMENTED();
}

void InputMethodKeyboardControllerFuchsia::RemoveObserver(
    InputMethodKeyboardControllerObserver* observer) {
  NOTIMPLEMENTED();
}

bool InputMethodKeyboardControllerFuchsia::IsKeyboardVisible() {
  return keyboard_visible_;
}

}  // namespace ui
