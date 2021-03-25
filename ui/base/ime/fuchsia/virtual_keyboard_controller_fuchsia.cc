// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/fuchsia/virtual_keyboard_controller_fuchsia.h"

#include <lib/fit/function.h>
#include <lib/sys/cpp/component_context.h>
#include <utility>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"

namespace ui {
namespace {

// Returns the FIDL enum representation of the current InputMode.
fuchsia::input::virtualkeyboard::TextType ConvertTextInputMode(
    ui::TextInputMode mode) {
  switch (mode) {
    case TEXT_INPUT_MODE_NUMERIC:
    case TEXT_INPUT_MODE_DECIMAL:
      return fuchsia::input::virtualkeyboard::TextType::NUMERIC;

    case TEXT_INPUT_MODE_TEL:
      return fuchsia::input::virtualkeyboard::TextType::PHONE;

    case TEXT_INPUT_MODE_DEFAULT:
    case TEXT_INPUT_MODE_NONE:
    case TEXT_INPUT_MODE_TEXT:
    case TEXT_INPUT_MODE_URL:
    case TEXT_INPUT_MODE_EMAIL:
    case TEXT_INPUT_MODE_SEARCH:
      return fuchsia::input::virtualkeyboard::TextType::ALPHANUMERIC;
  }
}

}  // namespace

VirtualKeyboardControllerFuchsia::VirtualKeyboardControllerFuchsia(
    fuchsia::ui::views::ViewRef view_ref,
    ui::InputMethodBase* input_method)
    : input_method_(input_method) {
  DCHECK(input_method_);

  base::ComponentContextForProcess()
      ->svc()
      ->Connect<fuchsia::input::virtualkeyboard::ControllerCreator>()
      ->Create(std::move(view_ref),
               ConvertTextInputMode(input_method_->GetTextInputMode()),
               controller_service_.NewRequest());

  controller_service_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "virtualkeyboard::Controller disconnected";
    keyboard_visible_ = false;
  });

  WatchVisibility();
}

VirtualKeyboardControllerFuchsia::~VirtualKeyboardControllerFuchsia() = default;

bool VirtualKeyboardControllerFuchsia::DisplayVirtualKeyboard() {
  if (!controller_service_)
    return false;

  controller_service_->SetTextType(
      ConvertTextInputMode(input_method_->GetTextInputMode()));

  if (!keyboard_visible_)
    controller_service_->RequestShow();

  return true;
}

void VirtualKeyboardControllerFuchsia::DismissVirtualKeyboard() {
  if (!controller_service_)
    return;

  controller_service_->RequestHide();
}

void VirtualKeyboardControllerFuchsia::AddObserver(
    VirtualKeyboardControllerObserver* observer) {}

void VirtualKeyboardControllerFuchsia::RemoveObserver(
    VirtualKeyboardControllerObserver* observer) {}

bool VirtualKeyboardControllerFuchsia::IsKeyboardVisible() {
  return keyboard_visible_;
}

void VirtualKeyboardControllerFuchsia::WatchVisibility() {
  if (!controller_service_)
    return;

  controller_service_->WatchVisibility(fit::bind_member(
      this, &VirtualKeyboardControllerFuchsia::OnVisibilityChange));
}

void VirtualKeyboardControllerFuchsia::OnVisibilityChange(bool is_visible) {
  keyboard_visible_ = is_visible;
  WatchVisibility();
}

}  // namespace ui
