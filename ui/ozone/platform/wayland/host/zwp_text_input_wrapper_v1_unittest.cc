// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper_v1.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/test/mock_zcr_extended_text_input.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_text_input.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zcr_text_input_extension.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::InSequence;
using ::testing::Mock;

namespace ui {

class ZWPTextInputWrapperV1Test : public WaylandTestSimple {
 public:
  void SetUp() override {
    WaylandTestSimple::SetUp();

    wrapper_ = std::make_unique<ZWPTextInputWrapperV1>(
        connection_.get(), nullptr, connection_->text_input_manager_v1(),
        connection_->text_input_extension_v1());
  }

 protected:
  std::unique_ptr<ZWPTextInputWrapperV1> wrapper_;
};

TEST_F(ZWPTextInputWrapperV1Test,
       FinalizeVirtualKeyboardChangesShowInputPanel) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                ShowInputPanel());
    EXPECT_CALL(*server->text_input_extension_v1()->extended_text_input(),
                FinalizeVirtualKeyboardChanges());
  });

  wrapper_->ShowInputPanel();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_manager_v1()->text_input());
    Mock::VerifyAndClearExpectations(server->text_input_extension_v1());
  });
}

TEST_F(ZWPTextInputWrapperV1Test,
       FinalizeVirtualKeyboardChangesHideInputPanel) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                HideInputPanel());
    EXPECT_CALL(*server->text_input_extension_v1()->extended_text_input(),
                FinalizeVirtualKeyboardChanges());
  });

  wrapper_->HideInputPanel();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_manager_v1()->text_input());
  });

  // The text input extension gets updated and called after another round trip.

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_extension_v1()->extended_text_input());
  });
}

TEST_F(ZWPTextInputWrapperV1Test,
       FinalizeVirtualKeyboardChangesMultipleInputPanelChanges) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const mock_text_input = server->text_input_manager_v1()->text_input();
    InSequence s;
    EXPECT_CALL(*mock_text_input, ShowInputPanel());
    EXPECT_CALL(*mock_text_input, HideInputPanel());
    EXPECT_CALL(*mock_text_input, ShowInputPanel());
    EXPECT_CALL(*mock_text_input, HideInputPanel());
    EXPECT_CALL(*mock_text_input, ShowInputPanel());
    EXPECT_CALL(*server->text_input_extension_v1()->extended_text_input(),
                FinalizeVirtualKeyboardChanges());
  });

  wrapper_->ShowInputPanel();
  wrapper_->HideInputPanel();
  wrapper_->ShowInputPanel();
  wrapper_->HideInputPanel();
  wrapper_->ShowInputPanel();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_manager_v1()->text_input());
  });

  // The text input extension gets updated and called after another round trip.

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_extension_v1()->extended_text_input());
  });
}

}  // namespace ui
