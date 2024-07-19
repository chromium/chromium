// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper_v3.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_text_input.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zwp_text_input_wrapper_client.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::InSequence;

namespace ui {

class ZWPTextInputWrapperV3Test : public WaylandTestSimple {
 public:
  ZWPTextInputWrapperV3Test()
      : WaylandTestSimple(
            {.text_input_wrapper_type = ui::ZWPTextInputWrapperType::kV3}) {}

  void SetUp() override {
    WaylandTestSimple::SetUp();

    wrapper_ = std::make_unique<ZWPTextInputWrapperV3>(
        connection_.get(), &test_client_, connection_->text_input_manager_v3());
  }

 protected:
  TestZWPTextInputWrapperClient test_client_;
  std::unique_ptr<ZWPTextInputWrapperV3> wrapper_;
};

TEST_F(ZWPTextInputWrapperV3Test, Activate) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Enable())
        .Times(1);
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Commit())
        .Times(1);
  });
  wrapper_->Activate(window_.get(), ui::TextInputClient::FOCUS_REASON_NONE);
}

TEST_F(ZWPTextInputWrapperV3Test, Deactivate) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Disable())
        .Times(1);
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Commit())
        .Times(1);
  });
  wrapper_->Deactivate();
}

TEST_F(ZWPTextInputWrapperV3Test, Reset) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Disable())
        .Times(1);
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Commit())
        .Times(1);
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Enable())
        .Times(1);
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Commit())
        .Times(1);
  });
  wrapper_->Reset();
}

}  // namespace ui
