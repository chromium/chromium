// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/content_protection_key_manager.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_features.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/manager/test/test_native_display_delegate.h"
#include "ui/display/types/display_constants.h"

namespace display::test {

namespace {
constexpr int64_t kDisplayId = 123;
const DisplayMode kDisplayMode({1366, 768}, false, 60.0f);
const std::string kFakeKey285 =
    "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuv"
    "wxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqr"
    "stuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmn"
    "opqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxy";

}  // namespace

class ContentProtectionKeyManagerTest : public testing::Test {
 protected:
  void SetProvisionedKeyRequest(bool use_valid_key) {
    if (use_valid_key) {
      key_manager_.set_provisioned_key_request(base::BindRepeating(
          [](base::OnceCallback<void(const std::string&)> cb) {
            std::move(cb).Run(kFakeKey285);
          }));
    } else {
      key_manager_.set_provisioned_key_request(base::BindRepeating(
          [](base::OnceCallback<void(const std::string&)> cb) {
            std::move(cb).Run("");
          }));
    }
  }

  void SetKeyIfRequiredForDisplay(auto on_key_set) {
    key_manager_.SetKeyIfRequired(
        std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>{
            display_.get()},
        kDisplayId, std::move(on_key_set));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  ContentProtectionKeyManager key_manager_;
  std::unique_ptr<DisplaySnapshot> display_;

  ActionLogger log_;
  TestNativeDisplayDelegate native_display_delegate_{&log_};

 private:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kRequireHdcpKeyProvisioning);

    key_manager_.set_native_display_delegate(&native_display_delegate_);

    display_ = FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayId)
                   .SetType(DISPLAY_CONNECTION_TYPE_DISPLAYPORT)
                   .SetHasContentProtectionKey(true)
                   .SetCurrentMode(kDisplayMode.Clone())
                   .Build();
  }
};

TEST_F(ContentProtectionKeyManagerTest, TestIfKeySetWhenServerKeyIsValid) {
  SetProvisionedKeyRequest(true);

  auto on_key_set = base::BindOnce([](bool result) { EXPECT_TRUE(result); });

  SetKeyIfRequiredForDisplay(std::move(on_key_set));

  EXPECT_EQ(GetSetHdcpKeyPropAction(kDisplayId, true),
            log_.GetActionsAndClear());
}

TEST_F(ContentProtectionKeyManagerTest, TestIfKeyNotSetWhenServerKeyIsEmpty) {
  SetProvisionedKeyRequest(false);

  auto on_key_set = base::BindOnce([](bool result) { EXPECT_FALSE(result); });

  SetKeyIfRequiredForDisplay(std::move(on_key_set));
}

TEST_F(ContentProtectionKeyManagerTest, TestIfKeyNotSetIfFeatureIsDisabled) {
  scoped_feature_list_.Reset();

  auto on_key_set = base::BindOnce([](bool result) { EXPECT_FALSE(result); });

  SetKeyIfRequiredForDisplay(std::move(on_key_set));
}

TEST_F(ContentProtectionKeyManagerTest, TestIfKeyNotSetWhenDisplayIdMismatch) {
  auto on_key_set = base::BindOnce([](bool result) { EXPECT_FALSE(result); });

  key_manager_.SetKeyIfRequired(
      std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>{display_.get()},
      kDisplayId + 1, std::move(on_key_set));
}

TEST_F(ContentProtectionKeyManagerTest,
       TestIfKeySetWithMultipleDisplaysOneIsValid) {
  auto other_display = FakeDisplaySnapshot::Builder()
                           .SetId(kDisplayId + 1)
                           .SetType(DISPLAY_CONNECTION_TYPE_DISPLAYPORT)
                           .SetHasContentProtectionKey(true)
                           .SetCurrentMode(kDisplayMode.Clone())
                           .Build();

  auto on_key_set = base::BindOnce([](bool result) { EXPECT_TRUE(result); });

  SetProvisionedKeyRequest(true);

  auto displays = std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>{
      display_.get(), other_display.get()};
  key_manager_.SetKeyIfRequired(displays, kDisplayId, std::move(on_key_set));

  EXPECT_EQ(GetSetHdcpKeyPropAction(kDisplayId, true),
            log_.GetActionsAndClear());
}

TEST_F(ContentProtectionKeyManagerTest, TestIfKeyNotSetWhenDisplayIsEdp) {
  display_ = FakeDisplaySnapshot::Builder()
                 .SetId(kDisplayId)
                 .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                 .SetHasContentProtectionKey(true)
                 .SetCurrentMode(kDisplayMode.Clone())
                 .Build();

  auto on_key_set = base::BindOnce([](bool result) { EXPECT_FALSE(result); });

  SetKeyIfRequiredForDisplay(std::move(on_key_set));
}

TEST_F(ContentProtectionKeyManagerTest,
       TestIfKeyNotSetWhenDisplayHasNoContentProtectionKeyProp) {
  display_ = FakeDisplaySnapshot::Builder()
                 .SetId(kDisplayId)
                 .SetType(DISPLAY_CONNECTION_TYPE_DISPLAYPORT)
                 .SetHasContentProtectionKey(false)
                 .SetCurrentMode(kDisplayMode.Clone())
                 .Build();

  auto on_key_set = base::BindOnce([](bool result) { EXPECT_FALSE(result); });

  SetKeyIfRequiredForDisplay(std::move(on_key_set));
}

TEST_F(ContentProtectionKeyManagerTest, TestThatKeyIsSetForMultipleDisplays) {
  SetProvisionedKeyRequest(true);

  auto on_key_set = base::BindOnce([](bool result) { EXPECT_TRUE(result); });
  key_manager_.SetKeyIfRequired(
      std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>{display_.get()},
      kDisplayId, std::move(on_key_set));

  auto other_display = FakeDisplaySnapshot::Builder()
                           .SetId(kDisplayId + 1)
                           .SetType(DISPLAY_CONNECTION_TYPE_DISPLAYPORT)
                           .SetHasContentProtectionKey(true)
                           .SetCurrentMode(kDisplayMode.Clone())
                           .Build();
  on_key_set = base::BindOnce([](bool result) { EXPECT_TRUE(result); });
  key_manager_.SetKeyIfRequired(
      std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>{
          other_display.get()},
      kDisplayId + 1, std::move(on_key_set));

  EXPECT_EQ(GetSetHdcpKeyPropAction(kDisplayId, true) + "," +
                GetSetHdcpKeyPropAction(kDisplayId + 1, true),
            log_.GetActionsAndClear());
}

TEST_F(ContentProtectionKeyManagerTest,
       TestThatKeyIsFetchedOnlyOnceIfKeyValid) {
  key_manager_.set_provisioned_key_request(
      base::BindRepeating([](base::OnceCallback<void(const std::string&)> cb) {
        static int fetch_key_call_count = 0;

        fetch_key_call_count++;
        std::move(cb).Run(kFakeKey285);

        EXPECT_EQ(1, fetch_key_call_count);
      }));

  auto on_key_set = base::BindOnce([](bool result) { EXPECT_TRUE(result); });

  SetKeyIfRequiredForDisplay(std::move(on_key_set));

  on_key_set = base::BindOnce([](bool result) { EXPECT_TRUE(true); });

  key_manager_.SetKeyIfRequired(
      std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>{display_.get()},
      kDisplayId + 1, std::move(on_key_set));
}

TEST_F(ContentProtectionKeyManagerTest, TestThatKeyIsFetchedAgainIfKeyInvalid) {
  key_manager_.set_provisioned_key_request(
      base::BindRepeating([](base::OnceCallback<void(const std::string&)> cb) {
        static int fetch_key_call_count = 0;

        fetch_key_call_count++;
        if (fetch_key_call_count == 1) {
          std::move(cb).Run("");
        } else {
          std::move(cb).Run(kFakeKey285);

          EXPECT_EQ(2, fetch_key_call_count);
        }
      }));

  auto on_key_set = base::BindOnce([](bool result) { EXPECT_FALSE(result); });

  SetKeyIfRequiredForDisplay(std::move(on_key_set));

  on_key_set = base::BindOnce([](bool result) { EXPECT_TRUE(true); });

  key_manager_.SetKeyIfRequired(
      std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>{display_.get()},
      kDisplayId + 1, std::move(on_key_set));
}

}  // namespace display::test
