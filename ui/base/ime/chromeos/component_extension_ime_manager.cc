// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/component_extension_ime_manager.h"

#include <stddef.h>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/ime/input_methods.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"

namespace chromeos {

namespace {

// Gets the input method category according to the given input method id.
// This is used for sorting a list of input methods.
int GetInputMethodCategory(const std::string& id) {
  const std::string engine_id =
      chromeos::extension_ime_util::GetComponentIDByInputMethodID(id);
  if (base::StartsWith(engine_id, "xkb:", base::CompareCase::SENSITIVE))
    return 0;
  if (base::StartsWith(engine_id, "vkd_", base::CompareCase::SENSITIVE))
    return 1;
  if (engine_id.find("-t-i0-") != std::string::npos &&
      !base::StartsWith(engine_id, "zh-", base::CompareCase::SENSITIVE)) {
    return 2;
  }
  return 3;
}

bool InputMethodCompare(const input_method::InputMethodDescriptor& im1,
                        const input_method::InputMethodDescriptor& im2) {
  return GetInputMethodCategory(im1.id()) < GetInputMethodCategory(im2.id());
}

} // namespace

ComponentExtensionEngine::ComponentExtensionEngine() = default;

ComponentExtensionEngine::ComponentExtensionEngine(
    const ComponentExtensionEngine& other) = default;

ComponentExtensionEngine::~ComponentExtensionEngine() = default;

ComponentExtensionIME::ComponentExtensionIME() = default;

ComponentExtensionIME::ComponentExtensionIME(
    const ComponentExtensionIME& other) = default;

ComponentExtensionIME::~ComponentExtensionIME() = default;

ComponentExtensionIMEManagerDelegate::ComponentExtensionIMEManagerDelegate() =
    default;

ComponentExtensionIMEManagerDelegate::~ComponentExtensionIMEManagerDelegate() =
    default;

ComponentExtensionIMEManager::ComponentExtensionIMEManager() {
  for (const auto& input_method : input_method::kInputMethods) {
    if (input_method.is_login_keyboard)
      login_layout_set_.insert(input_method.xkb_layout_id);
  }
}

ComponentExtensionIMEManager::~ComponentExtensionIMEManager() = default;

void ComponentExtensionIMEManager::Initialize(
    std::unique_ptr<ComponentExtensionIMEManagerDelegate> delegate) {
  delegate_ = std::move(delegate);
  std::vector<ComponentExtensionIME> ext_list = delegate_->ListIME();
  for (const auto& ext : ext_list) {
    bool extension_exists = IsWhitelistedExtension(ext.id);
    if (!extension_exists)
      component_extension_imes_[ext.id] = ext;
    for (const auto& ime : ext.engines) {
      const std::string input_method_id =
          extension_ime_util::GetComponentInputMethodID(ext.id, ime.engine_id);
      if (extension_exists && !IsWhitelisted(input_method_id))
        component_extension_imes_[ext.id].engines.push_back(ime);
      input_method_id_set_.insert(input_method_id);
    }
  }
}

bool ComponentExtensionIMEManager::LoadComponentExtensionIME(
    Profile* profile,
    const std::string& input_method_id) {
  ComponentExtensionIME ime;
  if (FindEngineEntry(input_method_id, &ime)) {
    delegate_->Load(profile, ime.id, ime.manifest, ime.path);
    return true;
  }
  return false;
}

bool ComponentExtensionIMEManager::UnloadComponentExtensionIME(
    Profile* profile,
    const std::string& input_method_id) {
  ComponentExtensionIME ime;
  if (!FindEngineEntry(input_method_id, &ime))
    return false;
  delegate_->Unload(profile, ime.id, ime.path);
  return true;
}

bool ComponentExtensionIMEManager::IsWhitelisted(
    const std::string& input_method_id) {
  return input_method_id_set_.find(input_method_id) !=
         input_method_id_set_.end();
}

bool ComponentExtensionIMEManager::IsWhitelistedExtension(
    const std::string& extension_id) {
  return component_extension_imes_.find(extension_id) !=
         component_extension_imes_.end();
}

input_method::InputMethodDescriptors
    ComponentExtensionIMEManager::GetAllIMEAsInputMethodDescriptor() {
  input_method::InputMethodDescriptors result;
  for (std::map<std::string, ComponentExtensionIME>::const_iterator it =
          component_extension_imes_.begin();
       it != component_extension_imes_.end(); ++it) {
    const ComponentExtensionIME& ext = it->second;
    for (size_t j = 0; j < ext.engines.size(); ++j) {
      const ComponentExtensionEngine& ime = ext.engines[j];
      const std::string input_method_id =
          extension_ime_util::GetComponentInputMethodID(
              ext.id, ime.engine_id);
      const std::vector<std::string>& layouts = ime.layouts;
      result.push_back(
          input_method::InputMethodDescriptor(
              input_method_id,
              ime.display_name,
              ime.indicator,
              layouts,
              ime.language_codes,
              // Enables extension based xkb keyboards on login screen.
              extension_ime_util::IsKeyboardLayoutExtension(
                  input_method_id) && IsInLoginLayoutWhitelist(layouts),
              ime.options_page_url,
              ime.input_view_url));
    }
  }
  std::stable_sort(result.begin(), result.end(), InputMethodCompare);
  return result;
}

input_method::InputMethodDescriptors
ComponentExtensionIMEManager::GetXkbIMEAsInputMethodDescriptor() {
  input_method::InputMethodDescriptors result;
  const input_method::InputMethodDescriptors& descriptors =
      GetAllIMEAsInputMethodDescriptor();
  for (const auto& descriptor : descriptors) {
    if (extension_ime_util::IsKeyboardLayoutExtension(descriptor.id()))
      result.push_back(descriptor);
  }
  return result;
}

bool ComponentExtensionIMEManager::FindEngineEntry(
    const std::string& input_method_id,
    ComponentExtensionIME* out_extension) {
  if (!IsWhitelisted(input_method_id))
    return false;

  std::string extension_id =
      extension_ime_util::GetExtensionIDFromInputMethodID(input_method_id);
  auto it = component_extension_imes_.find(extension_id);
  if (it == component_extension_imes_.end())
    return false;

  if (out_extension)
    *out_extension = it->second;
  return true;
}

bool ComponentExtensionIMEManager::IsInLoginLayoutWhitelist(
    const std::vector<std::string>& layouts) {
  for (const auto& layout : layouts) {
    if (login_layout_set_.find(layout) != login_layout_set_.end())
      return true;
  }
  return false;
}

}  // namespace chromeos
