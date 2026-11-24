// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_text_input_v3.h"

#include <text-input-unstable-v3-client-protocol.h>
#include <text-input-unstable-v3-server-protocol.h>

#include <memory>
#include <string_view>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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

  // Flushes client tasks, e.g. the enable posted by Reset(), and then syncs
  // with the server so that the requests they sent are processed.
  void FlushClientTasks() {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {});
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {});
  }

  MockZwpTextInputV3Client test_client_;
  raw_ptr<ZwpTextInputV3> text_input_v3_;
};

void ExpectEnableWithDefaultContentType(
    wl::MockZwpTextInputV3* zwp_text_input) {
  EXPECT_CALL(*zwp_text_input, Enable()).Times(1);
  EXPECT_CALL(*zwp_text_input,
              SetContentType(ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE,
                             ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL))
      .Times(1);
  EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
}

TEST_F(ZwpTextInputV3Test, Enable) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    ExpectEnableWithDefaultContentType(
        server->text_input_manager_v3()->text_input());
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);
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

// Some compositors (e.g. wlroots and Smithay based ones) only send done events
// in response to input method commits, not to text-input commits. Caret moves
// must still reach the input method in that case. See crbug.com/491869953.
TEST_F(ZwpTextInputV3Test, CursorRectSentWithoutDoneAfterEnable) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    ExpectEnableWithDefaultContentType(
        server->text_input_manager_v3()->text_input());
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);
  VerifyAndClearExpectations();

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

// State requests are sent right away, even if the last done serial does not
// match the number of commits. See crbug.com/491869953.
TEST_F(ZwpTextInputV3Test, RequestsSentWithoutMatchingDone) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  const std::string text("surroundingすしはおいしいですtext");
  constexpr std::string kSurroundingText("surroundingtext");
  constexpr gfx::Range kPreeditRange(11, 38);
  constexpr gfx::Range kSelectionRange(38, 38);

  // Trigger 2 commits by calling activate.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    ExpectEnableWithDefaultContentType(zwp_text_input);
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);
  VerifyAndClearExpectations();
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    ExpectEnableWithDefaultContentType(zwp_text_input);
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);
  VerifyAndClearExpectations();

  // Commit has been called twice, so done serial 1 does not match. The
  // requests are still sent.
  PostToServerAndWait(
      [kRect, kSurroundingText](wl::TestWaylandServerThread* server) {
        auto* zwp_text_input = server->text_input_manager_v3()->text_input();
        zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
        InSequence s;
        EXPECT_CALL(*zwp_text_input,
                    SetSurroundingText(kSurroundingText, gfx::Range{11, 11}))
            .Times(1);
        EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
        EXPECT_CALL(
            *zwp_text_input,
            SetCursorRect(kRect.x(), kRect.y(), kRect.width(), kRect.height()))
            .Times(1);
        EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
        EXPECT_CALL(*zwp_text_input,
                    SetContentType(ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK,
                                   ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL))
            .Times(1);
        EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
      });
  text_input_v3_->SetSurroundingText(text, kPreeditRange, kSelectionRange);
  text_input_v3_->SetCursorRect(kRect);
  text_input_v3_->SetContentType(TEXT_INPUT_TYPE_EMAIL,
                                 TEXT_INPUT_FLAG_AUTOCORRECT_ON, true);
  VerifyAndClearExpectations();

  // Nothing is resent when the done serial matches.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 5);
  });
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, LatestStateResentOnEnable) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kPreeditRange(11, 38);
  constexpr gfx::Range kSelectionRange(38, 38);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    ExpectEnableWithDefaultContentType(zwp_text_input);
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);
  VerifyAndClearExpectations();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(1);
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(1);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(3);
  });
  text_input_v3_->SetSurroundingText(text, kPreeditRange, kSelectionRange);
  text_input_v3_->SetCursorRect(kRect);
  text_input_v3_->SetContentType(TEXT_INPUT_TYPE_EMAIL,
                                 TEXT_INPUT_FLAG_AUTOCORRECT_ON, true);
  VerifyAndClearExpectations();

  // The latest surrounding text and cursor rect are sent along with enable
  // since enable resets all state, but the content type is replaced by the
  // one passed to Enable().
  PostToServerAndWait([kRect](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable()).Times(1);
    EXPECT_CALL(*zwp_text_input,
                SetContentType(ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE,
                               ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL))
        .Times(1);
    EXPECT_CALL(*zwp_text_input,
                SetSurroundingText("surroundingtext", gfx::Range{11, 11}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, SetCursorRect(kRect.x(), kRect.y(),
                                               kRect.width(), kRect.height()))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);
  VerifyAndClearExpectations();

  // Nothing more is sent on done events.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 4);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 5);
  });
  VerifyAndClearExpectations();
}

// Regression test for crbug.com/565066842: text-input-v3 state is reset on
// each committed enable, so the surrounding text and cursor rect must be resent
// along with enable after the window regains text input focus.
TEST_F(ZwpTextInputV3Test, LatestStateSentOnReEnable) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  constexpr std::string kText("surroundingtext");
  constexpr gfx::Range kSelectionRange(3, 5);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    ExpectEnableWithDefaultContentType(zwp_text_input);
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);
  VerifyAndClearExpectations();

  PostToServerAndWait([kRect, kText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(kRect.x(), kRect.y(),
                                               kRect.width(), kRect.height()))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(kText, gfx::Range{3, 5}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(2);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
  });
  text_input_v3_->SetCursorRect(kRect);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    zwp_text_input_v3_send_done(
        server->text_input_manager_v3()->text_input()->resource(), 2);
  });
  text_input_v3_->SetSurroundingText(kText, gfx::Range::InvalidRange(),
                                     kSelectionRange);
  VerifyAndClearExpectations();

  // Window loses and then regains text input focus. Both surrounding text and
  // cursor rect should be sent in the same commit as enable.
  PostToServerAndWait([kRect, kText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Disable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    EXPECT_CALL(*zwp_text_input, Enable()).Times(1);
    EXPECT_CALL(*zwp_text_input,
                SetContentType(ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE,
                               ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(kText, gfx::Range{3, 5}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, SetCursorRect(kRect.x(), kRect.y(),
                                               kRect.width(), kRect.height()))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->Disable();
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);
  VerifyAndClearExpectations();

  // Setting the same values again should be a no-op since they were already
  // sent with enable.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 5);
  });
  text_input_v3_->SetCursorRect(kRect);
  text_input_v3_->SetSurroundingText(kText, gfx::Range::InvalidRange(),
                                     kSelectionRange);
  VerifyAndClearExpectations();
}

// Surrounding text requested before the text input is enabled must be sent
// along with enable, as enable invalidates previously sent state.
TEST_F(ZwpTextInputV3Test, SurroundingTextSetBeforeEnableSentOnEnable) {
  constexpr std::string kText("surroundingtext");

  // Sent immediately (while disabled), but will be invalidated by enable.
  PostToServerAndWait([kText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(kText, gfx::Range{15, 15}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->SetSurroundingText(kText, gfx::Range::InvalidRange(),
                                     gfx::Range(15, 15));
  VerifyAndClearExpectations();

  PostToServerAndWait([kText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Enable()).Times(1);
    EXPECT_CALL(*zwp_text_input,
                SetContentType(ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE,
                               ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(kText, gfx::Range{15, 15}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, ClearCachedStateNotSentOnEnable) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(1);
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(2);
  });
  text_input_v3_->SetSurroundingText("text", gfx::Range::InvalidRange(),
                                     gfx::Range(4, 4));
  text_input_v3_->SetCursorRect(kRect);
  VerifyAndClearExpectations();

  text_input_v3_->ClearCachedState();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    ExpectEnableWithDefaultContentType(zwp_text_input);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, NothingSentOnDoneAfterDisable) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kPreeditRange(11, 38);
  constexpr gfx::Range kSelectionRange(38, 38);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    ExpectEnableWithDefaultContentType(zwp_text_input);
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);
  VerifyAndClearExpectations();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(1);
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(1);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(3);
  });
  text_input_v3_->SetSurroundingText(text, kPreeditRange, kSelectionRange);
  text_input_v3_->SetCursorRect(kRect);
  text_input_v3_->SetContentType(TEXT_INPUT_TYPE_EMAIL,
                                 TEXT_INPUT_FLAG_AUTOCORRECT_ON, true);
  VerifyAndClearExpectations();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Disable()).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->Disable();
  VerifyAndClearExpectations();

  // Nothing should be sent on done events after disabling.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 4);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 5);
  });
  VerifyAndClearExpectations();
}

// Reset clears the last sent state, so the same values are sent again after
// it. See crbug.com/353915732.
TEST_F(ZwpTextInputV3Test, StateResentAfterReset) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kPreeditRange(11, 38);
  constexpr gfx::Range kSelectionRange(38, 38);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    ExpectEnableWithDefaultContentType(zwp_text_input);
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);
  VerifyAndClearExpectations();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(1);
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(1);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(3);
  });
  text_input_v3_->SetSurroundingText(text, kPreeditRange, kSelectionRange);
  text_input_v3_->SetCursorRect(kRect);
  text_input_v3_->SetContentType(TEXT_INPUT_TYPE_EMAIL,
                                 TEXT_INPUT_FLAG_AUTOCORRECT_ON, true);
  VerifyAndClearExpectations();

  // Reset should not send anything by itself, even on done events.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, Disable()).Times(0);
    EXPECT_CALL(*zwp_text_input, Enable()).Times(0);
    EXPECT_CALL(*zwp_text_input, SetCursorRect(_, _, _, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetContentType(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
  });
  text_input_v3_->Reset();
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    zwp_text_input_v3_send_done(
        server->text_input_manager_v3()->text_input()->resource(), 4);
  });
  VerifyAndClearExpectations();

  // The same values are sent again after reset.
  PostToServerAndWait([kRect](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input,
                SetSurroundingText("surroundingtext", gfx::Range{11, 11}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    EXPECT_CALL(*zwp_text_input, SetCursorRect(kRect.x(), kRect.y(),
                                               kRect.width(), kRect.height()))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->SetSurroundingText(text, kPreeditRange, kSelectionRange);
  text_input_v3_->SetCursorRect(kRect);
  VerifyAndClearExpectations();
}

// Regression test for crbug.com/565605611: moving the cursor in the omnibox
// sends the cursor rect, then the surrounding text, then resets the input
// method context before any done event arrives. The surrounding text must
// still reach the compositor.
TEST_F(ZwpTextInputV3Test, SurroundingTextSetBeforeResetIsSent) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  constexpr gfx::Rect kNewRect(40, 20, 1, 1);
  constexpr std::string kText("abcd");

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    ExpectEnableWithDefaultContentType(zwp_text_input);
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    zwp_text_input_v3_send_done(
        server->text_input_manager_v3()->text_input()->resource(), 1);
  });
  VerifyAndClearExpectations();

  // Typing "abcd" sends the cursor rect and the surrounding text.
  PostToServerAndWait([kRect, kText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, SetCursorRect(kRect.x(), kRect.y(),
                                               kRect.width(), kRect.height()))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(kText, gfx::Range{4, 4}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->SetCursorRect(kRect);
  text_input_v3_->SetSurroundingText(kText, gfx::Range::InvalidRange(),
                                     gfx::Range(4, 4));
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    zwp_text_input_v3_send_done(
        server->text_input_manager_v3()->text_input()->resource(), 2);
  });
  VerifyAndClearExpectations();

  // Pressing Left moves the cursor, which sends the cursor rect and the
  // surrounding text. The context is then reset.
  PostToServerAndWait([kNewRect, kText](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input,
                SetCursorRect(kNewRect.x(), kNewRect.y(), kNewRect.width(),
                              kNewRect.height()))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(kText, gfx::Range{3, 3}))
        .Times(1);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(1);
  });
  text_input_v3_->SetCursorRect(kNewRect);
  text_input_v3_->SetSurroundingText(kText, gfx::Range::InvalidRange(),
                                     gfx::Range(3, 3));
  text_input_v3_->Reset();
  VerifyAndClearExpectations();

  // Nothing is left to send when the done event arrives.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 4);
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

// The deletion lengths are forwarded to the client, which deletes around the
// selection.
TEST_F(ZwpTextInputV3Test, OnDeleteSurroundingTextAroundSelection) {
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kSelectionRange = {11, 38};
  text_input_v3_->SetSurroundingText(text, gfx::Range::InvalidRange(),
                                     kSelectionRange);
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(1, 1));
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 1,
                                                   1);
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });
  VerifyAndClearExpectations();
}

// The deletion lengths are forwarded to the client, which deletes around the
// preedit.
TEST_F(ZwpTextInputV3Test, OnDeleteSurroundingTextAroundPreedit) {
  const std::string text("surroundingすしはおいしいですtext");
  constexpr gfx::Range kPreeditRange = {11, 38};
  constexpr gfx::Range kSelectionRange = {38, 38};
  text_input_v3_->SetSurroundingText(text, kPreeditRange, kSelectionRange);
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(1, 1));
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_delete_surrounding_text(text_input->resource(), 1,
                                                   1);
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
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(1, 1));
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

TEST_F(ZwpTextInputV3Test, OnInputEventsAppliedOnDone) {
  // The client has not sent surrounding text, so it is not asked to update it.
  {
    InSequence s;
    EXPECT_CALL(test_client_, OnCommitString("a"));
    EXPECT_CALL(test_client_, OnInputEventsApplied()).Times(0);
  }
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_commit_string(text_input->resource(), "a");
    zwp_text_input_v3_send_done(text_input->resource(), 0);
  });
  VerifyAndClearExpectations();

  text_input_v3_->SetSurroundingText("a", gfx::Range::InvalidRange(),
                                     gfx::Range(1));
  {
    InSequence s;
    EXPECT_CALL(test_client_, OnCommitString("b"));
    EXPECT_CALL(test_client_, OnInputEventsApplied());
  }
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_commit_string(text_input->resource(), "b");
    zwp_text_input_v3_send_done(text_input->resource(), 1);
  });
  VerifyAndClearExpectations();

  // No input events were applied.
  EXPECT_CALL(test_client_, OnInputEventsApplied()).Times(0);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v3()->text_input();
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
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(1, 1));
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
  EXPECT_CALL(test_client_, OnDeleteSurroundingText(1, 1));
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
    ExpectEnableWithDefaultContentType(zwp_text_input);
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);
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

// Resetting while the input method has a preedit disables and enables text
// input so that the input method drops the composition instead of committing
// it again later (crbug.com/470397995).
TEST_F(ZwpTextInputV3Test, ResetWithPreeditDisablesAndEnables) {
  constexpr gfx::Rect kRect(50, 20, 1, 1);
  constexpr std::string_view kPreeditString("PreeditString");

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    ExpectEnableWithDefaultContentType(
        server->text_input_manager_v3()->text_input());
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
    InSequence s;
    EXPECT_CALL(*zwp_text_input,
                SetContentType(ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK,
                               ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL));
    EXPECT_CALL(*zwp_text_input, Commit());
  });
  text_input_v3_->SetContentType(TEXT_INPUT_TYPE_EMAIL,
                                 TEXT_INPUT_FLAG_SPELLCHECK_ON, true);
  VerifyAndClearExpectations();

  PostToServerAndWait([kRect](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 2);
    InSequence s;
    EXPECT_CALL(*zwp_text_input, SetCursorRect(kRect.x(), kRect.y(),
                                               kRect.width(), kRect.height()));
    EXPECT_CALL(*zwp_text_input, Commit());
  });
  text_input_v3_->SetCursorRect(kRect);
  VerifyAndClearExpectations();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 3);
    InSequence s;
    EXPECT_CALL(*zwp_text_input, SetSurroundingText("ab", gfx::Range{2, 2}));
    EXPECT_CALL(*zwp_text_input, Commit());
  });
  text_input_v3_->SetSurroundingText("abPreeditString", gfx::Range(2, 15),
                                     gfx::Range(15, 15));
  VerifyAndClearExpectations();

  EXPECT_CALL(test_client_, OnPreeditString(kPreeditString, _, _));
  PostToServerAndWait([kPreeditString](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_preedit_string(zwp_text_input->resource(),
                                          kPreeditString.data(), 0, 0);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 4);
  });
  VerifyAndClearExpectations();

  // Reset() disables right away and enables in a posted task. The latest
  // content type and cursor rect are sent along with the enable request, but
  // not the surrounding text, which is stale after the preedit was dropped.
  PostToServerAndWait([kRect](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Disable());
    EXPECT_CALL(*zwp_text_input, Commit());
    EXPECT_CALL(*zwp_text_input, Enable());
    EXPECT_CALL(*zwp_text_input,
                SetContentType(ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK,
                               ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL));
    EXPECT_CALL(*zwp_text_input, SetCursorRect(kRect.x(), kRect.y(),
                                               kRect.width(), kRect.height()));
    EXPECT_CALL(*zwp_text_input, Commit());
  });
  text_input_v3_->Reset();
  FlushClientTasks();
  VerifyAndClearExpectations();

  // The preedit has been dropped, so another reset sends nothing.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, Disable()).Times(0);
    EXPECT_CALL(*zwp_text_input, Enable()).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
  });
  text_input_v3_->Reset();
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, ResetWithPreeditWhileDisabledSendsNothing) {
  constexpr std::string_view kPreeditString("PreeditString");

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    ExpectEnableWithDefaultContentType(
        server->text_input_manager_v3()->text_input());
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);

  EXPECT_CALL(test_client_, OnPreeditString(kPreeditString, _, _));
  PostToServerAndWait([kPreeditString](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_preedit_string(zwp_text_input->resource(),
                                          kPreeditString.data(), 0, 0);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
  });
  VerifyAndClearExpectations();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Disable());
    EXPECT_CALL(*zwp_text_input, Commit());
  });
  text_input_v3_->Disable();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    Mock::VerifyAndClearExpectations(zwp_text_input);
    EXPECT_CALL(*zwp_text_input, Disable()).Times(0);
    EXPECT_CALL(*zwp_text_input, Enable()).Times(0);
    EXPECT_CALL(*zwp_text_input, Commit()).Times(0);
  });
  text_input_v3_->Reset();
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, ResetWithPreeditThenFocusChangeEnablesOnce) {
  constexpr std::string_view kPreeditString("PreeditString");

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    ExpectEnableWithDefaultContentType(
        server->text_input_manager_v3()->text_input());
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
    InSequence s;
    EXPECT_CALL(*zwp_text_input, SetSurroundingText("ab", gfx::Range{2, 2}));
    EXPECT_CALL(*zwp_text_input, Commit());
  });
  text_input_v3_->SetSurroundingText("ab", gfx::Range::InvalidRange(),
                                     gfx::Range(2, 2));
  VerifyAndClearExpectations();

  EXPECT_CALL(test_client_, OnPreeditString(kPreeditString, _, _));
  PostToServerAndWait([kPreeditString](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_preedit_string(zwp_text_input->resource(),
                                          kPreeditString.data(), 0, 0);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 2);
  });
  VerifyAndClearExpectations();

  // Same sequence as WaylandInputMethodContext::Reset() followed by
  // UpdateFocus() for another text field.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    EXPECT_CALL(*zwp_text_input, SetSurroundingText(_, _)).Times(0);
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Disable());
    EXPECT_CALL(*zwp_text_input, Commit());
    EXPECT_CALL(*zwp_text_input, Enable());
    EXPECT_CALL(*zwp_text_input,
                SetContentType(ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE,
                               ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_URL));
    EXPECT_CALL(*zwp_text_input, Commit());
  });
  text_input_v3_->Reset();
  text_input_v3_->ClearCachedState();
  text_input_v3_->Disable();
  text_input_v3_->Enable(TEXT_INPUT_TYPE_URL, TEXT_INPUT_FLAG_NONE, true);
  FlushClientTasks();
  VerifyAndClearExpectations();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_manager_v3()->text_input());
  });
}

TEST_F(ZwpTextInputV3Test, ShowInputPanelKeepsPreeditForReset) {
  constexpr std::string_view kPreeditString("PreeditString");

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    ExpectEnableWithDefaultContentType(
        server->text_input_manager_v3()->text_input());
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);

  EXPECT_CALL(test_client_, OnPreeditString(kPreeditString, _, _));
  PostToServerAndWait([kPreeditString](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_preedit_string(zwp_text_input->resource(),
                                          kPreeditString.data(), 0, 0);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
  });
  VerifyAndClearExpectations();

  // Showing the input panel enables text input again.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    ExpectEnableWithDefaultContentType(
        server->text_input_manager_v3()->text_input());
  });
  text_input_v3_->ShowInputPanel();
  VerifyAndClearExpectations();

  // The input method still has its preedit, so Reset() disables and enables.
  // Showing the input panel while that enable is pending flushes it and enables
  // once more.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Disable());
    EXPECT_CALL(*zwp_text_input, Commit());
    ExpectEnableWithDefaultContentType(zwp_text_input);
    ExpectEnableWithDefaultContentType(zwp_text_input);
  });
  text_input_v3_->Reset();
  text_input_v3_->ShowInputPanel();
  FlushClientTasks();
  VerifyAndClearExpectations();
}

TEST_F(ZwpTextInputV3Test, ResetWithPreeditThenDisableDisablesOnce) {
  constexpr std::string_view kPreeditString("PreeditString");

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    InSequence s;
    ExpectEnableWithDefaultContentType(
        server->text_input_manager_v3()->text_input());
  });
  text_input_v3_->Enable(TEXT_INPUT_TYPE_TEXT, TEXT_INPUT_FLAG_NONE, true);

  EXPECT_CALL(test_client_, OnPreeditString(kPreeditString, _, _));
  PostToServerAndWait([kPreeditString](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    zwp_text_input_v3_send_preedit_string(zwp_text_input->resource(),
                                          kPreeditString.data(), 0, 0);
    zwp_text_input_v3_send_done(zwp_text_input->resource(), 1);
  });
  VerifyAndClearExpectations();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v3()->text_input();
    Mock::VerifyAndClearExpectations(zwp_text_input);
    EXPECT_CALL(*zwp_text_input, Enable()).Times(0);
    InSequence s;
    EXPECT_CALL(*zwp_text_input, Disable());
    EXPECT_CALL(*zwp_text_input, Commit());
  });
  text_input_v3_->Reset();
  text_input_v3_->Disable();
  FlushClientTasks();
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_manager_v3()->text_input());
  });
}

}  // namespace ui
