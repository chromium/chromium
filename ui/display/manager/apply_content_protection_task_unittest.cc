// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/apply_content_protection_task.h"

#include <stdint.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/fake/fake_display_snapshot.h"
#include "ui/display/manager/display_layout_manager.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/manager/test/test_display_layout_manager.h"
#include "ui/display/manager/test/test_native_display_delegate.h"

namespace display {
namespace test {

namespace {

constexpr int64_t kDisplayId = 1;

std::unique_ptr<DisplaySnapshot> CreateDisplaySnapshot(
    DisplayConnectionType type) {
  return FakeDisplaySnapshot::Builder()
      .SetId(kDisplayId)
      .SetNativeMode(gfx::Size(1024, 768))
      .SetType(type)
      .Build();
}

}  // namespace

class ApplyContentProtectionTaskTest : public testing::Test {
 public:
  using Response = ApplyContentProtectionTask::Status;

  ApplyContentProtectionTaskTest() = default;
  ~ApplyContentProtectionTaskTest() override = default;

  void ResponseCallback(Response response) { response_ = response; }

 protected:
  Response response_ = Response::KILLED;
  ActionLogger log_;
  TestNativeDisplayDelegate display_delegate_{&log_};

 private:
  DISALLOW_COPY_AND_ASSIGN(ApplyContentProtectionTaskTest);
};

TEST_F(ApplyContentProtectionTaskTest, ApplyHdcpToInternalDisplay) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(DISPLAY_CONNECTION_TYPE_INTERNAL));
  TestDisplayLayoutManager layout_manager(std::move(displays),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);

  ContentProtectionManager::ContentProtections request;
  request[1] = CONTENT_PROTECTION_METHOD_HDCP;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::BindOnce(&ApplyContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  EXPECT_EQ(Response::SUCCESS, response_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
}

TEST_F(ApplyContentProtectionTaskTest, ApplyHdcpToExternalDisplay) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(DISPLAY_CONNECTION_TYPE_HDMI));
  TestDisplayLayoutManager layout_manager(std::move(displays),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);

  ContentProtectionManager::ContentProtections request;
  request[1] = CONTENT_PROTECTION_METHOD_HDCP;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::BindOnce(&ApplyContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  EXPECT_EQ(Response::SUCCESS, response_);
  EXPECT_EQ(GetSetHDCPStateAction(kDisplayId, HDCP_STATE_DESIRED).c_str(),
            log_.GetActionsAndClear());
}

TEST_F(ApplyContentProtectionTaskTest, ApplyHdcpToUnknownDisplay) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(DISPLAY_CONNECTION_TYPE_UNKNOWN));
  TestDisplayLayoutManager layout_manager(std::move(displays),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);

  ContentProtectionManager::ContentProtections request;
  request[1] = CONTENT_PROTECTION_METHOD_HDCP;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::BindOnce(&ApplyContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  EXPECT_EQ(Response::FAILURE, response_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
}

TEST_F(ApplyContentProtectionTaskTest, ApplyHdcpToDisplayThatCannotGetHdcp) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(DISPLAY_CONNECTION_TYPE_HDMI));
  TestDisplayLayoutManager layout_manager(std::move(displays),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);
  display_delegate_.set_get_hdcp_state_expectation(false);

  ContentProtectionManager::ContentProtections request;
  request[1] = CONTENT_PROTECTION_METHOD_HDCP;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::BindOnce(&ApplyContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  EXPECT_EQ(Response::FAILURE, response_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
}

TEST_F(ApplyContentProtectionTaskTest, ApplyHdcpToDisplayThatCannotSetHdcp) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(DISPLAY_CONNECTION_TYPE_HDMI));
  TestDisplayLayoutManager layout_manager(std::move(displays),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);
  display_delegate_.set_set_hdcp_state_expectation(false);

  ContentProtectionManager::ContentProtections request;
  request[1] = CONTENT_PROTECTION_METHOD_HDCP;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::BindOnce(&ApplyContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  EXPECT_EQ(Response::FAILURE, response_);
  EXPECT_EQ(GetSetHDCPStateAction(kDisplayId, HDCP_STATE_DESIRED).c_str(),
            log_.GetActionsAndClear());
}

TEST_F(ApplyContentProtectionTaskTest, ApplyNoProtectionToExternalDisplay) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(DISPLAY_CONNECTION_TYPE_HDMI));
  TestDisplayLayoutManager layout_manager(std::move(displays),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);
  display_delegate_.set_hdcp_state(HDCP_STATE_UNDESIRED);

  ContentProtectionManager::ContentProtections request;
  request[1] = CONTENT_PROTECTION_METHOD_NONE;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::BindOnce(&ApplyContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  EXPECT_EQ(Response::SUCCESS, response_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
}

}  // namespace test
}  // namespace display
