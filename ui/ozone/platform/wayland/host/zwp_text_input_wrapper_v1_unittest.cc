// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper_v1.h"

#include <sys/mman.h>

#include <string_view>

#include "base/files/file_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "text-input-unstable-v1-server-protocol.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/test/mock_zcr_extended_text_input.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_text_input.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_text_input_wrapper_client.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zcr_text_input_extension.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;

namespace ui {

class ZWPTextInputWrapperV1Test : public WaylandTestSimple {
 public:
  void SetUp() override {
    WaylandTestSimple::SetUp();

    wrapper_ = std::make_unique<ZWPTextInputWrapperV1>(
        connection_.get(), &test_client_, connection_->text_input_manager_v1(),
        connection_->text_input_extension_v1());
  }

 protected:
  MockZWPTextInputWrapperClient test_client_;
  std::unique_ptr<ZWPTextInputWrapperV1> wrapper_;
};

TEST_F(ZWPTextInputWrapperV1Test, OnPreeditString) {
  constexpr std::string_view kPreeditString("PreeditString");
  constexpr int32_t kPreeditCursor = kPreeditString.size();
  EXPECT_CALL(test_client_,
              OnPreeditString(kPreeditString,
                              std::vector<ZWPTextInputWrapperClient::SpanStyle>{
                                  {0,
                                   static_cast<uint32_t>(kPreeditString.size()),
                                   {{ImeTextSpan::Type::kComposition,
                                     ImeTextSpan::Thickness::kThin}}}},
                              gfx::Range(kPreeditCursor)));
  PostToServerAndWait([kPreeditString](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v1()->text_input();
    zwp_text_input_v1_send_preedit_cursor(text_input->resource(),
                                          kPreeditCursor);
    zwp_text_input_v1_send_preedit_styling(
        text_input->resource(), 0, kPreeditString.size(),
        ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_UNDERLINE);
    zwp_text_input_v1_send_preedit_string(text_input->resource(),
                                          server->GetNextSerial(),
                                          kPreeditString.data(), "");
  });
}

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

TEST_F(ZWPTextInputWrapperV1Test, OnKeySym_TimestampPropagated) {
  uint32_t test_time = 666;

  EXPECT_CALL(test_client_, OnKeysym(_, _, _, test_time));
  PostToServerAndWait([test_time](wl::TestWaylandServerThread* server) {
    zwp_text_input_v1_send_keysym(
        server->text_input_manager_v1()->text_input()->resource(), 0, test_time,
        0, 0, 0);
  });
}

TEST_F(ZWPTextInputWrapperV1Test, OnInsertImageWithLargeURL) {
  std::string mime_type = "image/jpeg";
  std::string charset = "";
  std::string raw_bytes = "[fake image binary]";

  base::ScopedFD memfd(memfd_create("inserting_image", MFD_CLOEXEC));
  if (!memfd.get()) {
    LOG(ERROR) << "Failed to create memfd";
    return;
  }

  if (!base::WriteFileDescriptor(memfd.get(), raw_bytes)) {
    LOG(ERROR) << "Failed to write into memfd";
    return;
  }

  if (lseek(memfd.get(), 0, SEEK_SET) != 0) {
    LOG(ERROR) << "Failed to reset file descriptor";
    return;
  }

  GURL src("data:image/jpeg;base64,W2Zha2UgaW1hZ2UgYmluYXJ5XQ==");
  EXPECT_CALL(test_client_, OnInsertImage(src));
  PostToServerAndWait([&mime_type, &charset, &raw_bytes,
                       &memfd](wl::TestWaylandServerThread* server) {
    zcr_extended_text_input_v1_send_insert_image_with_large_url(
        server->text_input_extension_v1()->extended_text_input()->resource(),
        mime_type.c_str(), charset.c_str(), memfd.get(), raw_bytes.size());
  });
}

}  // namespace ui
