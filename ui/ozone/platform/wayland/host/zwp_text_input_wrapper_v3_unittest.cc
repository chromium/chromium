// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper_v3.h"

#include <text-input-unstable-v3-server-protocol.h>

#include <memory>
#include <string_view>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_text_input.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_text_input_wrapper_client.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;

namespace ui {

class ZWPTextInputWrapperV3Test : public WaylandTestSimple {
 public:
  ZWPTextInputWrapperV3Test()
      : WaylandTestSimple(
            {.text_input_wrapper_type = wl::ZWPTextInputWrapperType::kV3}) {}

  void SetUp() override {
    WaylandTestSimple::SetUp();

    wrapper_ = std::make_unique<ZWPTextInputWrapperV3>(
        connection_.get(), &test_client_, connection_->text_input_manager_v3());
  }

 protected:
  void VerifyAndClearExpectations() {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      Mock::VerifyAndClearExpectations(
          server->text_input_extension_v1()->extended_text_input());
    });
    Mock::VerifyAndClearExpectations(&test_client_);
  }

  MockZWPTextInputWrapperClient test_client_;
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

TEST_F(ZWPTextInputWrapperV3Test, ShowInputPanel) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Enable())
        .Times(0);
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Disable())
        .Times(0);
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Commit())
        .Times(0);
  });
  wrapper_->ShowInputPanel();
}

TEST_F(ZWPTextInputWrapperV3Test, HideInputPanel) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Enable())
        .Times(0);
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Disable())
        .Times(0);
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Commit())
        .Times(0);
  });
  wrapper_->HideInputPanel();
}

TEST_F(ZWPTextInputWrapperV3Test, SetContentType) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input,
                SetContentType(ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK,
                               ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->SetContentType(TEXT_INPUT_TYPE_EMAIL, TEXT_INPUT_MODE_NONE,
                           TEXT_INPUT_FLAG_AUTOCORRECT_ON, false, false);
  VerifyAndClearExpectations();

  // Calling again with the same values should be a no-op.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
    // Commit has been called once. So send done serial matching commit.
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
  });
  wrapper_->SetContentType(TEXT_INPUT_TYPE_EMAIL, TEXT_INPUT_MODE_NONE,
                           TEXT_INPUT_FLAG_AUTOCORRECT_ON, false, false);
  VerifyAndClearExpectations();

  // Calling with different values should work.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(
        *zwp_text_input,
        SetContentType(ZWP_TEXT_INPUT_V3_CONTENT_HINT_AUTO_CAPITALIZATION,
                       ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NUMBER))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->SetContentType(TEXT_INPUT_TYPE_NUMBER, TEXT_INPUT_MODE_NONE,
                           TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS, false, false);
}

TEST_F(ZWPTextInputWrapperV3Test, SetCursorRect) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  PostToServerAndWait([kRect](wl::TestWaylandServerThread* server) {
    InSequence s;
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(kRect.x(), kRect.y(),
                                               kRect.width(), kRect.height()));
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->SetCursorRect(kRect);
  VerifyAndClearExpectations();

  // Calling again with the same values should be a no-op.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
    // Commit has been called once. So send done serial matching commit.
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
  });
  wrapper_->SetCursorRect(kRect);
  VerifyAndClearExpectations();

  // Calling again with different values should work.
  constexpr gfx::Rect kRect2(100, 20, 1, 1);
  PostToServerAndWait([kRect2](wl::TestWaylandServerThread* server) {
    InSequence s;
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(
        *zwp_text_input,
        SetCursorRect(kRect2.x(), kRect2.y(), kRect2.width(), kRect2.height()));
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->SetCursorRect(kRect2);
  VerifyAndClearExpectations();
}

TEST_F(ZWPTextInputWrapperV3Test, SetSurroundingText) {
  const std::string text("surroundingすしはおいしいですtext");
  constexpr std::string kSurroundingText("surroundingtext");
  constexpr gfx::Range kPreeditRange(11, 38);
  gfx::Range selection_range;

  // cursor at end of preedit
  selection_range = {38, 38};
  PostToServerAndWait([kSurroundingText](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(),
                SetSurroundingText(kSurroundingText, gfx::Range{11, 11}))
        .Times(1);
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Commit())
        .Times(1);
  });
  wrapper_->SetSurroundingText(text, kPreeditRange, selection_range);
  VerifyAndClearExpectations();

  // values unchanged
  const std::string text2("surroundingこんにちはtext");
  constexpr gfx::Range preedit_range2(11, 26);
  selection_range = {26, 26};
  PostToServerAndWait([kSurroundingText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
    // Ensure done serial matches commit count.
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
  });
  wrapper_->SetSurroundingText(text2, preedit_range2, selection_range);
  VerifyAndClearExpectations();

  // selection before preedit
  selection_range = {8, 9};
  PostToServerAndWait([kSurroundingText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input,
                SetSurroundingText(kSurroundingText, gfx::Range{8, 9}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->SetSurroundingText(text, kPreeditRange, selection_range);
  VerifyAndClearExpectations();

  // selection bounded by preedit
  selection_range = {14, 23};
  PostToServerAndWait([kSurroundingText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input,
                SetSurroundingText(kSurroundingText, gfx::Range{11, 11}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 2);
  });
  wrapper_->SetSurroundingText(text, kPreeditRange, selection_range);
  VerifyAndClearExpectations();

  // selection starts inside preedit and ends after preedit
  selection_range = {35, 44};
  PostToServerAndWait([kSurroundingText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input,
                SetSurroundingText(kSurroundingText, gfx::Range{11, 17}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 3);
  });
  wrapper_->SetSurroundingText(text, kPreeditRange, selection_range);
  VerifyAndClearExpectations();

  // selection starts inside preedit and ends after preedit (inverted selection)
  selection_range = {44, 35};
  PostToServerAndWait([kSurroundingText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input,
                SetSurroundingText(kSurroundingText, gfx::Range{17, 11}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 4);
  });
  wrapper_->SetSurroundingText(text, kPreeditRange, selection_range);
  VerifyAndClearExpectations();

  // selection starts before preedit and ends inside preedit
  selection_range = {8, 26};
  PostToServerAndWait([kSurroundingText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input,
                SetSurroundingText(kSurroundingText, gfx::Range{8, 11}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 5);
  });
  wrapper_->SetSurroundingText(text, kPreeditRange, selection_range);
  VerifyAndClearExpectations();

  // selection starts before preedit and ends inside preedit (inverted
  // selection)
  selection_range = {26, 8};
  PostToServerAndWait([kSurroundingText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input,
                SetSurroundingText(kSurroundingText, gfx::Range{11, 8}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 6);
  });
  wrapper_->SetSurroundingText(text, kPreeditRange, selection_range);
  VerifyAndClearExpectations();

  // selection starts before preedit and ends after preedit
  selection_range = {8, 40};
  PostToServerAndWait([kSurroundingText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input,
                SetSurroundingText(kSurroundingText, gfx::Range{8, 13}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 7);
  });
  wrapper_->SetSurroundingText(text, kPreeditRange, selection_range);
  VerifyAndClearExpectations();

  // selection after preedit
  selection_range = {41, 44};
  PostToServerAndWait([kSurroundingText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input,
                SetSurroundingText(kSurroundingText, gfx::Range{14, 17}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 8);
  });
  wrapper_->SetSurroundingText(text, kPreeditRange, selection_range);
  VerifyAndClearExpectations();

  // invalid preedit
  selection_range = {12, 12};
  PostToServerAndWait([kSurroundingText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input,
                SetSurroundingText(kSurroundingText, gfx::Range{12, 12}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 9);
  });
  wrapper_->SetSurroundingText(kSurroundingText, gfx::Range::InvalidRange(),
                               selection_range);
  VerifyAndClearExpectations();

  // invalid selection
  PostToServerAndWait([kSurroundingText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input,
                SetSurroundingText(kSurroundingText, gfx::Range{11, 11}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 10);
  });
  wrapper_->SetSurroundingText(text, kPreeditRange, gfx::Range::InvalidRange());
  VerifyAndClearExpectations();

  // invalid preedit and selection
  PostToServerAndWait([text](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(text, gfx::Range{42, 42}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 11);
  });
  wrapper_->SetSurroundingText(text, gfx::Range::InvalidRange(),
                               gfx::Range::InvalidRange());
  VerifyAndClearExpectations();
}

TEST_F(ZWPTextInputWrapperV3Test, PendingRequestsSentOnDone) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  const std::string text("surroundingすしはおいしいですtext");
  constexpr std::string kSurroundingText("surroundingtext");
  constexpr gfx::Range kPreeditRange(11, 38);
  constexpr gfx::Range kSelectionRange(38, 38);

  // Trigger 2 commits by calling activate.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->Activate(window_.get(), ui::TextInputClient::FOCUS_REASON_NONE);
  VerifyAndClearExpectations();
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->Activate(window_.get(), ui::TextInputClient::FOCUS_REASON_NONE);
  VerifyAndClearExpectations();

  // Now if commit number doesn't match done serial it shouldn't send a request.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
    // Commit has been called twice. So done serial 1 should not match.
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
  });
  wrapper_->SetSurroundingText(text, kPreeditRange, kSelectionRange);
  wrapper_->SetCursorRect(kRect);
  wrapper_->SetContentType(TEXT_INPUT_TYPE_EMAIL, TEXT_INPUT_MODE_NONE,
                           TEXT_INPUT_FLAG_AUTOCORRECT_ON, false, false);
  VerifyAndClearExpectations();

  // Multiple pending requests should be sent when commit number finally
  // matches.
  PostToServerAndWait([kRect,
                       kSurroundingText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input,
                SetContentType(ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK,
                               ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL))
        .Times(1);
    EXPECT_CALL(
        *server->text_input_manager_v3()->text_input(),
        SetCursorRect(kRect.x(), kRect.y(), kRect.width(), kRect.height()));
    EXPECT_CALL(*zwp_text_input,
                SetSurroundingText(kSurroundingText, gfx::Range{11, 11}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    // Commit has been called twice. So done serial 2 should match.
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 2);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZWPTextInputWrapperV3Test, PendingRequestsClearedOnEnable) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kPreeditRange(11, 38);
  constexpr gfx::Range kSelectionRange(38, 38);

  // Trigger 1 commit by calling activate.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->Activate(window_.get(), ui::TextInputClient::FOCUS_REASON_NONE);
  VerifyAndClearExpectations();

  // Pending set requests should not be sent without matching done event.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
  });
  wrapper_->SetSurroundingText(text, kPreeditRange, kSelectionRange);
  wrapper_->SetCursorRect(kRect);
  wrapper_->SetContentType(TEXT_INPUT_TYPE_EMAIL, TEXT_INPUT_MODE_NONE,
                           TEXT_INPUT_FLAG_AUTOCORRECT_ON, false, false);
  VerifyAndClearExpectations();

  // Enable should clear pending requests.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->Activate(window_.get(), ui::TextInputClient::FOCUS_REASON_NONE);
  VerifyAndClearExpectations();

  // Since there are no more pending requests nothing should be sent even if
  // done serial matches.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 2);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZWPTextInputWrapperV3Test, PendingRequestsClearedOnDisable) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kPreeditRange(11, 38);
  constexpr gfx::Range kSelectionRange(38, 38);

  // Trigger 1 commit by calling activate.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->Activate(window_.get(), ui::TextInputClient::FOCUS_REASON_NONE);
  VerifyAndClearExpectations();

  // Pending set requests should not be sent without matching done event.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
  });
  wrapper_->SetSurroundingText(text, kPreeditRange, kSelectionRange);
  wrapper_->SetCursorRect(kRect);
  wrapper_->SetContentType(TEXT_INPUT_TYPE_EMAIL, TEXT_INPUT_MODE_NONE,
                           TEXT_INPUT_FLAG_AUTOCORRECT_ON, false, false);
  VerifyAndClearExpectations();

  // Disable should clear pending requests.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Disable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->Deactivate();
  VerifyAndClearExpectations();

  // Since there are no more pending requests nothing should be sent even if
  // done serial matches.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 2);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZWPTextInputWrapperV3Test, PendingRequestsClearedOnReset) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kPreeditRange(11, 38);
  constexpr gfx::Range kSelectionRange(38, 38);

  // Trigger 1 commit by calling activate
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->Activate(window_.get(), ui::TextInputClient::FOCUS_REASON_NONE);

  // Pending set requests should not be sent without matching done event.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
  });
  wrapper_->SetSurroundingText(text, kPreeditRange, kSelectionRange);
  wrapper_->SetCursorRect(kRect);
  wrapper_->SetContentType(TEXT_INPUT_TYPE_EMAIL, TEXT_INPUT_MODE_NONE,
                           TEXT_INPUT_FLAG_AUTOCORRECT_ON, false, false);
  VerifyAndClearExpectations();

  // Reset should clear pending requests.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Disable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    EXPECT_CALL(*zwp_text_input, Enable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->Reset();
  VerifyAndClearExpectations();

  // Since there are no more pending requests nothing should be sent even if
  // done serial matches.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 2);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 3);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZWPTextInputWrapperV3Test, OnPreeditString) {
  constexpr std::string_view kPreeditString("PreeditString");
  constexpr gfx::Range kPreeditCursor{0, 13};
  EXPECT_CALL(
      test_client_,
      OnPreeditString(kPreeditString,
                      std::vector<ZWPTextInputWrapperClient::SpanStyle>{},
                      kPreeditCursor));
  PostToServerAndWait(
      [kPreeditString, kPreeditCursor](wl::TestWaylandServerThread* server) {
        auto* text_input = server->text_input_manager_v3()->text_input();
        zwp_text_input_v3_send_preedit_string(
            text_input->resource(), kPreeditString.data(),
            kPreeditCursor.start(), kPreeditCursor.end());
        zwp_text_input_v3_send_done(text_input->resource(), 0);
      });
  VerifyAndClearExpectations();

  // Invalid range if negative cursor begin
  EXPECT_CALL(
      test_client_,
      OnPreeditString(kPreeditString,
                      std::vector<ZWPTextInputWrapperClient::SpanStyle>{},
                      gfx::Range::InvalidRange()));
  PostToServerAndWait(
      [kPreeditString, kPreeditCursor](wl::TestWaylandServerThread* server) {
        auto* text_input = server->text_input_manager_v3()->text_input();
        zwp_text_input_v3_send_preedit_string(text_input->resource(),
                                              kPreeditString.data(), -1,
                                              kPreeditCursor.end());
        zwp_text_input_v3_send_done(text_input->resource(), 0);
      });
  VerifyAndClearExpectations();

  // Invalid range if negative cursor end
  EXPECT_CALL(
      test_client_,
      OnPreeditString(kPreeditString,
                      std::vector<ZWPTextInputWrapperClient::SpanStyle>{},
                      gfx::Range::InvalidRange()));
  PostToServerAndWait(
      [kPreeditString, kPreeditCursor](wl::TestWaylandServerThread* server) {
        auto* text_input = server->text_input_manager_v3()->text_input();
        zwp_text_input_v3_send_preedit_string(text_input->resource(),
                                              kPreeditString.data(),
                                              kPreeditCursor.start(), -1);
        zwp_text_input_v3_send_done(text_input->resource(), 0);
      });
  VerifyAndClearExpectations();
}

TEST_F(ZWPTextInputWrapperV3Test, OnCommitString) {
  constexpr std::string kCommitString("CommitString");
  EXPECT_CALL(test_client_, OnCommitString(kCommitString));
  PostToServerAndWait([kCommitString](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_commit_string(text_input->resource(),
                                         kCommitString.c_str());
    zwp_text_input_v3_send_done(text_input->resource(), 0);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZWPTextInputWrapperV3Test, OnDoneWithCommitAndPreedit) {
  constexpr std::string kPreeditString("PreeditString");
  constexpr gfx::Range kPreeditCursor{0, 13};
  constexpr std::string kCommitString("CommitString");
  InSequence s;
  EXPECT_CALL(test_client_, OnCommitString(kCommitString));
  EXPECT_CALL(
      test_client_,
      OnPreeditString(kPreeditString,
                      std::vector<ZWPTextInputWrapperClient::SpanStyle>{},
                      kPreeditCursor));
  PostToServerAndWait([kPreeditString, kPreeditCursor,
                       kCommitString](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_preedit_string(
        text_input->resource(), kPreeditString.c_str(), kPreeditCursor.start(),
        kPreeditCursor.end());
    zwp_text_input_v3_send_commit_string(text_input->resource(),
                                         kCommitString.c_str());
    zwp_text_input_v3_send_done(text_input->resource(), 0);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZWPTextInputWrapperV3Test, PendingInputEventsClearedOnEnable) {
  constexpr std::string kCommitString("CommitString");
  constexpr std::string_view kPreeditString("PreeditString");
  constexpr gfx::Range kPreeditCursor{0, 13};
  PostToServerAndWait([kPreeditString, kPreeditCursor,
                       kCommitString](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_commit_string(text_input->resource(),
                                         kCommitString.c_str());
    zwp_text_input_v3_send_preedit_string(
        text_input->resource(), kPreeditString.data(), kPreeditCursor.start(),
        kPreeditCursor.end());
  });

  // Enable should clear pending requests.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->Activate(window_.get(), ui::TextInputClient::FOCUS_REASON_NONE);
  VerifyAndClearExpectations();

  // Sending done should have no effect.
  EXPECT_CALL(test_client_, OnCommitString(_)).Times(0);
  EXPECT_CALL(test_client_, OnPreeditString(_, _, _)).Times(0);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZWPTextInputWrapperV3Test, PendingInputEventsClearedOnDisable) {
  constexpr std::string_view kPreeditString("PreeditString");
  constexpr gfx::Range kPreeditCursor{0, 13};
  PostToServerAndWait(
      [kPreeditString, kPreeditCursor](wl::TestWaylandServerThread* server) {
        auto* text_input = server->text_input_manager_v3()->text_input();
        zwp_text_input_v3_send_preedit_string(
            text_input->resource(), kPreeditString.data(),
            kPreeditCursor.start(), kPreeditCursor.end());
      });

  // Disable should clear pending requests.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Disable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->Deactivate();
  VerifyAndClearExpectations();

  // Sending done should have no effect.
  EXPECT_CALL(test_client_, OnPreeditString(_, _, _)).Times(0);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZWPTextInputWrapperV3Test, PendingInputEventsClearedOnReset) {
  constexpr std::string kCommitString("CommitString");
  constexpr std::string_view kPreeditString("PreeditString");
  constexpr gfx::Range kPreeditCursor{0, 13};
  PostToServerAndWait([kPreeditString, kPreeditCursor,
                       kCommitString](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_commit_string(text_input->resource(),
                                         kCommitString.c_str());
    zwp_text_input_v3_send_preedit_string(
        text_input->resource(), kPreeditString.data(), kPreeditCursor.start(),
        kPreeditCursor.end());
  });

  // Reset should clear pending requests.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Disable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    EXPECT_CALL(*zwp_text_input, Enable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  wrapper_->Reset();
  VerifyAndClearExpectations();

  // Sending done should have no effect.
  EXPECT_CALL(test_client_, OnCommitString(_)).Times(0);
  EXPECT_CALL(test_client_, OnPreeditString(_, _, _)).Times(0);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_done(text_input->resource(), 1);
    zwp_text_input_v3_send_done(text_input->resource(), 2);
  });
  VerifyAndClearExpectations();
}

}  // namespace ui
