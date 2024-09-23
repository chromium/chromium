// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/mock_input_method_manager.h"

#include <utility>

#include "ui/base/ime/ash/input_method_util.h"

namespace ash {
namespace input_method {

MockInputMethodManager::State::State() = default;

scoped_refptr<InputMethodManager::State> MockInputMethodManager::State::Clone()
    const {
  return nullptr;
}

void MockInputMethodManager::State::AddInputMethodExtension(
    const std::string& extension_id,
    const InputMethodDescriptors& descriptors,
    TextInputMethod* instance) {}

void MockInputMethodManager::State::RemoveInputMethodExtension(
    const std::string& extension_id) {}

void MockInputMethodManager::State::ChangeInputMethod(
    const std::string& input_method_id,
    bool show_message) {}

void MockInputMethodManager::State::ChangeInputMethodToJpKeyboard() {}

void MockInputMethodManager::State::ChangeInputMethodToJpIme() {}

void MockInputMethodManager::State::ToggleInputMethodForJpIme() {}

bool MockInputMethodManager::State::EnableInputMethod(
    const std::string& new_enabled_input_method_id) {
  return true;
}

void MockInputMethodManager::State::EnableLoginLayouts(
    const std::string& language_code,
    const std::vector<std::string>& initial_layout) {}

void MockInputMethodManager::State::DisableNonLockScreenLayouts() {}

void MockInputMethodManager::State::GetInputMethodExtensions(
    InputMethodDescriptors* result) {}

InputMethodDescriptors MockInputMethodManager::State::
    GetEnabledInputMethodsSortedByLocalizedDisplayNames() const {
  return {};
}

InputMethodDescriptors MockInputMethodManager::State::GetEnabledInputMethods()
    const {
  return {};
}

const std::vector<std::string>&
MockInputMethodManager::State::GetEnabledInputMethodIds() const {
  return enabled_input_method_ids;
}

const InputMethodDescriptor*
MockInputMethodManager::State::GetInputMethodFromId(
    const std::string& input_method_id) const {
  return nullptr;
}

size_t MockInputMethodManager::State::GetNumEnabledInputMethods() const {
  return enabled_input_method_ids.size();
}

void MockInputMethodManager::State::SetEnabledExtensionImes(
    base::span<const std::string> ids) {}

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
    const std::vector<std::string>& new_enabled_input_method_ids) {
  return true;
}

bool MockInputMethodManager::State::SetAllowedInputMethods(
    const std::vector<std::string>& new_allowed_input_method_ids) {
  allowed_input_method_ids_ = new_allowed_input_method_ids;
  return true;
}

const std::vector<std::string>&
MockInputMethodManager::State::GetAllowedInputMethodIds() const {
  return allowed_input_method_ids_;
}

std::string MockInputMethodManager::State::GetAllowedFallBackKeyboardLayout()
    const {
  return "input_method_id";
}

void MockInputMethodManager::State::EnableInputView() {}

void MockInputMethodManager::State::DisableInputView() {}

const GURL& MockInputMethodManager::State::GetInputViewUrl() const {
  return GURL::EmptyGURL();
}

InputMethodManager::UIStyle MockInputMethodManager::State::GetUIStyle() const {
  return ui_style_;
}

void MockInputMethodManager::State::SetUIStyle(
    InputMethodManager::UIStyle ui_style) {
  ui_style_ = ui_style;
}

MockInputMethodManager::State::~State() = default;

MockInputMethodManager::MockInputMethodManager()
    : state_(new State()),
      features_enabled_state_(InputMethodManager::FEATURE_ALL) {}

MockInputMethodManager::~MockInputMethodManager() = default;

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

void MockInputMethodManager::ActivateInputMethodMenuItem(
    const std::string& key) {}

void MockInputMethodManager::ConnectInputEngineManager(
    mojo::PendingReceiver<ime::mojom::InputEngineManager> receiver) {}

void MockInputMethodManager::BindInputMethodUserDataService(
    mojo::PendingReceiver<ime::mojom::InputMethodUserDataService> receiver) {}

bool MockInputMethodManager::IsISOLevel5ShiftUsedByCurrentInputMethod() const {
  return false;
}

bool MockInputMethodManager::IsAltGrUsedByCurrentInputMethod() const {
  return false;
}

bool MockInputMethodManager::ArePositionalShortcutsUsedByCurrentInputMethod()
    const {
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

std::string MockInputMethodManager::GetMigratedInputMethodID(
    const std::string& input_method_id) {
  return "";
}

bool MockInputMethodManager::GetMigratedInputMethodIDs(
    std::vector<std::string>* input_method_ids) {
  return false;
}
scoped_refptr<InputMethodManager::State> MockInputMethodManager::CreateNewState(
    Profile* profile) {
  return nullptr;
}

scoped_refptr<InputMethodManager::State>
MockInputMethodManager::GetActiveIMEState() {
  return state_;
}

void MockInputMethodManager::SetState(
    scoped_refptr<InputMethodManager::State> state) {}

void MockInputMethodManager::ImeMenuActivationChanged(bool is_active) {}

void MockInputMethodManager::NotifyImeMenuItemsChanged(
    const std::string& engine_id,
    const std::vector<InputMethodManager::MenuItem>& items) {}

void MockInputMethodManager::MaybeNotifyImeMenuActivationChanged() {}

void MockInputMethodManager::OverrideKeyboardKeyset(ImeKeyset keyset) {}

void MockInputMethodManager::SetImeMenuFeatureEnabled(ImeMenuFeature feature,
                                                      bool enabled) {
  if (enabled) {
    features_enabled_state_ |= feature;
  } else {
    features_enabled_state_ &= ~feature;
  }
}

bool MockInputMethodManager::GetImeMenuFeatureEnabled(
    ImeMenuFeature feature) const {
  return features_enabled_state_ & feature;
}

void MockInputMethodManager::NotifyObserversImeExtraInputStateChange() {}

void MockInputMethodManager::NotifyInputMethodExtensionAdded(
    const std::string& extension_id) {}

void MockInputMethodManager::NotifyInputMethodExtensionRemoved(
    const std::string& extension_id) {}

}  // namespace input_method
}  // namespace ash
