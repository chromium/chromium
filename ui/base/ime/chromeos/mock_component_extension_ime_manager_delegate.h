// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_MOCK_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_H_
#define UI_BASE_IME_CHROMEOS_MOCK_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager_delegate.h"

namespace chromeos {
namespace input_method {

class COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
    MockComponentExtensionIMEManagerDelegate
    : public ComponentExtensionIMEManagerDelegate {
 public:
  MockComponentExtensionIMEManagerDelegate();
  ~MockComponentExtensionIMEManagerDelegate() override;

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

 private:
  std::set<std::string> login_layout_set_;
  std::vector<ComponentExtensionIME> ime_list_;

  DISALLOW_COPY_AND_ASSIGN(MockComponentExtensionIMEManagerDelegate);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_MOCK_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_H_
