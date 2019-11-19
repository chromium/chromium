// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_MOCK_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_H_
#define UI_BASE_IME_CHROMEOS_MOCK_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"

namespace chromeos {
namespace input_method {

class COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) MockComponentExtIMEManagerDelegate
    : public ComponentExtensionIMEManagerDelegate {
 public:
  MockComponentExtIMEManagerDelegate();
  ~MockComponentExtIMEManagerDelegate() override;

  std::vector<ComponentExtensionIME> ListIME() override;
  void Load(Profile*,
            const std::string& extension_id,
            const std::string& manifest,
            const base::FilePath& path) override;
  void Unload(Profile*,
              const std::string& extension_id,
              const base::FilePath& path) override;

  int load_call_count() const { return load_call_count_; }
  int unload_call_count() const { return unload_call_count_; }
  const std::string& last_loaded_extension_id() const {
    return last_loaded_extension_id_;
  }
  const std::string& last_unloaded_extension_id() const {
    return last_unloaded_extension_id_;
  }
  const base::FilePath& last_loaded_file_path() const {
    return last_loaded_file_path_;
  }
  const base::FilePath& last_unloaded_file_path() const {
    return last_unloaded_file_path_;
  }
  void set_ime_list(const std::vector<ComponentExtensionIME>& ime_list) {
    ime_list_ = ime_list;
  }

 private:
  int load_call_count_;
  int unload_call_count_;
  std::string last_loaded_extension_id_;
  std::string last_unloaded_extension_id_;
  base::FilePath last_loaded_file_path_;
  base::FilePath last_unloaded_file_path_;

  std::vector<ComponentExtensionIME> ime_list_;

  DISALLOW_COPY_AND_ASSIGN(MockComponentExtIMEManagerDelegate);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_MOCK_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_H_
