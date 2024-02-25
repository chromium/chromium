// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/apply_content_protection_task.h"

#include <stdint.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_layout_manager.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/manager/test/test_display_layout_manager.h"
#include "ui/display/manager/test/test_native_display_delegate.h"

namespace display::test {

namespace {

constexpr int64_t kDisplayId1 = 1;
constexpr int64_t kDisplayId2 = 2;

std::unique_ptr<DisplaySnapshot> CreateDisplaySnapshot(
    DisplayConnectionType type,
    int display_id = kDisplayId1) {
  return FakeDisplaySnapshot::Builder()
      .SetId(display_id)
      .SetNativeMode(gfx::Size(1024, 768))
      .SetType(type)
      .Build();
}

}  // namespace

class ApplyContentProtectionTaskTest : public testing::Test {
 public:
  using Response = ApplyContentProtectionTask::Status;

  ApplyContentProtectionTaskTest() = default;

  ApplyContentProtectionTaskTest(const ApplyContentProtectionTaskTest&) =
      delete;
  ApplyContentProtectionTaskTest& operator=(
      const ApplyContentProtectionTaskTest&) = delete;

  ~ApplyContentProtectionTaskTest() override = default;

  void ResponseCallback(Response response) { response_ = response; }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  Response response_ = Response::KILLED;
  ActionLogger log_;
  TestNativeDisplayDelegate display_delegate_{&log_};
};

TEST_F(ApplyContentProtectionTaskTest, ApplyHdcpToInternalDisplay) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(DISPLAY_CONNECTION_TYPE_INTERNAL));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);

  ContentProtectionManager::ContentProtections request;
  request[kDisplayId1] = CONTENT_PROTECTION_METHOD_HDCP;
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
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);

  ContentProtectionManager::ContentProtections request;
  request[kDisplayId1] = CONTENT_PROTECTION_METHOD_HDCP;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::BindOnce(&ApplyContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  EXPECT_EQ(Response::SUCCESS, response_);
  EXPECT_EQ(GetSetHDCPStateAction(kDisplayId1, HDCP_STATE_DESIRED,
                                  CONTENT_PROTECTION_METHOD_HDCP)
                .c_str(),
            log_.GetActionsAndClear());
}

TEST_F(ApplyContentProtectionTaskTest, ApplyHdcpToUnknownDisplay) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(DISPLAY_CONNECTION_TYPE_UNKNOWN));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);

  ContentProtectionManager::ContentProtections request;
  request[kDisplayId1] = CONTENT_PROTECTION_METHOD_HDCP;
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
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);
  display_delegate_.set_get_hdcp_state_expectation(false);

  ContentProtectionManager::ContentProtections request;
  request[kDisplayId1] = CONTENT_PROTECTION_METHOD_HDCP;
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
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);
  display_delegate_.set_set_hdcp_state_expectation(false);

  ContentProtectionManager::ContentProtections request;
  request[kDisplayId1] = CONTENT_PROTECTION_METHOD_HDCP;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::BindOnce(&ApplyContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  EXPECT_EQ(Response::FAILURE, response_);
  EXPECT_EQ(GetSetHDCPStateAction(kDisplayId1, HDCP_STATE_DESIRED,
                                  CONTENT_PROTECTION_METHOD_HDCP)
                .c_str(),
            log_.GetActionsAndClear());
}

TEST_F(ApplyContentProtectionTaskTest, ApplyNoProtectionToExternalDisplay) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(DISPLAY_CONNECTION_TYPE_HDMI));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);
  display_delegate_.set_hdcp_state(HDCP_STATE_UNDESIRED);

  ContentProtectionManager::ContentProtections request;
  request[kDisplayId1] = CONTENT_PROTECTION_METHOD_NONE;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::BindOnce(&ApplyContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  EXPECT_EQ(Response::SUCCESS, response_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
}

TEST_F(ApplyContentProtectionTaskTest, ApplyHdcpWhileConfiguringDisplays) {
  // Run async so the test can simulate a display change in the middle of
  // updating HDCP state.
  display_delegate_.set_run_async(true);

  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(DISPLAY_CONNECTION_TYPE_HDMI));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);

  ContentProtectionManager::ContentProtections request;
  request[kDisplayId1] = CONTENT_PROTECTION_METHOD_HDCP;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::BindOnce(&ApplyContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();
  // Content protection task asked for HDCP state. The response is queued on the
  // task runner. At this point clear the display state. Content protection task
  // should re-query state and respond with failure since the display is no
  // longer present.
  layout_manager.set_displays({});

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(Response::FAILURE, response_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
}

TEST_F(ApplyContentProtectionTaskTest, ApplyHdcpToMultipleMonitors) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(
      CreateDisplaySnapshot(DISPLAY_CONNECTION_TYPE_HDMI, kDisplayId1));
  displays.push_back(
      CreateDisplaySnapshot(DISPLAY_CONNECTION_TYPE_HDMI, kDisplayId2));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(
      display_delegate_.GetOutputs(), MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);

  ContentProtectionManager::ContentProtections request;
  request[kDisplayId1] = CONTENT_PROTECTION_METHOD_HDCP_TYPE_0;
  request[kDisplayId2] = CONTENT_PROTECTION_METHOD_HDCP_TYPE_1;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::BindOnce(&ApplyContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  EXPECT_EQ(Response::SUCCESS, response_);
  EXPECT_EQ(
      JoinActions(GetSetHDCPStateAction(kDisplayId1, HDCP_STATE_DESIRED,
                                        CONTENT_PROTECTION_METHOD_HDCP_TYPE_0)
                      .c_str(),
                  GetSetHDCPStateAction(kDisplayId2, HDCP_STATE_DESIRED,
                                        CONTENT_PROTECTION_METHOD_HDCP_TYPE_1)
                      .c_str(),
                  nullptr),
      log_.GetActionsAndClear());
}

TEST_F(ApplyContentProtectionTaskTest, ApplyHdcpToMirroredMonitors) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(
      CreateDisplaySnapshot(DISPLAY_CONNECTION_TYPE_HDMI, kDisplayId1));
  displays.push_back(
      CreateDisplaySnapshot(DISPLAY_CONNECTION_TYPE_HDMI, kDisplayId2));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);

  ContentProtectionManager::ContentProtections request;
  request[kDisplayId1] = CONTENT_PROTECTION_METHOD_HDCP_TYPE_1;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::BindOnce(&ApplyContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  EXPECT_EQ(Response::SUCCESS, response_);
  EXPECT_EQ(
      JoinActions(GetSetHDCPStateAction(kDisplayId1, HDCP_STATE_DESIRED,
                                        CONTENT_PROTECTION_METHOD_HDCP_TYPE_1)
                      .c_str(),
                  GetSetHDCPStateAction(kDisplayId2, HDCP_STATE_DESIRED,
                                        CONTENT_PROTECTION_METHOD_HDCP_TYPE_1)
                      .c_str(),
                  nullptr),
      log_.GetActionsAndClear());
}

}  // namespace display::test
