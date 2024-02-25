// Copyright 2013 The Chromium Authors
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

void DummyInputMethod::SetImeKeyEventDispatcher(
    ImeKeyEventDispatcher* ime_key_event_dispatcher) {}

void DummyInputMethod::OnFocus() {
}

void DummyInputMethod::OnBlur() {
}

#if BUILDFLAG(IS_WIN)
bool DummyInputMethod::OnUntranslatedIMEMessage(const CHROME_MSG event,
                                                NativeEventResult* result) {
  return false;
}

void DummyInputMethod::OnInputLocaleChanged() {}

bool DummyInputMethod::IsInputLocaleCJK() const {
  return false;
}

void DummyInputMethod::OnUrlChanged() {}
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

void DummyInputMethod::OnTextInputTypeChanged(TextInputClient* client) {}

void DummyInputMethod::OnCaretBoundsChanged(const TextInputClient* client) {
}

void DummyInputMethod::CancelComposition(const TextInputClient* client) {
}

TextInputType DummyInputMethod::GetTextInputType() const {
  return TEXT_INPUT_TYPE_NONE;
}

bool DummyInputMethod::IsCandidatePopupOpen() const {
  return false;
}

void DummyInputMethod::SetVirtualKeyboardVisibilityIfEnabled(bool should_show) {
}

void DummyInputMethod::AddObserver(InputMethodObserver* observer) {
}

void DummyInputMethod::RemoveObserver(InputMethodObserver* observer) {
}

VirtualKeyboardController* DummyInputMethod::GetVirtualKeyboardController() {
  return nullptr;
}

void DummyInputMethod::SetVirtualKeyboardControllerForTesting(
    std::unique_ptr<VirtualKeyboardController> controller) {}

}  // namespace ui
