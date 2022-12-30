// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_MOCK_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_H_
#define UI_BASE_IME_ASH_MOCK_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_H_

#include <set>

#include "base/component_export.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/component_extension_ime_manager_delegate.h"

namespace ash {
namespace input_method {

class COMPONENT_EXPORT(UI_BASE_IME_ASH) MockComponentExtensionIMEManagerDelegate
    : public ComponentExtensionIMEManagerDelegate {
 public:
  MockComponentExtensionIMEManagerDelegate();
  ~MockComponentExtensionIMEManagerDelegate() override;

  MockComponentExtensionIMEManagerDelegate(
      const MockComponentExtensionIMEManagerDelegate&) = delete;
  MockComponentExtensionIMEManagerDelegate& operator=(
      const MockComponentExtensionIMEManagerDelegate&) = delete;

  std::vector<ComponentExtensionIME> ListIME() override;
  void Load(Profile*,
            const std::string& extension_id,
            const std::string& manifest,
            const base::FilePath& path) override;
  bool IsInLoginLayoutAllowlist(const std::string& layout) override;

  void set_ime_list(const std::vector<ComponentExtensionIME>& ime_list) {
    ime_list_ = ime_list;
  }
  void set_login_layout_set(const std::set<std::string>& login_layout_set) {
    login_layout_set_ = login_layout_set;
  }
  int load_call_count() const { return load_call_count_; }
  const std::string& last_loaded_extension_id() const {
    return last_loaded_extension_id_;
  }

 private:
  std::set<std::string> login_layout_set_;
  std::vector<ComponentExtensionIME> ime_list_;
  std::string last_loaded_extension_id_;
  int load_call_count_{0};
};

}  // namespace input_method
}  // namespace ash

#endif  // UI_BASE_IME_ASH_MOCK_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_H_
