// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper_v1.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/test/mock_zcr_extended_text_input.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_text_input.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zcr_text_input_extension.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::InSequence;
using ::testing::Values;

namespace ui {
namespace {

class ZWPTextInputWrapperV1Test : public WaylandTest {
 public:
  ZWPTextInputWrapperV1Test() = default;
  ZWPTextInputWrapperV1Test(const ZWPTextInputWrapperV1Test&) = delete;
  ZWPTextInputWrapperV1Test& operator=(const ZWPTextInputWrapperV1Test&) =
      delete;
  ~ZWPTextInputWrapperV1Test() override = default;

  void SetUp() override {
    WaylandTest::SetUp();

    wrapper_ = std::make_unique<ZWPTextInputWrapperV1>(
        connection_.get(), nullptr, connection_->text_input_manager_v1(),
        connection_->text_input_extension_v1());

    connection_->Flush();
    Sync();

    mock_text_input_ = server_.text_input_manager_v1()->text_input();
    ASSERT_TRUE(mock_text_input_);
    mock_ext_text_input_ =
        server_.text_input_extension_v1()->extended_text_input();
    ASSERT_TRUE(mock_ext_text_input_);
  }

 protected:
  std::unique_ptr<ZWPTextInputWrapperV1> wrapper_;
  wl::MockZwpTextInput* mock_text_input_ = nullptr;
  wl::MockZcrExtendedTextInput* mock_ext_text_input_ = nullptr;
};

TEST_P(ZWPTextInputWrapperV1Test,
       FinalizeVirtualKeyboardChangesShowInputPanel) {
  InSequence s;
  EXPECT_CALL(*mock_text_input_, ShowInputPanel());
  EXPECT_CALL(*mock_ext_text_input_, FinalizeVirtualKeyboardChanges());
  wrapper_->ShowInputPanel();
  connection_->Flush();
  Sync();

  // Flush again after sync, so the scheduled finalize request is processed.
  connection_->Flush();
  Sync();
}

TEST_P(ZWPTextInputWrapperV1Test,
       FinalizeVirtualKeyboardChangesHideInputPanel) {
  InSequence s;
  EXPECT_CALL(*mock_text_input_, HideInputPanel());
  EXPECT_CALL(*mock_ext_text_input_, FinalizeVirtualKeyboardChanges());
  wrapper_->HideInputPanel();
  connection_->Flush();
  Sync();

  // Flush again after sync, so the scheduled finalize request is processed.
  connection_->Flush();
  Sync();
}

TEST_P(ZWPTextInputWrapperV1Test,
       FinalizeVirtualKeyboardChangesMultipleInputPanelChanges) {
  InSequence s;
  EXPECT_CALL(*mock_text_input_, ShowInputPanel());
  EXPECT_CALL(*mock_text_input_, HideInputPanel());
  EXPECT_CALL(*mock_text_input_, ShowInputPanel());
  EXPECT_CALL(*mock_text_input_, HideInputPanel());
  EXPECT_CALL(*mock_text_input_, ShowInputPanel());
  EXPECT_CALL(*mock_ext_text_input_, FinalizeVirtualKeyboardChanges());
  wrapper_->ShowInputPanel();
  wrapper_->HideInputPanel();
  wrapper_->ShowInputPanel();
  wrapper_->HideInputPanel();
  wrapper_->ShowInputPanel();
  connection_->Flush();
  Sync();

  // Flush again after sync, so the scheduled finalize request is processed.
  connection_->Flush();
  Sync();

  // Flush and sync again to make sure no extra finalize request.
  connection_->Flush();
  Sync();
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         ZWPTextInputWrapperV1Test,
                         Values(wl::ServerConfig{}));

}  // namespace
}  // namespace ui
