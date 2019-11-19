// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/mock_input_method_manager.h"

#include <utility>

namespace chromeos {
namespace input_method {

MockInputMethodManager::State::State() = default;

scoped_refptr<InputMethodManager::State> MockInputMethodManager::State::Clone()
    const {
  return nullptr;
}

void MockInputMethodManager::State::AddInputMethodExtension(
    const std::string& extension_id,
    const InputMethodDescriptors& descriptors,
    ui::IMEEngineHandlerInterface* instance) {}

void MockInputMethodManager::State::RemoveInputMethodExtension(
    const std::string& extension_id) {}

void MockInputMethodManager::State::ChangeInputMethod(
    const std::string& input_method_id,
    bool show_message) {}

void MockInputMethodManager::State::ChangeInputMethodToJpKeyboard() {}

void MockInputMethodManager::State::ChangeInputMethodToJpIme() {}

void MockInputMethodManager::State::ToggleInputMethodForJpIme() {}

bool MockInputMethodManager::State::EnableInputMethod(
    const std::string& new_active_input_method_id) {
  return true;
}

void MockInputMethodManager::State::EnableLoginLayouts(
    const std::string& language_code,
    const std::vector<std::string>& initial_layout) {}

void MockInputMethodManager::State::EnableLockScreenLayouts() {}

void MockInputMethodManager::State::GetInputMethodExtensions(
    InputMethodDescriptors* result) {}

std::unique_ptr<InputMethodDescriptors>
MockInputMethodManager::State::GetActiveInputMethods() const {
  return nullptr;
}

const std::vector<std::string>&
MockInputMethodManager::State::GetActiveInputMethodIds() const {
  return active_input_method_ids;
}

const InputMethodDescriptor*
MockInputMethodManager::State::GetInputMethodFromId(
    const std::string& input_method_id) const {
  return nullptr;
}

size_t MockInputMethodManager::State::GetNumActiveInputMethods() const {
  return active_input_method_ids.size();
}

void MockInputMethodManager::State::SetEnabledExtensionImes(
    std::vector<std::string>* ids) {}

void MockInputMethodManager::State::SetInputMethodLoginDefault() {}

void MockInputMethodManager::State::SetInputMethodLoginDefaultFromVPD(
    const std::string& locale,
    const std::string& layout) {}

void MockInputMethodManager::State::SwitchToNextInputMethod() {}

void MockInputMethodManager::State::SwitchToLastUsedInputMethod() {}

InputMethodDescriptor MockInputMethodManager::State::GetCurrentInputMethod()
    const {
  InputMethodDescriptor descriptor;
  return descriptor;
}

bool MockInputMethodManager::State::ReplaceEnabledInputMethods(
    const std::vector<std::string>& new_active_input_method_ids) {
  return true;
}

bool MockInputMethodManager::State::SetAllowedInputMethods(
    const std::vector<std::string>& new_allowed_input_method_ids,
    bool enable_allowed_input_methods) {
  allowed_input_method_ids_ = new_allowed_input_method_ids;
  return true;
}

const std::vector<std::string>&
MockInputMethodManager::State::GetAllowedInputMethods() {
  return allowed_input_method_ids_;
}

void MockInputMethodManager::State::EnableInputView() {}

void MockInputMethodManager::State::DisableInputView() {}

const GURL& MockInputMethodManager::State::GetInputViewUrl() const {
  return GURL::EmptyGURL();
}

MockInputMethodManager::State::~State() = default;

MockInputMethodManager::MockInputMethodManager()
    : features_enabled_state_(InputMethodManager::FEATURE_ALL) {}

MockInputMethodManager::~MockInputMethodManager() = default;

InputMethodManager::UISessionState MockInputMethodManager::GetUISessionState() {
  return InputMethodManager::STATE_BROWSER_SCREEN;
}

void MockInputMethodManager::AddObserver(
    InputMethodManager::Observer* observer) {}

void MockInputMethodManager::AddCandidateWindowObserver(
    InputMethodManager::CandidateWindowObserver* observer) {}

void MockInputMethodManager::AddImeMenuObserver(
    InputMethodManager::ImeMenuObserver* observer) {}

void MockInputMethodManager::RemoveObserver(
    InputMethodManager::Observer* observer) {}

void MockInputMethodManager::RemoveCandidateWindowObserver(
    InputMethodManager::CandidateWindowObserver* observer) {}

void MockInputMethodManager::RemoveImeMenuObserver(
    InputMethodManager::ImeMenuObserver* observer) {}

std::unique_ptr<InputMethodDescriptors>
MockInputMethodManager::GetSupportedInputMethods() const {
  return nullptr;
}

void MockInputMethodManager::ActivateInputMethodMenuItem(
    const std::string& key) {}

void MockInputMethodManager::ConnectInputEngineManager(
    mojo::PendingReceiver<chromeos::ime::mojom::InputEngineManager> receiver) {}

bool MockInputMethodManager::IsISOLevel5ShiftUsedByCurrentInputMethod() const {
  return false;
}

bool MockInputMethodManager::IsAltGrUsedByCurrentInputMethod() const {
  return false;
}

ImeKeyboard* MockInputMethodManager::GetImeKeyboard() {
  return nullptr;
}

InputMethodUtil* MockInputMethodManager::GetInputMethodUtil() {
  return nullptr;
}

ComponentExtensionIMEManager*
MockInputMethodManager::GetComponentExtensionIMEManager() {
  return nullptr;
}

bool MockInputMethodManager::IsLoginKeyboard(const std::string& layout) const {
  return true;
}

bool MockInputMethodManager::MigrateInputMethods(
    std::vector<std::string>* input_method_ids) {
  return false;
}
scoped_refptr<InputMethodManager::State> MockInputMethodManager::CreateNewState(
    Profile* profile) {
  return nullptr;
}

scoped_refptr<InputMethodManager::State>
MockInputMethodManager::GetActiveIMEState() {
  return nullptr;
}

void MockInputMethodManager::SetState(
    scoped_refptr<InputMethodManager::State> state) {}

void MockInputMethodManager::ImeMenuActivationChanged(bool is_active) {}

void MockInputMethodManager::NotifyImeMenuItemsChanged(
    const std::string& engine_id,
    const std::vector<InputMethodManager::MenuItem>& items) {}

void MockInputMethodManager::MaybeNotifyImeMenuActivationChanged() {}

void MockInputMethodManager::OverrideKeyboardKeyset(mojom::ImeKeyset keyset) {}

void MockInputMethodManager::SetImeMenuFeatureEnabled(ImeMenuFeature feature,
                                                      bool enabled) {
  if (enabled)
    features_enabled_state_ |= feature;
  else
    features_enabled_state_ &= ~feature;
}

bool MockInputMethodManager::GetImeMenuFeatureEnabled(
    ImeMenuFeature feature) const {
  return features_enabled_state_ & feature;
}

void MockInputMethodManager::NotifyObserversImeExtraInputStateChange() {}

ui::InputMethodKeyboardController*
MockInputMethodManager::GetInputMethodKeyboardController() {
  return this;
}

void MockInputMethodManager::NotifyInputMethodExtensionAdded(
    const std::string& extension_id) {}

void MockInputMethodManager::NotifyInputMethodExtensionRemoved(
    const std::string& extension_id) {}

bool MockInputMethodManager::DisplayVirtualKeyboard() {
  return false;
}

void MockInputMethodManager::DismissVirtualKeyboard() {}

void MockInputMethodManager::AddObserver(
    ui::InputMethodKeyboardControllerObserver* observer) {}

void MockInputMethodManager::RemoveObserver(
    ui::InputMethodKeyboardControllerObserver* observer) {}

bool MockInputMethodManager::IsKeyboardVisible() {
  return false;
}

}  // namespace input_method
}  // namespace chromeos
