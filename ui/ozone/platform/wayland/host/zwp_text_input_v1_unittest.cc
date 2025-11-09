// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_text_input_v1.h"

#include <sys/mman.h>

#include <string_view>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "text-input-unstable-v1-server-protocol.h"
#include "ui/ozone/platform/wayland/host/span_style.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_text_input.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_text_input_v1_client.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;

namespace ui {

class ZwpTextInputV1Test : public WaylandTestSimple {
 public:
  void SetUp() override {
    WaylandTestSimple::SetUp();

    text_input_v1_ = std::make_unique<ZwpTextInputV1Impl>(
        connection_.get(), connection_->text_input_manager_v1());
    text_input_v1_->SetClient(&test_client_);
  }

 protected:
  MockZwpTextInputV1Client test_client_;
  std::unique_ptr<ZwpTextInputV1> text_input_v1_;
};

TEST_F(ZwpTextInputV1Test, OnPreeditString) {
  constexpr std::string_view kPreeditString("PreeditString");
  constexpr int32_t kPreeditCursor = kPreeditString.size();
  EXPECT_CALL(test_client_,
              OnPreeditString(kPreeditString,
                              std::vector<SpanStyle>{
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

TEST_F(ZwpTextInputV1Test, OnKeySym_TimestampPropagated) {
  uint32_t test_time = 666;

  EXPECT_CALL(test_client_, OnKeysym(_, _, _, test_time));
  PostToServerAndWait([test_time](wl::TestWaylandServerThread* server) {
    zwp_text_input_v1_send_keysym(
        server->text_input_manager_v1()->text_input()->resource(), 0, test_time,
        0, 0, 0);
  });
}

}  // namespace ui
