// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_text_input_v3.h"

#include <text-input-unstable-v3-client-protocol.h>
#include <text-input-unstable-v3-server-protocol.h>

#include <memory>
#include <string_view>

#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/surrounding_text_tracker.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/host/span_style.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_text_input.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_text_input_v3_client.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;

namespace ui {

class ZwpTextInputV3Test : public WaylandTestSimple {
 public:
  ZwpTextInputV3Test()
      : WaylandTestSimple({.text_input_type = wl::ZwpTextInputType::kV3}) {}

  void SetUp() override {
    WaylandTestSimple::SetUp();

    text_input_v3_ = connection_->EnsureTextInputV3();
    text_input_v3_->SetClient(&test_client_);
  }

 protected:
  void VerifyAndClearExpectations() {
    Mock::VerifyAndClearExpectations(&test_client_);
  }

  MockZwpTextInputV3Client test_client_;
  raw_ptr<ZwpTextInputV3> text_input_v3_;
};

TEST_F(ZwpTextInputV3Test, Enable) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Enable())
        .Times(1);
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Commit())
        .Times(1);
  });
  text_input_v3_->Enable();
}

TEST_F(ZwpTextInputV3Test, Disable) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Disable())
        .Times(1);
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Commit())
        .Times(1);
  });
  text_input_v3_->Disable();
}

TEST_F(ZwpTextInputV3Test, Reset) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Disable())
        .Times(0);
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Enable())
        .Times(0);
    EXPECT_CALL(*server->text_input_manager_v3()->text_input(), Commit())
        .Times(0);
  });
  text_input_v3_->Reset();
}

TEST_F(ZwpTextInputV3Test, SetContentType) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(
        *zwp_text_input,
        SetContentType(ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK |
                           ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA,
                       ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->SetContentType(TEXT_INPUT_TYPE_EMAIL,
                                 TEXT_INPUT_FLAG_AUTOCORRECT_ON, false);
  VerifyAndClearExpectations();

  // Calling again with the same values should be a no-op.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
    // Commit has been called once. So send done serial matching commit.
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
  });
  text_input_v3_->SetContentType(TEXT_INPUT_TYPE_EMAIL,
                                 TEXT_INPUT_FLAG_AUTOCORRECT_ON, false);
  VerifyAndClearExpectations();

  // Calling with different values should work.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(
        *zwp_text_input,
        SetContentType(ZWP_TEXT_INPUT_V3_CONTENT_HINT_AUTO_CAPITALIZATION |
                           ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA,
                       ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NUMBER))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->SetContentType(TEXT_INPUT_TYPE_NUMBER,
                                 TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS, false);
}

TEST_F(ZwpTextInputV3Test, SetCursorRect) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  PostToServerAndWait([kRect](wl::TestWaylandServerThread* server) {
    InSequence s;
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(kRect.x(), kRect.y(),
                                               kRect.width(), kRect.height()));
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->SetCursorRect(kRect);
  VerifyAndClearExpectations();

  // Calling again with the same values should be a no-op.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
    // Commit has been called once. So send done serial matching commit.
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
  });
  text_input_v3_->SetCursorRect(kRect);
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
  text_input_v3_->SetCursorRect(kRect2);
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, SetSurroundingText) {
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
  text_input_v3_->SetSurroundingText(text, kPreeditRange, selection_range);
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
  text_input_v3_->SetSurroundingText(text2, preedit_range2, selection_range);
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
  text_input_v3_->SetSurroundingText(text, kPreeditRange, selection_range);
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
  text_input_v3_->SetSurroundingText(text, kPreeditRange, selection_range);
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
  text_input_v3_->SetSurroundingText(text, kPreeditRange, selection_range);
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
  text_input_v3_->SetSurroundingText(text, kPreeditRange, selection_range);
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
  text_input_v3_->SetSurroundingText(text, kPreeditRange, selection_range);
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
  text_input_v3_->SetSurroundingText(text, kPreeditRange, selection_range);
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
  text_input_v3_->SetSurroundingText(text, kPreeditRange, selection_range);
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
  text_input_v3_->SetSurroundingText(text, kPreeditRange, selection_range);
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
  text_input_v3_->SetSurroundingText(
      kSurroundingText, gfx::Range::InvalidRange(), selection_range);
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
  text_input_v3_->SetSurroundingText(text, kPreeditRange,
                                     gfx::Range::InvalidRange());
  VerifyAndClearExpectations();

  // invalid preedit and selection
  PostToServerAndWait([text](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(text, gfx::Range{42, 42}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 11);
  });
  text_input_v3_->SetSurroundingText(text, gfx::Range::InvalidRange(),
                                     gfx::Range::InvalidRange());
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, PendingRequestsSentOnDone) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  const std::string text("surroundingすしはおいしいですtext");
  constexpr std::string kSurroundingText("surroundingtext");
  constexpr gfx::Range kPreeditRange(11, 38);
  constexpr gfx::Range kSelectionRange(38, 38);

  // Trigger 2 commits by calling activate.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable());
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->Enable();
  VerifyAndClearExpectations();
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable());
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->Enable();
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
  text_input_v3_->SetSurroundingText(text, kPreeditRange, kSelectionRange);
  text_input_v3_->SetCursorRect(kRect);
  text_input_v3_->SetContentType(TEXT_INPUT_TYPE_EMAIL,
                                 TEXT_INPUT_FLAG_AUTOCORRECT_ON, true);
  VerifyAndClearExpectations();

  // Multiple pending requests should be sent when commit number finally
  // matches.
  PostToServerAndWait(
      [kRect, kSurroundingText](wl::TestWaylandServerThread* server) {
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

TEST_F(ZwpTextInputV3Test, PendingRequestsClearedOnEnable) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kPreeditRange(11, 38);
  constexpr gfx::Range kSelectionRange(38, 38);

  // Trigger 1 commit by calling activate.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable());
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->Enable();
  VerifyAndClearExpectations();

  // Pending set requests should not be sent without matching done event.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
  });
  text_input_v3_->SetSurroundingText(text, kPreeditRange, kSelectionRange);
  text_input_v3_->SetCursorRect(kRect);
  text_input_v3_->SetContentType(TEXT_INPUT_TYPE_EMAIL,
                                 TEXT_INPUT_FLAG_AUTOCORRECT_ON, true);
  VerifyAndClearExpectations();

  // Enable should clear pending requests.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable());
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->Enable();
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

TEST_F(ZwpTextInputV3Test, PendingRequestsClearedOnDisable) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kPreeditRange(11, 38);
  constexpr gfx::Range kSelectionRange(38, 38);

  // Trigger 1 commit by calling activate.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable());
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->Enable();
  VerifyAndClearExpectations();

  // Pending set requests should not be sent without matching done event.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
  });
  text_input_v3_->SetSurroundingText(text, kPreeditRange, kSelectionRange);
  text_input_v3_->SetCursorRect(kRect);
  text_input_v3_->SetContentType(TEXT_INPUT_TYPE_EMAIL,
                                 TEXT_INPUT_FLAG_AUTOCORRECT_ON, true);
  VerifyAndClearExpectations();

  // Disable should clear pending requests.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Disable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->Disable();
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

TEST_F(ZwpTextInputV3Test, PendingRequestsClearedOnReset) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kPreeditRange(11, 38);
  constexpr gfx::Range kSelectionRange(38, 38);

  // Trigger 1 commit by calling activate
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable());
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->Enable();

  // Pending set requests should not be sent without matching done event.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
  });
  text_input_v3_->SetSurroundingText(text, kPreeditRange, kSelectionRange);
  text_input_v3_->SetCursorRect(kRect);
  text_input_v3_->SetContentType(TEXT_INPUT_TYPE_EMAIL,
                                 TEXT_INPUT_FLAG_AUTOCORRECT_ON, true);
  VerifyAndClearExpectations();

  // Reset should clear pending requests.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Disable()).Times(0);
    EXPECT_CALL(*zwp_text_input, Enable()).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
  });
  text_input_v3_->Reset();
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

TEST_F(ZwpTextInputV3Test, OnPreeditString) {
  constexpr std::string_view kPreeditString("PreeditString");
  constexpr gfx::Range kPreeditCursor{0, 13};
  EXPECT_CALL(test_client_,
              OnPreeditString(kPreeditString, std::vector<SpanStyle>{},
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
  EXPECT_CALL(test_client_,
              OnPreeditString(kPreeditString, std::vector<SpanStyle>{},
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
  EXPECT_CALL(test_client_,
              OnPreeditString(kPreeditString, std::vector<SpanStyle>{},
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

TEST_F(ZwpTextInputV3Test, OnCommitString) {
  constexpr std::string kCommitString("CommitString");
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(_, _)).Times(0);
  EXPECT_CALL(test_client_, OnCommitString(kCommitString));
  PostToServerAndWait([kCommitString](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_commit_string(text_input->resource(),
                                         kCommitString.c_str());
    zwp_text_input_v3_send_done(text_input->resource(), 0);
  });
  VerifyAndClearExpectations();
}

// When only selection is set, delete surrounding text around it.
TEST_F(ZwpTextInputV3Test, OnDeleteSurroundingTextAroundSelection) {
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kSelectionRange = {11, 38};
  text_input_v3_->SetSurroundingText(text, gfx::Range::InvalidRange(),
                                     kSelectionRange);
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(10, 29));
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 1,
                                                   1);
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });
  VerifyAndClearExpectations();
}

// When preedit and selection are both set, delete surrounding text around
// preedit.
TEST_F(ZwpTextInputV3Test, OnDeleteSurroundingTextAroundPreedit) {
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kPreeditRange = {11, 38};
  constexpr gfx::Range kSelectionRange = {38, 38};
  text_input_v3_->SetSurroundingText(text, kPreeditRange, kSelectionRange);
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(10, 29));
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 1,
                                                   1);
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });
  VerifyAndClearExpectations();
}

// If neither preedit and selection range are set surrounding text should
// not be deleted.
TEST_F(ZwpTextInputV3Test,
       OnDeleteSurroundingTextIgnoredWithoutPreeditAndSelection) {
  const std::string text("surroundingすしはおいしいですtext");
  text_input_v3_->SetSurroundingText(text, gfx::Range::InvalidRange(),
                                     gfx::Range::InvalidRange());
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(_, _)).Times(0);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 1,
                                                   1);
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, OnDeleteSurroundingTextNegativeIndex) {
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kSelectionRange = {11, 38};
  text_input_v3_->SetSurroundingText(text, gfx::Range::InvalidRange(),
                                     kSelectionRange);
  // Ensure the minimum index is 0 and the length is calculated from that.
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(0, 39));
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    // Force a negative index
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 100,
                                                   1);
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, OnDeleteSurroundingTextLengthMorethanText) {
  // Text length is 42.
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kSelectionRange = {11, 38};
  text_input_v3_->SetSurroundingText(text, gfx::Range::InvalidRange(),
                                     kSelectionRange);
  // Ensure the deletion length from index doesn't exceed text length.
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(10, 32));
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    // Force a negative index
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 1,
                                                   20);
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, OnDoneWithCommitAndPreedit) {
  constexpr std::string kPreeditString("PreeditString");
  constexpr gfx::Range kPreeditCursor{0, 13};
  constexpr std::string kCommitString("CommitString");
  InSequence s;
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(_, _)).Times(0);
  EXPECT_CALL(test_client_, OnCommitString(kCommitString));
  EXPECT_CALL(test_client_,
              OnPreeditString(kPreeditString, std::vector<SpanStyle>{},
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

  // Sending done again should report empty preedit.
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(_, _)).Times(0);
  EXPECT_CALL(test_client_, OnCommitString(_)).Times(0);
  EXPECT_CALL(test_client_,
              OnPreeditString("", std::vector<SpanStyle>{}, gfx::Range()));
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, OnDoneWithDeleteSurroundingAndCommit) {
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kSelectionRange = {11, 38};
  text_input_v3_->SetSurroundingText(text, gfx::Range::InvalidRange(),
                                     kSelectionRange);
  constexpr std::string kCommitString("CommitString");

  InSequence s;
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(10, 29));
  EXPECT_CALL(test_client_, OnCommitString(kCommitString));
  EXPECT_CALL(test_client_, OnPreeditString(_, _, _)).Times(0);
  PostToServerAndWait([kCommitString](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 1,
                                                   1);
    zwp_text_input_v3_send_commit_string(text_input->resource(),
                                         kCommitString.c_str());
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, OnDoneWithDeleteSurroundingAndPreedit) {
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kSelectionRange = {11, 38};
  text_input_v3_->SetSurroundingText(text, gfx::Range::InvalidRange(),
                                     kSelectionRange);
  constexpr std::string kPreeditString("PreeditString");
  constexpr gfx::Range kPreeditCursor{0, 13};

  InSequence s;
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(10, 29));
  EXPECT_CALL(test_client_, OnCommitString(_)).Times(0);
  EXPECT_CALL(test_client_,
              OnPreeditString(kPreeditString, std::vector<SpanStyle>{},
                              kPreeditCursor));
  PostToServerAndWait(
      [kPreeditString, kPreeditCursor](wl::TestWaylandServerThread* server) {
        auto* text_input = server->text_input_manager_v3()->text_input();
        zwp_text_input_v3_send_preedit_string(
            text_input->resource(), kPreeditString.c_str(),
            kPreeditCursor.start(), kPreeditCursor.end());
        zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(),
                                                       1, 1);
        zwp_text_input_v3_send_done(text_input->resource(), 1);
      });
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, OnDoneWithDeleteSurroundingCommitAndPreedit) {
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kSelectionRange = {11, 38};
  text_input_v3_->SetSurroundingText(text, gfx::Range::InvalidRange(),
                                     kSelectionRange);
  constexpr std::string kCommitString("CommitString");
  constexpr std::string kPreeditString("PreeditString");
  constexpr gfx::Range kPreeditCursor{0, 13};

  InSequence s;
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(10, 29));
  EXPECT_CALL(test_client_, OnCommitString(kCommitString));
  EXPECT_CALL(test_client_,
              OnPreeditString(kPreeditString, std::vector<SpanStyle>{},
                              kPreeditCursor));
  PostToServerAndWait([kPreeditString, kPreeditCursor,
                       kCommitString](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_preedit_string(
        text_input->resource(), kPreeditString.c_str(), kPreeditCursor.start(),
        kPreeditCursor.end());
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 1,
                                                   1);
    zwp_text_input_v3_send_commit_string(text_input->resource(),
                                         kCommitString.c_str());
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, PendingInputEventsClearedOnEnable) {
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
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 1,
                                                   1);
  });

  // Enable should clear pending requests.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable());
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->Enable();
  VerifyAndClearExpectations();

  // Sending done should have no effect.
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(_, _)).Times(0);
  EXPECT_CALL(test_client_, OnCommitString(_)).Times(0);
  EXPECT_CALL(test_client_, OnPreeditString(_, _, _)).Times(0);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, PendingInputEventsClearedOnDisable) {
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
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 1,
                                                   1);
  });

  // Disable should clear pending requests.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Disable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->Disable();
  VerifyAndClearExpectations();

  // Sending done should have no effect.
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(_, _)).Times(0);
  EXPECT_CALL(test_client_, OnCommitString(_)).Times(0);
  EXPECT_CALL(test_client_, OnPreeditString(_, _, _)).Times(0);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, PendingInputEventsClearedOnReset) {
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
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 1,
                                                   1);
  });

  // Reset should clear pending requests.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Disable()).Times(0);
    EXPECT_CALL(*zwp_text_input, Enable()).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
  });
  text_input_v3_->Reset();
  VerifyAndClearExpectations();

  // Sending done should have no effect.
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(_, _)).Times(0);
  EXPECT_CALL(test_client_, OnCommitString(_)).Times(0);
  EXPECT_CALL(test_client_, OnPreeditString(_, _, _)).Times(0);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_done(text_input->resource(), 1);
    zwp_text_input_v3_send_done(text_input->resource(), 2);
  });
  VerifyAndClearExpectations();
}

class ZwpTextInputV3TestWithCustomClient : public WaylandTestSimple {
 public:
  ZwpTextInputV3TestWithCustomClient()
      : WaylandTestSimple({.text_input_type = wl::ZwpTextInputType::kV3}) {}

  void SetUp() override {
    WaylandTestSimple::SetUp();

    text_input_v3_ = connection_->EnsureTextInputV3();
    text_input_v3_->SetClient(&test_client_);
  }

 protected:
  // Somewhat based on real impl.
  class TestClient : public ZwpTextInputV3Client {
   public:
    TestClient() = default;
    ~TestClient() override = default;
    void OnPreeditString(std::string_view text,
                         const std::vector<SpanStyle>& spans,
                         const gfx::Range& preedit_cursor) override {
      CompositionText composition_text;
      composition_text.text = base::UTF8ToUTF16(text);
      std::vector<size_t> offsets = {
          static_cast<uint32_t>(preedit_cursor.start()),
          static_cast<uint32_t>(preedit_cursor.end())};
      base::UTF8ToUTF16AndAdjustOffsets(text, &offsets);
      if (offsets[0] == std::u16string::npos ||
          offsets[1] == std::u16string::npos) {
        DVLOG(1) << "got invalid cursor position (byte offset)="
                 << preedit_cursor.start() << "-" << preedit_cursor.end();
        // Invalid cursor position. Do nothing.
        return;
      }
      composition_text.selection = gfx::Range(offsets[0], offsets[1]);
      surrounding_text_tracker_.OnSetCompositionText(composition_text);
      RecordState();
    }
    void OnCommitString(std::string_view text) override {
      std::u16string text_utf16 = base::UTF8ToUTF16(text);
      surrounding_text_tracker_.OnInsertText(
          text_utf16,
          TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
      RecordState();
    }
    void OnDeleteSurroundingText(int32_t index, uint32_t length) override {
      const auto& [surrounding_text, utf16_offset, selection,
                   unsused_composition] =
          surrounding_text_tracker_.predicted_state();
      DCHECK(selection.IsValid());

      std::vector<size_t> offsets_for_adjustment = {
          static_cast<size_t>(index),
          static_cast<size_t>(index) + length,
      };
      base::UTF8ToUTF16AndAdjustOffsets(base::UTF16ToUTF8(surrounding_text),
                                        &offsets_for_adjustment);
      if (base::Contains(offsets_for_adjustment, std::u16string::npos)) {
        LOG(DFATAL) << "The selection range for surrounding text is invalid.";
        return;
      }

      if (selection.GetMin() < offsets_for_adjustment[0] + utf16_offset ||
          selection.GetMax() > offsets_for_adjustment[1] + utf16_offset) {
        // The range is started after the selection, or ended before the
        // selection, which is not supported.
        LOG(DFATAL)
            << "The deletion range needs to cover whole selection range.";
        return;
      }

      // Move by offset calculated in SetSurroundingText to adjust to the
      // original text place.
      size_t before =
          selection.GetMin() - offsets_for_adjustment[0] - utf16_offset;
      size_t after =
          offsets_for_adjustment[1] + utf16_offset - selection.GetMax();

      surrounding_text_tracker_.OnExtendSelectionAndDelete(before, after);
      RecordState();
    }

    SurroundingTextTracker& tracker() { return surrounding_text_tracker_; }

    void ExpectState(std::u16string surrounding_text,
                     gfx::Range selection,
                     gfx::Range composition) {
      expected_states_.push_back({.surrounding_text = surrounding_text,
                                  .selection = selection,
                                  .composition = composition});
    }

    void VerifyStateUpdates() {
      size_t len = recorded_states_.size();
      size_t len_exp = expected_states_.size();
      EXPECT_EQ(len_exp, len);
      for (size_t i = 0; i < len; i++) {
        EXPECT_EQ(expected_states_[i].surrounding_text,
                  recorded_states_[i].surrounding_text);
        EXPECT_EQ(expected_states_[i].selection, recorded_states_[i].selection);
        EXPECT_EQ(expected_states_[i].composition,
                  recorded_states_[i].composition);
      }
      recorded_states_.clear();
      expected_states_.clear();
    }

   private:
    void RecordState() {
      recorded_states_.push_back(surrounding_text_tracker_.predicted_state());
    }
    SurroundingTextTracker surrounding_text_tracker_;
    std::vector<SurroundingTextTracker::State> recorded_states_;
    std::vector<SurroundingTextTracker::State> expected_states_;
  };

  TestClient test_client_;
  raw_ptr<ZwpTextInputV3> text_input_v3_;
};

TEST_F(ZwpTextInputV3TestWithCustomClient,
       ConsecutiveOnDeleteSurroundingTextWithoutSetSurroundingText) {
  const std::string text("abcdefghi");
  // Initially select 'ef'.
  constexpr gfx::Range kInitialSelection(4, 6);
  test_client_.tracker().Update(base::UTF8ToUTF16(text), 0, kInitialSelection);
  EXPECT_EQ(test_client_.tracker().predicted_state().selection,
            kInitialSelection);
  text_input_v3_->SetSurroundingText(text, gfx::Range::InvalidRange(),
                                     kInitialSelection);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    // Delete 'def'
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 1,
                                                   0);
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });
  EXPECT_EQ(test_client_.tracker().predicted_state().surrounding_text,
            u"abcghi");
  // Cursor between 'c' and 'g'.
  EXPECT_EQ(test_client_.tracker().predicted_state().selection, gfx::Range(3));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    // Delete 'cgh'
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 1,
                                                   2);
    zwp_text_input_v3_send_done(text_input->resource(), 2);
  });
  EXPECT_EQ(test_client_.tracker().predicted_state().surrounding_text, u"abi");
  // Cursor between 'b' and 'i'.
  EXPECT_EQ(test_client_.tracker().predicted_state().selection, gfx::Range(2));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    // Delete 'abi'
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 2,
                                                   1);
    zwp_text_input_v3_send_done(text_input->resource(), 3);
  });
  EXPECT_EQ(test_client_.tracker().predicted_state().surrounding_text, u"");
  EXPECT_EQ(test_client_.tracker().predicted_state().selection, gfx::Range(0));
}

TEST_F(ZwpTextInputV3TestWithCustomClient, DeleteSurroundingCommitAndPreedit) {
  // Add state before preedit. Cursor between c and d.
  test_client_.tracker().Update(u"abcdefgh", 0, gfx::Range(3));

  // This adds P1 preedit between c and d.
  CompositionText composition_text;
  composition_text.text = u"P1";
  composition_text.selection = gfx::Range(2);
  test_client_.tracker().OnSetCompositionText(composition_text);

  const std::string initial_text("abcP1defgh");
  constexpr gfx::Range kInitialPreedit(3, 5);
  constexpr gfx::Range kInitialSelection(5);
  EXPECT_EQ(test_client_.tracker().predicted_state().selection,
            kInitialSelection);
  EXPECT_EQ(test_client_.tracker().predicted_state().composition,
            kInitialPreedit);
  text_input_v3_->SetSurroundingText(initial_text, kInitialPreedit,
                                     kInitialSelection);

  // Send events in random order before sending done.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_preedit_string(text_input->resource(), "P2", 2, 2);
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 1,
                                                   0);
    zwp_text_input_v3_send_commit_string(text_input->resource(), "C1");
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });

  // First existing preedit "P1" should be cleared and replaced with cursor and
  // one extra character to its left, 'c', should be deleted, placing the cursor
  // between 'b' and 'd'.
  test_client_.ExpectState(u"abdefgh", gfx::Range(2), gfx::Range());
  // Then commit string "C1" should be added with cursor at its end.
  test_client_.ExpectState(u"abC1defgh", gfx::Range(4), gfx::Range());
  // Finally new preedit string "P2" should be inserted and the cursor should be
  // moved based on preedit cursor position.
  test_client_.ExpectState(u"abC1P2defgh", gfx::Range(6), gfx::Range(4, 6));

  test_client_.VerifyStateUpdates();
}

}  // namespace ui
