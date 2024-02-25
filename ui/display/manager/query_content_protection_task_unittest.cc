// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/query_content_protection_task.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_layout_manager.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/manager/test/test_display_layout_manager.h"
#include "ui/display/manager/test/test_native_display_delegate.h"

namespace display::test {

namespace {

std::unique_ptr<DisplaySnapshot> CreateDisplaySnapshot(
    int64_t id,
    DisplayConnectionType type) {
  return FakeDisplaySnapshot::Builder()
      .SetId(id)
      .SetNativeMode(gfx::Size(1024, 768))
      .SetType(type)
      .Build();
}

}  // namespace

class QueryContentProtectionTaskTest : public testing::Test {
 public:
  using Status = QueryContentProtectionTask::Status;

  QueryContentProtectionTaskTest() = default;

  QueryContentProtectionTaskTest(const QueryContentProtectionTaskTest&) =
      delete;
  QueryContentProtectionTaskTest& operator=(
      const QueryContentProtectionTaskTest&) = delete;

  ~QueryContentProtectionTaskTest() override = default;

  void ResponseCallback(Status status,
                        uint32_t connection_mask,
                        uint32_t protection_mask) {
    response_ = Response{status, connection_mask, protection_mask};
  }

 protected:
  ActionLogger log_;
  TestNativeDisplayDelegate display_delegate_{&log_};

  struct Response {
    Status status;
    uint32_t connection_mask;
    uint32_t protection_mask;
  };

  std::optional<Response> response_;
};

TEST_F(QueryContentProtectionTaskTest, QueryInternalDisplay) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(
      CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_INTERNAL));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);

  QueryContentProtectionTask task(
      &layout_manager, &display_delegate_, 1,
      base::BindOnce(&QueryContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  ASSERT_TRUE(response_);
  EXPECT_EQ(Status::SUCCESS, response_->status);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_INTERNAL, response_->connection_mask);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE, response_->protection_mask);
}

TEST_F(QueryContentProtectionTaskTest, QueryUnknownDisplay) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_UNKNOWN));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);

  QueryContentProtectionTask task(
      &layout_manager, &display_delegate_, 1,
      base::BindOnce(&QueryContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  ASSERT_TRUE(response_);
  EXPECT_EQ(Status::FAILURE, response_->status);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_UNKNOWN, response_->connection_mask);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE, response_->protection_mask);
}

TEST_F(QueryContentProtectionTaskTest, QueryDisplayThatCannotGetHdcp) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_HDMI));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);
  display_delegate_.set_get_hdcp_state_expectation(false);

  QueryContentProtectionTask task(
      &layout_manager, &display_delegate_, 1,
      base::BindOnce(&QueryContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  ASSERT_TRUE(response_);
  EXPECT_EQ(Status::FAILURE, response_->status);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_HDMI, response_->connection_mask);
}

TEST_F(QueryContentProtectionTaskTest, QueryDisplayWithHdcpDisabled) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_HDMI));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);

  QueryContentProtectionTask task(
      &layout_manager, &display_delegate_, 1,
      base::BindOnce(&QueryContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  ASSERT_TRUE(response_);
  EXPECT_EQ(Status::SUCCESS, response_->status);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_HDMI, response_->connection_mask);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE, response_->protection_mask);
}

TEST_F(QueryContentProtectionTaskTest, QueryDisplayWithHdcpType0Enabled) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_HDMI));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);
  display_delegate_.set_hdcp_state(HDCP_STATE_ENABLED);
  display_delegate_.set_content_protection_method(
      CONTENT_PROTECTION_METHOD_HDCP_TYPE_0);

  QueryContentProtectionTask task(
      &layout_manager, &display_delegate_, 1,
      base::BindOnce(&QueryContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  ASSERT_TRUE(response_);
  EXPECT_EQ(Status::SUCCESS, response_->status);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_HDMI, response_->connection_mask);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_HDCP_TYPE_0, response_->protection_mask);
}

TEST_F(QueryContentProtectionTaskTest, QueryDisplayWithHdcpType1Enabled) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_HDMI));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);
  display_delegate_.set_hdcp_state(HDCP_STATE_ENABLED);
  display_delegate_.set_content_protection_method(
      CONTENT_PROTECTION_METHOD_HDCP_TYPE_1);

  QueryContentProtectionTask task(
      &layout_manager, &display_delegate_, 1,
      base::BindOnce(&QueryContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  ASSERT_TRUE(response_);
  EXPECT_EQ(Status::SUCCESS, response_->status);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_HDMI, response_->connection_mask);
  // This should have both Type 0 and Type 1 set.
  EXPECT_EQ(kContentProtectionMethodHdcpAll, response_->protection_mask);
}

TEST_F(QueryContentProtectionTaskTest, QueryInMultiDisplayMode) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_HDMI));
  displays.push_back(CreateDisplaySnapshot(2, DISPLAY_CONNECTION_TYPE_DVI));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(
      display_delegate_.GetOutputs(), MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);

  QueryContentProtectionTask task(
      &layout_manager, &display_delegate_, 1,
      base::BindOnce(&QueryContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  ASSERT_TRUE(response_);
  EXPECT_EQ(Status::SUCCESS, response_->status);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_HDMI, response_->connection_mask);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE, response_->protection_mask);
}

TEST_F(QueryContentProtectionTaskTest, QueryInMirroringMode) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_HDMI));
  displays.push_back(CreateDisplaySnapshot(2, DISPLAY_CONNECTION_TYPE_DVI));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);

  QueryContentProtectionTask task(
      &layout_manager, &display_delegate_, 1,
      base::BindOnce(&QueryContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  ASSERT_TRUE(response_);
  EXPECT_EQ(Status::SUCCESS, response_->status);
  EXPECT_EQ(static_cast<uint32_t>(DISPLAY_CONNECTION_TYPE_HDMI |
                                  DISPLAY_CONNECTION_TYPE_DVI),
            response_->connection_mask);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE, response_->protection_mask);
}

TEST_F(QueryContentProtectionTaskTest, QueryAnalogDisplay) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_VGA));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);

  QueryContentProtectionTask task(
      &layout_manager, &display_delegate_, 1,
      base::BindOnce(&QueryContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task.Run();

  ASSERT_TRUE(response_);
  EXPECT_EQ(Status::SUCCESS, response_->status);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_VGA, response_->connection_mask);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE, response_->protection_mask);
}

TEST_F(QueryContentProtectionTaskTest, QueryAnalogDisplayMirror) {
  std::vector<std::unique_ptr<DisplaySnapshot>> displays;
  displays.push_back(CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_HDMI));
  displays.push_back(CreateDisplaySnapshot(2, DISPLAY_CONNECTION_TYPE_VGA));
  display_delegate_.SetOutputs(std::move(displays));
  TestDisplayLayoutManager layout_manager(display_delegate_.GetOutputs(),
                                          MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);

  display_delegate_.set_hdcp_state(HDCP_STATE_ENABLED);

  QueryContentProtectionTask task1(
      &layout_manager, &display_delegate_, 1,
      base::BindOnce(&QueryContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task1.Run();

  ASSERT_TRUE(response_);
  EXPECT_EQ(Status::SUCCESS, response_->status);
  EXPECT_EQ(static_cast<uint32_t>(DISPLAY_CONNECTION_TYPE_HDMI |
                                  DISPLAY_CONNECTION_TYPE_VGA),
            response_->connection_mask);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE, response_->protection_mask);

  response_.reset();

  QueryContentProtectionTask task2(
      &layout_manager, &display_delegate_, 2,
      base::BindOnce(&QueryContentProtectionTaskTest::ResponseCallback,
                     base::Unretained(this)));
  task2.Run();

  ASSERT_TRUE(response_);
  EXPECT_EQ(Status::SUCCESS, response_->status);
  EXPECT_EQ(static_cast<uint32_t>(DISPLAY_CONNECTION_TYPE_HDMI |
                                  DISPLAY_CONNECTION_TYPE_VGA),
            response_->connection_mask);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE, response_->protection_mask);
}

}  // namespace display::test
