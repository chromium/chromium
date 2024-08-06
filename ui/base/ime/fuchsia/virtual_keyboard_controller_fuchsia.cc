// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/fuchsia/virtual_keyboard_controller_fuchsia.h"

#include <fidl/fuchsia.input.virtualkeyboard/cpp/natural_ostream.h>
#include <lib/async/default.h>
#include <lib/fit/function.h>
#include <lib/sys/cpp/component_context.h>

#include <utility>

#include "base/check.h"
#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "ui/base/ime/text_input_client.h"

namespace ui {

VirtualKeyboardControllerFuchsia::VirtualKeyboardControllerFuchsia(
    fuchsia_ui_views::ViewRef view_ref,
    ui::InputMethodBase* input_method)
    : input_method_(input_method) {
  DCHECK(input_method_);

  auto controller_creator_client_end = base::fuchsia_component::Connect<
      fuchsia_input_virtualkeyboard::ControllerCreator>();
  if (!controller_creator_client_end.is_ok()) {
    LOG(ERROR) << base::FidlConnectionErrorMessage(
        controller_creator_client_end);
    return;
  }
  fidl::Client controller_creator(
      std::move(controller_creator_client_end.value()),
      async_get_default_dispatcher());

  auto controller_endpoints =
      fidl::CreateEndpoints<fuchsia_input_virtualkeyboard::Controller>();
  ZX_CHECK(controller_endpoints.is_ok(), controller_endpoints.status_value());
  auto create_result = controller_creator->Create({{
      .view_ref = std::move(view_ref),
      .text_type = requested_type_,
      .controller_request = std::move(controller_endpoints->server),
  }});

  if (create_result.is_error()) {
    ZX_DLOG(ERROR, create_result.error_value().status());
    return;
  }
  controller_client_.Bind(std::move(controller_endpoints->client),
                          async_get_default_dispatcher(),
                          &controller_error_logger_);

  WatchVisibility();
}

VirtualKeyboardControllerFuchsia::~VirtualKeyboardControllerFuchsia() = default;

bool VirtualKeyboardControllerFuchsia::DisplayVirtualKeyboard() {
  DVLOG(1) << "DisplayVirtualKeyboard (visible= " << keyboard_visible_ << ")";

  if (!controller_client_) {
    return false;
  }

  UpdateTextType();

  requested_visible_ = true;
  auto result = controller_client_->RequestShow();
  if (result.is_error()) {
    ZX_DLOG(ERROR, result.error_value().status())
        << "Error calling RequestShow()";
    return false;
  }

  return true;
}

void VirtualKeyboardControllerFuchsia::DismissVirtualKeyboard() {
  DVLOG(1) << "DismissVirtualKeyboard (visible= " << keyboard_visible_ << ")";

  if (!controller_client_) {
    return;
  }

  if (!requested_visible_ && !keyboard_visible_)
    return;

  requested_visible_ = false;
  auto result = controller_client_->RequestHide();
  if (result.is_error()) {
    ZX_DLOG(ERROR, result.error_value().status())
        << "Error calling RequestHide()";
  }
}

void VirtualKeyboardControllerFuchsia::AddObserver(
    VirtualKeyboardControllerObserver* observer) {}

void VirtualKeyboardControllerFuchsia::RemoveObserver(
    VirtualKeyboardControllerObserver* observer) {}

bool VirtualKeyboardControllerFuchsia::IsKeyboardVisible() {
  return keyboard_visible_;
}

void VirtualKeyboardControllerFuchsia::WatchVisibility() {
  if (!controller_client_) {
    return;
  }

  controller_client_->WatchVisibility().Then(fit::bind_member(
      this, &VirtualKeyboardControllerFuchsia::OnVisibilityChange));
}

void VirtualKeyboardControllerFuchsia::OnVisibilityChange(
    const fidl::Result<
        fuchsia_input_virtualkeyboard::Controller::WatchVisibility>& result) {
  if (result.is_error()) {
    ZX_DLOG(ERROR, result.error_value().status()) << __func__;
    return;
  }
  DVLOG(1) << "OnVisibilityChange " << result->is_visible();
  keyboard_visible_ = result->is_visible();
  WatchVisibility();
}

// Returns the FIDL enum representation of the current InputMode.
fuchsia_input_virtualkeyboard::TextType
VirtualKeyboardControllerFuchsia::GetFocusedTextType() const {
  TextInputClient* client = input_method_->GetTextInputClient();
  // This function should only be called when there's focus, so there should
  // always be a TextInputClient.
  DCHECK(client);

  switch (client->GetTextInputMode()) {
    case TEXT_INPUT_MODE_NUMERIC:
    case TEXT_INPUT_MODE_DECIMAL:
      return fuchsia_input_virtualkeyboard::TextType::kNumeric;

    case TEXT_INPUT_MODE_TEL:
      return fuchsia_input_virtualkeyboard::TextType::kPhone;

    case TEXT_INPUT_MODE_TEXT:
    case TEXT_INPUT_MODE_URL:
    case TEXT_INPUT_MODE_EMAIL:
    case TEXT_INPUT_MODE_SEARCH:
      return fuchsia_input_virtualkeyboard::TextType::kAlphanumeric;

    // Should be handled in InputMethodFuchsia.
    case TEXT_INPUT_MODE_NONE:
      NOTREACHED();

    case TEXT_INPUT_MODE_DEFAULT:
      // Fall-through to using TextInputType.
      break;
  }

  switch (client->GetTextInputType()) {
    case TEXT_INPUT_TYPE_NUMBER:
      return fuchsia_input_virtualkeyboard::TextType::kNumeric;

    case TEXT_INPUT_TYPE_TELEPHONE:
      return fuchsia_input_virtualkeyboard::TextType::kPhone;

    default:
      return fuchsia_input_virtualkeyboard::TextType::kAlphanumeric;
  }
}

void VirtualKeyboardControllerFuchsia::UpdateTextType() {
  // Only send updates if the type has changed.
  auto new_type = GetFocusedTextType();
  DVLOG(1) << "UpdateTextType() called (current: " << requested_type_
           << ", new: " << new_type << ")";
  if (new_type != requested_type_) {
    auto result = controller_client_->SetTextType(new_type);
    if (result.is_error()) {
      ZX_DLOG(ERROR, result.error_value().status())
          << "Error calling SetTextType()";
    }
    requested_type_ = new_type;
  }
}

}  // namespace ui
