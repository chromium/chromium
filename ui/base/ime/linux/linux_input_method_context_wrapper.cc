// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/linux/linux_input_method_context_wrapper.h"

namespace ui {

LinuxInputMethodContextWrapper::LinuxInputMethodContextWrapper(
    std::unique_ptr<LinuxInputMethodContext> context,
    std::unique_ptr<LinuxInputMethodContext> simple_context)
    : context_(std::move(context)),
      simple_context_(std::move(simple_context)) {}

LinuxInputMethodContextWrapper::~LinuxInputMethodContextWrapper() = default;

bool LinuxInputMethodContextWrapper::DispatchKeyEvent(
    const ui::KeyEvent& key_event) {
  return GetBaseContext()->DispatchKeyEvent(key_event);
}

bool LinuxInputMethodContextWrapper::IsPeekKeyEvent(
    const ui::KeyEvent& key_event) {
  return GetBaseContext()->IsPeekKeyEvent(key_event);
}

void LinuxInputMethodContextWrapper::SetCursorLocation(const gfx::Rect& rect) {
  // Always use |context_|. This behavior is carried for compatibility.
  context_->SetCursorLocation(rect);
}

void LinuxInputMethodContextWrapper::SetSurroundingText(
    const std::u16string& text,
    const gfx::Range& selection_range) {
  // Always use |context_|. This behavior is carried for compatibility.
  context_->SetSurroundingText(text, selection_range);
}

void LinuxInputMethodContextWrapper::SetContentType(TextInputType type,
                                                    TextInputMode mode,
                                                    uint32_t flags,
                                                    bool should_do_learning) {
  GetBaseContext()->SetContentType(type, mode, flags, should_do_learning);
}

void LinuxInputMethodContextWrapper::Reset() {
  context_->Reset();
  simple_context_->Reset();
}

void LinuxInputMethodContextWrapper::UpdateFocus(bool has_client,
                                                 TextInputType old_type,
                                                 TextInputType new_type) {
  context_->UpdateFocus(has_client, old_type, new_type);
  simple_context_->UpdateFocus(has_client, old_type, new_type);
  current_type_ = new_type;
}

VirtualKeyboardController*
LinuxInputMethodContextWrapper::GetVirtualKeyboardController() {
  // Always use |context_|. THis behavior is carried for compatibility.
  return context_->GetVirtualKeyboardController();
}

LinuxInputMethodContext* LinuxInputMethodContextWrapper::GetBaseContext() {
  switch (current_type_) {
    case TEXT_INPUT_TYPE_NONE:
    case TEXT_INPUT_TYPE_PASSWORD:
      return simple_context_.get();
    default:
      return context_.get();
  }
}

}  // namespace ui
