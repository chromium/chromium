// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/fuchsia/virtual_keyboard_controller_fuchsia.h"

#include <lib/fit/function.h>
#include <lib/sys/cpp/component_context.h>
#include <utility>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "ui/base/ime/text_input_client.h"

namespace ui {

VirtualKeyboardControllerFuchsia::VirtualKeyboardControllerFuchsia(
    fuchsia::ui::views::ViewRef view_ref,
    ui::InputMethodBase* input_method)
    : input_method_(input_method) {
  DCHECK(input_method_);

  base::ComponentContextForProcess()
      ->svc()
      ->Connect<fuchsia::input::virtualkeyboard::ControllerCreator>()
      ->Create(std::move(view_ref), requested_type_,
               controller_service_.NewRequest());

  controller_service_.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "virtualkeyboard::Controller disconnected";
  });

  WatchVisibility();
}

VirtualKeyboardControllerFuchsia::~VirtualKeyboardControllerFuchsia() = default;

bool VirtualKeyboardControllerFuchsia::DisplayVirtualKeyboard() {
  DVLOG(1) << "DisplayVirtualKeyboard (visible= " << keyboard_visible_ << ")";

  if (!controller_service_)
    return false;

  UpdateTextType();

  requested_visible_ = true;
  controller_service_->RequestShow();

  return true;
}

void VirtualKeyboardControllerFuchsia::DismissVirtualKeyboard() {
  DVLOG(1) << "DismissVirtualKeyboard (visible= " << keyboard_visible_ << ")";

  if (!controller_service_)
    return;

  if (!requested_visible_ && !keyboard_visible_)
    return;

  requested_visible_ = false;
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
  DVLOG(1) << "OnVisibilityChange " << is_visible;
  keyboard_visible_ = is_visible;
  WatchVisibility();
}

// Returns the FIDL enum representation of the current InputMode.
fuchsia::input::virtualkeyboard::TextType
VirtualKeyboardControllerFuchsia::GetFocusedTextType() const {
  TextInputClient* client = input_method_->GetTextInputClient();
  // This function should only be called when there's focus, so there should
  // always be a TextInputClient.
  DCHECK(client);

  switch (client->GetTextInputMode()) {
    case TEXT_INPUT_MODE_NUMERIC:
    case TEXT_INPUT_MODE_DECIMAL:
      return fuchsia::input::virtualkeyboard::TextType::NUMERIC;

    case TEXT_INPUT_MODE_TEL:
      return fuchsia::input::virtualkeyboard::TextType::PHONE;

    case TEXT_INPUT_MODE_TEXT:
    case TEXT_INPUT_MODE_URL:
    case TEXT_INPUT_MODE_EMAIL:
    case TEXT_INPUT_MODE_SEARCH:
      return fuchsia::input::virtualkeyboard::TextType::ALPHANUMERIC;

    // Should be handled in InputMethodFuchsia.
    case TEXT_INPUT_MODE_NONE:
      NOTREACHED();
      return fuchsia::input::virtualkeyboard::TextType::ALPHANUMERIC;

    case TEXT_INPUT_MODE_DEFAULT:
      // Fall-through to using TextInputType.
      break;
  }

  switch (client->GetTextInputType()) {
    case TEXT_INPUT_TYPE_NUMBER:
      return fuchsia::input::virtualkeyboard::TextType::NUMERIC;

    case TEXT_INPUT_TYPE_TELEPHONE:
      return fuchsia::input::virtualkeyboard::TextType::PHONE;

    default:
      return fuchsia::input::virtualkeyboard::TextType::ALPHANUMERIC;
  }
}

void VirtualKeyboardControllerFuchsia::UpdateTextType() {
  // Only send updates if the type has changed.
  auto new_type = GetFocusedTextType();
  DVLOG(1) << "UpdateTextType() called (current: " << requested_type_
           << ", new: " << new_type << ")";
  if (new_type != requested_type_) {
    controller_service_->SetTextType(new_type);
    requested_type_ = new_type;
  }
}

}  // namespace ui
