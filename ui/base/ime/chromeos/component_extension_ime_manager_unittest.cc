// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/component_extension_ime_manager.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/mock_component_extension_ime_manager_delegate.h"

namespace chromeos {
namespace input_method {

namespace {

class ComponentExtensionIMEManagerTest : public testing::Test {
 public:
  ComponentExtensionIMEManagerTest() : mock_delegate_(NULL) {}

  virtual void SetUp() {
    ime_list_.clear();

    ComponentExtensionIME ext1;
    ext1.id = "ext1_id_xxxxxxxxxxxxxxxxxxxxxxxx";
    ext1.description = "ext1_description";
    ext1.options_page_url =
        GURL("chrome-extension://" + ext1.id + "/options.html");
    ext1.path = base::FilePath("ext1_file_path");

    ComponentExtensionEngine ext1_engine1;
    ext1_engine1.engine_id = "zh-t-i0-pinyin";
    ext1_engine1.display_name = "ext1_engine_1_display_name";
    ext1_engine1.language_codes.push_back("zh-CN");
    ext1_engine1.layouts.push_back("us");
    ext1.engines.push_back(ext1_engine1);

    ComponentExtensionEngine ext1_engine2;
    ext1_engine2.engine_id = "mozc_us";
    ext1_engine2.display_name = "ext1_engine2_display_name";
    ext1_engine2.language_codes.push_back("jp");
    ext1_engine2.layouts.push_back("us");
    ext1.engines.push_back(ext1_engine2);

    ComponentExtensionEngine ext1_engine3;
    ext1_engine3.engine_id = "xkb:ru::rus";
    ext1_engine3.display_name = "ext1_engine3_display_name";
    ext1_engine3.language_codes.push_back("ru");
    ext1_engine3.layouts.push_back("ru");
    ext1.engines.push_back(ext1_engine3);

    ime_list_.push_back(ext1);

    ComponentExtensionIME ext2;
    ext2.id = "ext2_id_xxxxxxxxxxxxxxxxxxxxxxxx";
    ext2.description = "ext2_description";
    ext2.path = base::FilePath("ext2_file_path");

    ComponentExtensionEngine ext2_engine1;
    ext2_engine1.engine_id = "vkd_ru_phone_aatseel";
    ext2_engine1.display_name = "ext2_engine_1_display_name";
    ext2_engine1.language_codes.push_back("ru");
    ext2_engine1.layouts.push_back("us");
    ext2.engines.push_back(ext2_engine1);

    ComponentExtensionEngine ext2_engine2;
    ext2_engine2.engine_id = "vkd_vi_telex";
    ext2_engine2.display_name = "ext2_engine2_display_name";
    ext2_engine2.language_codes.push_back("hi");
    ext2_engine2.layouts.push_back("us");
    ext2.engines.push_back(ext2_engine2);

    ComponentExtensionEngine ext2_engine3;
    ext2_engine3.engine_id = "xkb:us::eng";
    ext2_engine3.display_name = "ext2_engine3_display_name";
    ext2_engine3.language_codes.push_back("us");
    ext2_engine3.layouts.push_back("us");
    ext2.engines.push_back(ext2_engine3);

    ime_list_.push_back(ext2);

    ComponentExtensionIME ext3;
    ext3.id = "ext3_id_xxxxxxxxxxxxxxxxxxxxxxxx";
    ext3.description = "ext3_description";
    ext3.options_page_url =
    GURL("chrome-extension://" + ext3.id + "/options.html");
    ext3.path = base::FilePath("ext3_file_path");

    ComponentExtensionEngine ext3_engine1;
    ext3_engine1.engine_id = "ext3_engine1_engine_id";
    ext3_engine1.display_name = "ext3_engine_1_display_name";
    ext3_engine1.language_codes.push_back("hi");
    ext3_engine1.layouts.push_back("us");
    ext3.engines.push_back(ext3_engine1);

    ComponentExtensionEngine ext3_engine2;
    ext3_engine2.engine_id = "ext3_engine2_engine_id";
    ext3_engine2.display_name = "ext3_engine2_display_name";
    ext3_engine2.language_codes.push_back("en");
    ext3_engine2.layouts.push_back("us");
    ext3.engines.push_back(ext3_engine2);

    ComponentExtensionEngine ext3_engine3;
    ext3_engine3.engine_id = "ext3_engine3_engine_id";
    ext3_engine3.display_name = "ext3_engine3_display_name";
    ext3_engine3.language_codes.push_back("en");
    ext3_engine3.layouts.push_back("us");
    ext3.engines.push_back(ext3_engine3);

    ime_list_.push_back(ext3);

    mock_delegate_ = new MockComponentExtIMEManagerDelegate();
    mock_delegate_->set_ime_list(ime_list_);
    component_ext_mgr_ = std::make_unique<ComponentExtensionIMEManager>();
    component_ext_mgr_->Initialize(base::WrapUnique(mock_delegate_));
  }

  virtual void TearDown() {
  }

 protected:
  MockComponentExtIMEManagerDelegate* mock_delegate_;
  std::unique_ptr<ComponentExtensionIMEManager> component_ext_mgr_;
  std::vector<ComponentExtensionIME> ime_list_;

 private:

  DISALLOW_COPY_AND_ASSIGN(ComponentExtensionIMEManagerTest);
};

TEST_F(ComponentExtensionIMEManagerTest, LoadComponentExtensionIMETest) {
  for (size_t i = 0; i < ime_list_.size(); ++i) {
    for (size_t j = 0; j < ime_list_[i].engines.size(); ++j) {
      const std::string input_method_id =
          extension_ime_util::GetComponentInputMethodID(
              ime_list_[i].id,
              ime_list_[i].engines[j].engine_id);
      component_ext_mgr_->LoadComponentExtensionIME(NULL /* profile */,
                                                    input_method_id);
      EXPECT_EQ(ime_list_[i].id, mock_delegate_->last_loaded_extension_id());
    }
  }
  EXPECT_EQ(9, mock_delegate_->load_call_count());
}

TEST_F(ComponentExtensionIMEManagerTest, UnloadComponentExtensionIMETest) {
  for (size_t i = 0; i < ime_list_.size(); ++i) {
    for (size_t j = 0; j < ime_list_[i].engines.size(); ++j) {
      const std::string input_method_id =
          extension_ime_util::GetComponentInputMethodID(
              ime_list_[i].id,
              ime_list_[i].engines[j].engine_id);
      component_ext_mgr_->UnloadComponentExtensionIME(NULL /* profile */,
                                                      input_method_id);
      EXPECT_EQ(ime_list_[i].id, mock_delegate_->last_unloaded_extension_id());
    }
  }
  EXPECT_EQ(9, mock_delegate_->unload_call_count());
}

TEST_F(ComponentExtensionIMEManagerTest, IsWhitelistedTest) {
  EXPECT_TRUE(component_ext_mgr_->IsWhitelisted(
      extension_ime_util::GetComponentInputMethodID(
          ime_list_[0].id,
          ime_list_[0].engines[0].engine_id)));
  EXPECT_FALSE(component_ext_mgr_->IsWhitelisted(
      extension_ime_util::GetInputMethodID(
          ime_list_[0].id,
          ime_list_[0].engines[0].engine_id)));
  EXPECT_FALSE(component_ext_mgr_->IsWhitelisted("mozc"));
  EXPECT_FALSE(component_ext_mgr_->IsWhitelisted(
      extension_ime_util::GetInputMethodID("AAAA", "012345")));
  EXPECT_FALSE(component_ext_mgr_->IsWhitelisted(
      extension_ime_util::GetComponentInputMethodID(
          "AAAA", "012345")));
}

TEST_F(ComponentExtensionIMEManagerTest, IsWhitelistedExtensionTest) {
  EXPECT_TRUE(component_ext_mgr_->IsWhitelistedExtension(ime_list_[0].id));
  EXPECT_TRUE(component_ext_mgr_->IsWhitelistedExtension(ime_list_[1].id));
  EXPECT_FALSE(component_ext_mgr_->IsWhitelistedExtension("dummy"));
  EXPECT_FALSE(component_ext_mgr_->IsWhitelistedExtension(""));
}

TEST_F(ComponentExtensionIMEManagerTest, GetAllIMEAsInputMethodDescriptor) {
  input_method::InputMethodDescriptors descriptors =
      component_ext_mgr_->GetAllIMEAsInputMethodDescriptor();
  size_t total_ime_size = 0;
  for (size_t i = 0; i < ime_list_.size(); ++i) {
    total_ime_size += ime_list_[i].engines.size();
  }
  EXPECT_EQ(total_ime_size, descriptors.size());

  // Verify order
  for (size_t i = 0; i < descriptors.size(); ++i) {
    const input_method::InputMethodDescriptor& d = descriptors[i];
    if (i < 2) {
      EXPECT_TRUE(d.id().find("xkb:") != std::string::npos);
    } else if (i >= 2 && i < 4) {
      EXPECT_TRUE(d.id().find("vkd_") != std::string::npos);
    }
  }
}

}  // namespace

}  // namespace input_method
}  // namespace chromeos
