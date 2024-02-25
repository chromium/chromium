// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/mock_input_method_manager_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "ui/base/ime/ash/input_method_util.h"

namespace ash {
namespace input_method {

MockInputMethodManagerImpl::State::State(MockInputMethodManagerImpl* manager)
    : manager_(manager) {
  enabled_input_method_ids.emplace_back("xkb:us::eng");
}

scoped_refptr<InputMethodManager::State>
MockInputMethodManagerImpl::State::Clone() const {
  NOTIMPLEMENTED();
  return manager_->GetActiveIMEState();
}

InputMethodDescriptors MockInputMethodManagerImpl::State::
    GetEnabledInputMethodsSortedByLocalizedDisplayNames() const {
  // GetEnabledInputMethods() returns a one-element list, so already "sorted".
  return GetEnabledInputMethods();
}

InputMethodDescriptors
MockInputMethodManagerImpl::State::GetEnabledInputMethods() const {
  InputMethodDescriptors result;
  result.push_back(InputMethodUtil::GetFallbackInputMethodDescriptor());
  return result;
}

const InputMethodDescriptor*
MockInputMethodManagerImpl::State::GetInputMethodFromId(
    const std::string& input_method_id) const {
  static const InputMethodDescriptor defaultInputMethod =
      InputMethodUtil::GetFallbackInputMethodDescriptor();
  for (const auto& enabled_input_method_id : enabled_input_method_ids) {
    if (input_method_id == enabled_input_method_id) {
      return &defaultInputMethod;
    }
  }
  return nullptr;
}

InputMethodDescriptor MockInputMethodManagerImpl::State::GetCurrentInputMethod()
    const {
  InputMethodDescriptor descriptor =
      InputMethodUtil::GetFallbackInputMethodDescriptor();
  if (!current_input_method_id.empty()) {
    return InputMethodDescriptor(
        current_input_method_id, descriptor.name(), descriptor.indicator(),
        descriptor.keyboard_layout(), descriptor.language_codes(), true,
        GURL(),  // options page url.
        GURL(),  // input view page url.
        /*handwriting_language=*/std::nullopt);
  }
  return descriptor;
}

MockInputMethodManagerImpl::State::~State() = default;

MockInputMethodManagerImpl::MockInputMethodManagerImpl()
    : state_(new State(this)), util_(new InputMethodUtil(&delegate_)) {}

MockInputMethodManagerImpl::~MockInputMethodManagerImpl() = default;

void MockInputMethodManagerImpl::AddObserver(
    InputMethodManager::Observer* observer) {
  ++add_observer_count_;
}

void MockInputMethodManagerImpl::AddImeMenuObserver(ImeMenuObserver* observer) {
  ++add_menu_observer_count_;
}

void MockInputMethodManagerImpl::RemoveObserver(
    InputMethodManager::Observer* observer) {
  ++remove_observer_count_;
}

void MockInputMethodManagerImpl::RemoveImeMenuObserver(
    ImeMenuObserver* observer) {
  ++remove_menu_observer_count_;
}

bool MockInputMethodManagerImpl::IsISOLevel5ShiftUsedByCurrentInputMethod()
    const {
  return mod3_used_;
}

ImeKeyboard* MockInputMethodManagerImpl::GetImeKeyboard() {
  return &keyboard_;
}

InputMethodUtil* MockInputMethodManagerImpl::GetInputMethodUtil() {
  return util_.get();
}

ComponentExtensionIMEManager*
MockInputMethodManagerImpl::GetComponentExtensionIMEManager() {
  return comp_ime_manager_.get();
}

scoped_refptr<InputMethodManager::State>
MockInputMethodManagerImpl::CreateNewState(Profile* profile) {
  NOTIMPLEMENTED();
  return state_;
}

scoped_refptr<InputMethodManager::State>
MockInputMethodManagerImpl::GetActiveIMEState() {
  return scoped_refptr<InputMethodManager::State>(state_.get());
}

void MockInputMethodManagerImpl::SetState(
    scoped_refptr<InputMethodManager::State> state) {
  state_ = scoped_refptr<MockInputMethodManagerImpl::State>(
      static_cast<MockInputMethodManagerImpl::State*>(state.get()));
}

void MockInputMethodManagerImpl::SetCurrentInputMethodId(
    const std::string& input_method_id) {
  state_->current_input_method_id = input_method_id;
}

void MockInputMethodManagerImpl::SetComponentExtensionIMEManager(
    std::unique_ptr<ComponentExtensionIMEManager> comp_ime_manager) {
  comp_ime_manager_ = std::move(comp_ime_manager);
}

}  // namespace input_method
}  // namespace ash
