// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/dummy_input_method.h"
#include "build/build_config.h"
#include "ui/events/event.h"

namespace ui {

DummyInputMethod::DummyInputMethod() {
}

DummyInputMethod::~DummyInputMethod() {
}

void DummyInputMethod::SetDelegate(internal::InputMethodDelegate* delegate) {
}

void DummyInputMethod::OnFocus() {
}

void DummyInputMethod::OnBlur() {
}

#if defined(OS_WIN)
bool DummyInputMethod::OnUntranslatedIMEMessage(const MSG event,
                                                NativeEventResult* result) {
  return false;
}
#endif

void DummyInputMethod::SetFocusedTextInputClient(TextInputClient* client) {
}

void DummyInputMethod::DetachTextInputClient(TextInputClient* client) {
}

TextInputClient* DummyInputMethod::GetTextInputClient() const {
  return NULL;
}

ui::EventDispatchDetails DummyInputMethod::DispatchKeyEvent(
    ui::KeyEvent* event) {
  return ui::EventDispatchDetails();
}

void DummyInputMethod::OnTextInputTypeChanged(const TextInputClient* client) {
}

void DummyInputMethod::OnCaretBoundsChanged(const TextInputClient* client) {
}

void DummyInputMethod::CancelComposition(const TextInputClient* client) {
}

void DummyInputMethod::OnInputLocaleChanged() {
}

bool DummyInputMethod::IsInputLocaleCJK() const {
  return false;
}

TextInputType DummyInputMethod::GetTextInputType() const {
  return TEXT_INPUT_TYPE_NONE;
}

TextInputMode DummyInputMethod::GetTextInputMode() const {
  return TEXT_INPUT_MODE_DEFAULT;
}

int DummyInputMethod::GetTextInputFlags() const {
  return 0;
}

bool DummyInputMethod::CanComposeInline() const {
  return true;
}

bool DummyInputMethod::IsCandidatePopupOpen() const {
  return false;
}

bool DummyInputMethod::GetClientShouldDoLearning() {
  return false;
}

void DummyInputMethod::ShowVirtualKeyboardIfEnabled() {}

void DummyInputMethod::AddObserver(InputMethodObserver* observer) {
}

void DummyInputMethod::RemoveObserver(InputMethodObserver* observer) {
}

VirtualKeyboardController* DummyInputMethod::GetVirtualKeyboardController() {
  return nullptr;
}

}  // namespace ui
