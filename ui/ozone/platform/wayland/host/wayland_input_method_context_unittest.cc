// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <text-input-unstable-v1-server-protocol.h>
#include <wayland-server.h>
#include <memory>

#include "base/i18n/break_iterator.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/events/event.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_input_method_context.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_zcr_extended_text_input.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_text_input.h"
#include "ui/ozone/platform/wayland/test/test_util.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::Values;

namespace ui {

// Returns the number of grapheme clusters in the text.
absl::optional<size_t> CountGraphemeCluster(base::StringPiece16 text) {
  base::i18n::BreakIterator iter(text,
                                 base::i18n::BreakIterator::BREAK_CHARACTER);
  if (!iter.Init())
    return absl::nullopt;
  size_t result = 0;
  while (iter.Advance())
    ++result;
  return result;
}

// TODO(crbug.com/1370046): Subclass FakeTextInputClient after pruning deps.
class MockTextInputClient : public TextInputClient {
 public:
  explicit MockTextInputClient(TextInputType text_input_type) {
    text_input_type_ = text_input_type;
  }
  MockTextInputClient(const MockTextInputClient& other) = delete;
  MockTextInputClient& operator=(const MockTextInputClient& other) = delete;
  ~MockTextInputClient() override = default;

  TextInputType GetTextInputType() const override { return text_input_type_; }

  MOCK_METHOD(void,
              SetCompositionText,
              (const ui::CompositionText&),
              (override));
  MOCK_METHOD(size_t, ConfirmCompositionText, (bool), (override));
  MOCK_METHOD(void, ClearCompositionText, (), (override));
  MOCK_METHOD(void,
              InsertText,
              (const std::u16string&,
               ui::TextInputClient::InsertTextCursorBehavior cursor_behavior),
              (override));
  MOCK_METHOD(void, InsertChar, (const ui::KeyEvent&), (override));
  MOCK_METHOD(ui::TextInputMode, GetTextInputMode, (), (const, override));
  MOCK_METHOD(base::i18n::TextDirection,
              GetTextDirection,
              (),
              (const, override));
  MOCK_METHOD(int, GetTextInputFlags, (), (const, override));
  MOCK_METHOD(bool, CanComposeInline, (), (const, override));
  MOCK_METHOD(gfx::Rect, GetCaretBounds, (), (const, override));
  MOCK_METHOD(gfx::Rect, GetSelectionBoundingBox, (), (const, override));
  MOCK_METHOD(bool,
              GetCompositionCharacterBounds,
              (size_t, gfx::Rect*),
              (const, override));
  MOCK_METHOD(bool, HasCompositionText, (), (const, override));
  MOCK_METHOD(ui::TextInputClient::FocusReason,
              GetFocusReason,
              (),
              (const, override));
  MOCK_METHOD(bool, GetTextRange, (gfx::Range*), (const, override));
  MOCK_METHOD(bool, GetCompositionTextRange, (gfx::Range*), (const, override));
  MOCK_METHOD(bool,
              GetEditableSelectionRange,
              (gfx::Range*),
              (const, override));
  MOCK_METHOD(bool, SetEditableSelectionRange, (const gfx::Range&), (override));
  MOCK_METHOD(bool,
              GetTextFromRange,
              (const gfx::Range&, std::u16string*),
              (const, override));
  MOCK_METHOD(void, OnInputMethodChanged, (), (override));
  MOCK_METHOD(bool,
              ChangeTextDirectionAndLayoutAlignment,
              (base::i18n::TextDirection),
              (override));
  MOCK_METHOD(void, ExtendSelectionAndDelete, (size_t, size_t), (override));
  MOCK_METHOD(void, EnsureCaretNotInRect, (const gfx::Rect&), (override));
  MOCK_METHOD(bool,
              IsTextEditCommandEnabled,
              (TextEditCommand),
              (const, override));
  MOCK_METHOD(void,
              SetTextEditCommandForNextKeyEvent,
              (TextEditCommand),
              (override));
  MOCK_METHOD(ukm::SourceId, GetClientSourceForMetrics, (), (const, override));
  MOCK_METHOD(bool, ShouldDoLearning, (), (override));
  MOCK_METHOD(bool,
              SetCompositionFromExistingText,
              (const gfx::Range&, const std::vector<ui::ImeTextSpan>&),
              (override));
#if BUILDFLAG(IS_CHROMEOS)
  MOCK_METHOD(gfx::Range, GetAutocorrectRange, (), (const, override));
  MOCK_METHOD(gfx::Rect, GetAutocorrectCharacterBounds, (), (const, override));
  MOCK_METHOD(bool, SetAutocorrectRange, (const gfx::Range& range), (override));
  MOCK_METHOD(void,
              GetActiveTextInputControlLayoutBounds,
              (absl::optional<gfx::Rect> * control_bounds,
               absl::optional<gfx::Rect>* selection_bounds),
              (override));
#endif

 private:
  TextInputType text_input_type_;
};

class TestInputMethodContextDelegate : public LinuxInputMethodContextDelegate {
 public:
  TestInputMethodContextDelegate() = default;
  TestInputMethodContextDelegate(const TestInputMethodContextDelegate&) =
      delete;
  TestInputMethodContextDelegate& operator=(
      const TestInputMethodContextDelegate&) = delete;
  ~TestInputMethodContextDelegate() override = default;

  void OnCommit(const std::u16string& text) override {
    was_on_commit_called_ = true;
  }
  void OnConfirmCompositionText(bool keep_selection) override {
    was_on_confirm_composition_text_called_ = true;
  }
  void OnPreeditChanged(const ui::CompositionText& composition_text) override {
    was_on_preedit_changed_called_ = true;
  }
  void OnClearGrammarFragments(const gfx::Range& range) override {
    was_on_clear_grammar_fragments_called_ = true;
  }
  void OnAddGrammarFragment(const ui::GrammarFragment& fragment) override {
    was_on_add_grammar_fragment_called_ = true;
  }
  void OnSetAutocorrectRange(const gfx::Range& range) override {
    was_on_set_autocorrect_range_called_ = true;
  }
  void OnPreeditEnd() override {}
  void OnPreeditStart() override {}
  void OnDeleteSurroundingText(size_t before, size_t after) override {
    last_on_delete_surrounding_text_args_ = std::make_pair(before, after);
  }

  void OnSetPreeditRegion(const gfx::Range& range,
                          const std::vector<ImeTextSpan>& spans) override {
    was_on_set_preedit_region_called_ = true;
  }

  void OnSetVirtualKeyboardOccludedBounds(
      const gfx::Rect& screen_bounds) override {
    virtual_keyboard_bounds_ = screen_bounds;
  }

  bool was_on_commit_called() const { return was_on_commit_called_; }

  bool was_on_confirm_composition_text_called() const {
    return was_on_confirm_composition_text_called_;
  }

  bool was_on_preedit_changed_called() const {
    return was_on_preedit_changed_called_;
  }

  bool was_on_set_preedit_region_called() const {
    return was_on_set_preedit_region_called_;
  }

  bool was_on_clear_grammar_fragments_called() const {
    return was_on_clear_grammar_fragments_called_;
  }

  bool was_on_add_grammar_fragment_called() const {
    return was_on_add_grammar_fragment_called_;
  }

  bool was_on_set_autocorrect_range_called() const {
    return was_on_set_autocorrect_range_called_;
  }

  const absl::optional<std::pair<size_t, size_t>>&
  last_on_delete_surrounding_text_args() const {
    return last_on_delete_surrounding_text_args_;
  }

  const absl::optional<gfx::Rect>& virtual_keyboard_bounds() const {
    return virtual_keyboard_bounds_;
  }

 private:
  bool was_on_commit_called_ = false;
  bool was_on_confirm_composition_text_called_ = false;
  bool was_on_preedit_changed_called_ = false;
  bool was_on_set_preedit_region_called_ = false;
  bool was_on_clear_grammar_fragments_called_ = false;
  bool was_on_add_grammar_fragment_called_ = false;
  bool was_on_set_autocorrect_range_called_ = false;
  absl::optional<std::pair<size_t, size_t>>
      last_on_delete_surrounding_text_args_;
  absl::optional<gfx::Rect> virtual_keyboard_bounds_;
};

class WaylandInputMethodContextTest : public WaylandTestSimple {
 public:
  void SetUp() override {
    WaylandTestSimple::SetUp();

    surface_id_ = window_->root_surface()->get_surface_id();

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      // WaylandInputMethodContext behaves differently when no keyboard is
      // attached.
      wl_seat_send_capabilities(server->seat()->resource(),
                                WL_SEAT_CAPABILITY_KEYBOARD);
    });
    ASSERT_TRUE(connection_->seat()->keyboard());

    SetUpInternal();
  }

 protected:
  void SetUpInternal() {
    input_method_context_delegate_ =
        std::make_unique<TestInputMethodContextDelegate>();
    input_method_context_ = std::make_unique<WaylandInputMethodContext>(
        connection_.get(), connection_->event_source(),
        input_method_context_delegate_.get());
    input_method_context_->Init(true);
    connection_->Flush();

    wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());

    // Unset Keyboard focus.
    connection_->window_manager()->SetKeyboardFocusedWindow(nullptr);

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      ASSERT_TRUE(server->text_input_manager_v1()->text_input());
      ASSERT_TRUE(server->text_input_extension_v1()->extended_text_input());
    });

    ASSERT_TRUE(connection_->text_input_manager_v1());
    ASSERT_TRUE(connection_->text_input_extension_v1());
  }

  std::unique_ptr<TestInputMethodContextDelegate>
      input_method_context_delegate_;
  std::unique_ptr<WaylandInputMethodContext> input_method_context_;
  raw_ptr<wl::MockZwpTextInput> zwp_text_input_ = nullptr;
  raw_ptr<wl::MockZcrExtendedTextInput> zcr_extended_text_input_ = nullptr;

  uint32_t surface_id_ = 0u;
};

TEST_F(WaylandInputMethodContextTest, ActivateDeactivate) {
  // Activate is called only when both InputMethod's TextInputClient focus and
  // Wayland's keyboard focus is met.

  // Scenario 1: InputMethod focus is set, then Keyboard focus is set.
  // Unset them in the reversed order.

  InSequence s;
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v1()->text_input();
    ASSERT_TRUE(zwp_text_input);
    EXPECT_CALL(*zwp_text_input,
                Activate(server->GetObject<wl::MockSurface>(id)->resource()))
        .Times(0);
    EXPECT_CALL(*zwp_text_input, ShowInputPanel()).Times(0);
  });

  input_method_context_->UpdateFocus(true, ui::TEXT_INPUT_TYPE_NONE,
                                     ui::TEXT_INPUT_TYPE_TEXT);
  connection_->Flush();

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(zwp_text_input);

    EXPECT_CALL(*zwp_text_input,
                Activate(server->GetObject<wl::MockSurface>(id)->resource()));
    EXPECT_CALL(*zwp_text_input, ShowInputPanel());
  });

  connection_->window_manager()->SetKeyboardFocusedWindow(window_.get());
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(zwp_text_input);

    EXPECT_CALL(*zwp_text_input, HideInputPanel());
    EXPECT_CALL(*zwp_text_input, Deactivate());
  });

  connection_->window_manager()->SetKeyboardFocusedWindow(nullptr);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(zwp_text_input);

    EXPECT_CALL(*zwp_text_input, HideInputPanel()).Times(0);
    EXPECT_CALL(*zwp_text_input, Deactivate()).Times(0);
  });

  input_method_context_->UpdateFocus(true, ui::TEXT_INPUT_TYPE_TEXT,
                                     ui::TEXT_INPUT_TYPE_NONE);
  connection_->Flush();

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(zwp_text_input);

    // Scenario 2: Keyboard focus is set, then InputMethod focus is set.
    // Unset them in the reversed order.
    EXPECT_CALL(*zwp_text_input,
                Activate(server->GetObject<wl::MockSurface>(id)->resource()))
        .Times(0);
    EXPECT_CALL(*zwp_text_input, ShowInputPanel()).Times(0);
  });

  connection_->window_manager()->SetKeyboardFocusedWindow(window_.get());
  connection_->Flush();

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(zwp_text_input);

    EXPECT_CALL(*zwp_text_input,
                Activate(server->GetObject<wl::MockSurface>(id)->resource()));
    EXPECT_CALL(*zwp_text_input, ShowInputPanel());
  });

  input_method_context_->UpdateFocus(true, ui::TEXT_INPUT_TYPE_NONE,
                                     ui::TEXT_INPUT_TYPE_TEXT);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(zwp_text_input);

    EXPECT_CALL(*zwp_text_input, HideInputPanel());
    EXPECT_CALL(*zwp_text_input, Deactivate());
  });

  input_method_context_->UpdateFocus(true, ui::TEXT_INPUT_TYPE_TEXT,
                                     ui::TEXT_INPUT_TYPE_NONE);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(zwp_text_input);

    EXPECT_CALL(*zwp_text_input, HideInputPanel()).Times(0);
    EXPECT_CALL(*zwp_text_input, Deactivate()).Times(0);
  });

  connection_->window_manager()->SetKeyboardFocusedWindow(nullptr);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(zwp_text_input);
  });
}

TEST_F(WaylandInputMethodContextTest, Reset) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(), Reset());
  });
  input_method_context_->Reset();
  connection_->Flush();
}

TEST_F(WaylandInputMethodContextTest, SetCursorLocation) {
  constexpr gfx::Rect cursor_location(50, 0, 1, 1);
  PostToServerAndWait([cursor_location](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(
        *server->text_input_manager_v1()->text_input(),
        SetCursorRect(cursor_location.x(), cursor_location.y(),
                      cursor_location.width(), cursor_location.height()));
  });
  input_method_context_->SetCursorLocation(cursor_location);
  connection_->Flush();
}

TEST_F(WaylandInputMethodContextTest, SetSurroundingTextForShortText) {
  const std::u16string text(50, u'あ');
  constexpr gfx::Range range(20, 30);

  const std::string kExpectedSentText(base::UTF16ToUTF8(text));
  constexpr gfx::Range kExpectedSentRange(60, 90);

  PostToServerAndWait([kExpectedSentText, kExpectedSentRange](
                          wl::TestWaylandServerThread* server) {
    // The text and range sent as wayland protocol must be same to the original
    // text and range where the original text is shorter than 4000 byte.
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText(kExpectedSentText, kExpectedSentRange))
        .Times(1);
  });

  input_method_context_->SetSurroundingText(text, range);
  connection_->Flush();

  PostToServerAndWait(
      [kExpectedSentRange](wl::TestWaylandServerThread* server) {
        auto* text_input = server->text_input_manager_v1()->text_input();
        Mock::VerifyAndClearExpectations(text_input);

        // Test OnDeleteSurroundingText with this input.
        zwp_text_input_v1_send_delete_surrounding_text(
            text_input->resource(), kExpectedSentRange.start(),
            kExpectedSentRange.length());
      });

  EXPECT_EQ(
      input_method_context_delegate_->last_on_delete_surrounding_text_args(),
      (std::pair<size_t, size_t>(0, 0)));
}

TEST_F(WaylandInputMethodContextTest, SetSurroundingTextForLongText) {
  const std::u16string text(5000, u'あ');
  constexpr gfx::Range range(2800, 3200);

  // The text sent as wayland protocol must be at most 4000 byte and long
  // enough in the limitation.
  const std::string kExpectedSentText(
      base::UTF16ToUTF8(std::u16string(1332, u'あ')));
  // The selection range must be relocated accordingly to the sent text.
  constexpr gfx::Range kExpectedSentRange(1398, 2598);

  PostToServerAndWait([kExpectedSentText, kExpectedSentRange](
                          wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText(kExpectedSentText, kExpectedSentRange))
        .Times(1);
  });

  input_method_context_->SetSurroundingText(text, range);
  connection_->Flush();

  PostToServerAndWait(
      [kExpectedSentRange](wl::TestWaylandServerThread* server) {
        auto* text_input = server->text_input_manager_v1()->text_input();
        Mock::VerifyAndClearExpectations(text_input);

        // Test OnDeleteSurroundingText with this input.
        zwp_text_input_v1_send_delete_surrounding_text(
            text_input->resource(), kExpectedSentRange.start(),
            kExpectedSentRange.length());
      });

  EXPECT_EQ(
      input_method_context_delegate_->last_on_delete_surrounding_text_args(),
      (std::pair<size_t, size_t>(0, 0)));
}

TEST_F(WaylandInputMethodContextTest, SetSurroundingTextForLongTextInLeftEdge) {
  const std::u16string text(5000, u'あ');
  constexpr gfx::Range range(0, 500);

  // The text sent as wayland protocol must be at most 4000 byte and large
  // enough in the limitation.
  const std::string kExpectedSentText(
      base::UTF16ToUTF8(std::u16string(1333, u'あ')));
  // The selection range must be relocated accordingly to the sent text.
  constexpr gfx::Range kExpectedSentRange(0, 1500);

  PostToServerAndWait([kExpectedSentText, kExpectedSentRange](
                          wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText(kExpectedSentText, kExpectedSentRange))
        .Times(1);
  });

  input_method_context_->SetSurroundingText(text, range);
  connection_->Flush();

  PostToServerAndWait(
      [kExpectedSentRange](wl::TestWaylandServerThread* server) {
        auto* text_input = server->text_input_manager_v1()->text_input();
        Mock::VerifyAndClearExpectations(text_input);

        // Test OnDeleteSurroundingText with this input.
        zwp_text_input_v1_send_delete_surrounding_text(
            text_input->resource(), kExpectedSentRange.start(),
            kExpectedSentRange.length());
      });

  EXPECT_EQ(
      input_method_context_delegate_->last_on_delete_surrounding_text_args(),
      (std::pair<size_t, size_t>(0, 0)));
}

TEST_F(WaylandInputMethodContextTest,
       SetSurroundingTextForLongTextInRightEdge) {
  const std::u16string text(5000, u'あ');
  constexpr gfx::Range range(4500, 5000);

  // The text sent as wayland protocol must be at most 4000 byte and large
  // enough in the limitation.
  const std::string kExpectedSentText(
      base::UTF16ToUTF8(std::u16string(1333, u'あ')));
  // The selection range must be relocated accordingly to the sent text.
  constexpr gfx::Range kExpectedSentRange(2499, 3999);

  PostToServerAndWait([kExpectedSentText, kExpectedSentRange](
                          wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText(kExpectedSentText, kExpectedSentRange))
        .Times(1);
  });

  input_method_context_->SetSurroundingText(text, range);
  connection_->Flush();

  PostToServerAndWait(
      [kExpectedSentRange](wl::TestWaylandServerThread* server) {
        auto* text_input = server->text_input_manager_v1()->text_input();
        Mock::VerifyAndClearExpectations(text_input);

        // Test OnDeleteSurroundingText with this input.
        zwp_text_input_v1_send_delete_surrounding_text(
            text_input->resource(), kExpectedSentRange.start(),
            kExpectedSentRange.length());
      });

  EXPECT_EQ(
      input_method_context_delegate_->last_on_delete_surrounding_text_args(),
      (std::pair<size_t, size_t>(0, 0)));
}

TEST_F(WaylandInputMethodContextTest, SetSurroundingTextForLongRange) {
  const std::u16string text(5000, u'あ');
  constexpr gfx::Range range(1000, 4000);

  // set_surrounding_text request should be skipped when the selection range in
  // UTF8 form is longer than 4000 byte.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText(_, _))
        .Times(0);
  });

  input_method_context_->SetSurroundingText(text, range);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_manager_v1()->text_input());
  });
}

TEST_F(WaylandInputMethodContextTest, DeleteSurroundingTextWithExtendedRange) {
  const std::u16string text(50, u'あ');
  const gfx::Range range(20, 30);

  // The text and range sent as wayland protocol must be same to the original
  // text and range where the original text is shorter than 4000 byte.
  const std::string kExpectedSentText(base::UTF16ToUTF8(text));
  // The selection range must be relocated accordingly to the sent text.
  constexpr gfx::Range kExpectedSentRange(60, 90);

  PostToServerAndWait([kExpectedSentText, kExpectedSentRange](
                          wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText(kExpectedSentText, kExpectedSentRange))
        .Times(1);
  });

  input_method_context_->SetSurroundingText(text, range);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(text_input);

    // Test OnDeleteSurroundingText with this input.
    // One char more deletion for each before and after the selection.
    zwp_text_input_v1_send_delete_surrounding_text(text_input->resource(), 57,
                                                   36);
  });

  EXPECT_EQ(
      input_method_context_delegate_->last_on_delete_surrounding_text_args(),
      (std::pair<size_t, size_t>(1, 1)));
}

TEST_F(WaylandInputMethodContextTest, SetContentType) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(
        *server->text_input_extension_v1()->extended_text_input(),
        SetInputType(ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_TYPE_URL,
                     ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_MODE_DEFAULT,
                     ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_FLAGS_AUTOCOMPLETE_ON,
                     ZCR_EXTENDED_TEXT_INPUT_V1_LEARNING_MODE_ENABLED))
        .Times(1);
  });
  input_method_context_->SetContentType(TEXT_INPUT_TYPE_URL,
                                        TEXT_INPUT_MODE_DEFAULT,
                                        TEXT_INPUT_FLAG_AUTOCOMPLETE_ON,
                                        /*should_do_learning=*/true);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_extension_v1()->extended_text_input());
  });
}

TEST_F(WaylandInputMethodContextTest, SetContentTypeWithoutLearning) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(
        *server->text_input_extension_v1()->extended_text_input(),
        SetInputType(ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_TYPE_URL,
                     ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_MODE_DEFAULT,
                     ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_FLAGS_AUTOCOMPLETE_ON,
                     ZCR_EXTENDED_TEXT_INPUT_V1_LEARNING_MODE_DISABLED))
        .Times(1);
  });
  input_method_context_->SetContentType(TEXT_INPUT_TYPE_URL,
                                        TEXT_INPUT_MODE_DEFAULT,
                                        TEXT_INPUT_FLAG_AUTOCOMPLETE_ON,
                                        /*should_do_learning=*/false);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_extension_v1()->extended_text_input());
  });
}

TEST_F(WaylandInputMethodContextTest, OnPreeditChanged) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    zwp_text_input_v1_send_preedit_string(
        server->text_input_manager_v1()->text_input()->resource(),
        server->GetNextSerial(), "PreeditString", "");
  });
  EXPECT_TRUE(input_method_context_delegate_->was_on_preedit_changed_called());
}

TEST_F(WaylandInputMethodContextTest, OnCommit) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    zwp_text_input_v1_send_commit_string(
        server->text_input_manager_v1()->text_input()->resource(),
        server->GetNextSerial(), "CommitString");
  });
  EXPECT_TRUE(input_method_context_delegate_->was_on_commit_called());
}

// TODO(1353668): WaylandInputMethodContext::OnCursorPosition sets
// |pending_keep_selection| only on lacros. That's the reason why this test
// doesn't pass on Linux. We need to clarify that.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE(x) x
#else
#define MAYBE(x) DISABLED_##x
#endif

TEST_F(WaylandInputMethodContextTest, MAYBE(OnConfirmCompositionText)) {
  constexpr char16_t text[] = u"ab😀cあdef";
  constexpr gfx::Range range(5, 6);  // あ is selected.

  // SetSurroundingText should be called in UTF-8.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText("ab😀cあdef", gfx::Range(7, 10)));
  });
  input_method_context_->SetSurroundingText(text, range);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(text_input);

    zwp_text_input_v1_send_cursor_position(text_input->resource(), 7, 10);
    zwp_text_input_v1_send_commit_string(text_input->resource(), 0,
                                         "ab😀cあdef");
  });

  EXPECT_TRUE(
      input_method_context_delegate_->was_on_confirm_composition_text_called());
}

TEST_F(WaylandInputMethodContextTest,
       MAYBE(OnConfirmCompositionTextForLongRange)) {
  const std::u16string original_text(5000, u'あ');
  constexpr gfx::Range original_range(4000, 4500);

  // Text longer than 4000 bytes is trimmed to meet the limitation.
  // Selection range is also adjusted by the trimmed text before sendin to Exo.
  const std::string kExpectedSentText(
      base::UTF16ToUTF8(std::u16string(1332, u'あ')));
  constexpr gfx::Range kExpectedSentRange(1248, 2748);

  // SetSurroundingText should be called in UTF-8.
  PostToServerAndWait([kExpectedSentText, kExpectedSentRange](
                          wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText(kExpectedSentText, kExpectedSentRange));
  });
  input_method_context_->SetSurroundingText(original_text, original_range);
  connection_->Flush();

  PostToServerAndWait([kExpectedSentText, kExpectedSentRange](
                          wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(text_input);

    zwp_text_input_v1_send_cursor_position(text_input->resource(),
                                           kExpectedSentRange.start(),
                                           kExpectedSentRange.end());
    zwp_text_input_v1_send_commit_string(text_input->resource(), 0,
                                         kExpectedSentText.c_str());
  });

  EXPECT_TRUE(
      input_method_context_delegate_->was_on_confirm_composition_text_called());
}

TEST_F(WaylandInputMethodContextTest, OnSetPreeditRegion_Success) {
  constexpr char16_t text[] = u"abcあdef";
  const gfx::Range range(3, 4);  // あ is selected.

  // SetSurroundingText should be called in UTF-8.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText("abcあdef", gfx::Range(3, 6)));
  });

  input_method_context_->SetSurroundingText(text, range);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_manager_v1()->text_input());

    // Specify "cあd" as a new preedit region.
    zcr_extended_text_input_v1_send_set_preedit_region(
        server->text_input_extension_v1()->extended_text_input()->resource(),
        -4, 5);
  });

  EXPECT_TRUE(
      input_method_context_delegate_->was_on_set_preedit_region_called());
}

TEST_F(WaylandInputMethodContextTest, OnSetPreeditRegion_NoSurroundingText) {
  // If no surrounding text is set yet, set_preedit_region would fail.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    zcr_extended_text_input_v1_send_set_preedit_region(
        server->text_input_extension_v1()->extended_text_input()->resource(),
        -1, 3);
  });
  EXPECT_FALSE(
      input_method_context_delegate_->was_on_set_preedit_region_called());
}

// The range is represented in UTF-16 code points, so it is independent from
// grapheme clusters.
TEST_F(WaylandInputMethodContextTest,
       OnSetPreeditRegion_GraphemeClusterIndependeceSimple) {
  // Single code point representation of é.
  constexpr char16_t u16_text[] = u"\u00E9";
  constexpr char u8_text[] = "\xC3\xA9";  // In UTF-8 encode.

  constexpr gfx::Range u16_range(0, 1);
  constexpr gfx::Range u8_range(0, 2);

  // Double check the text has one grapheme cluster.
  ASSERT_EQ(1u, CountGraphemeCluster(u16_text));

  // SetSurroundingText should be called in UTF-8.
  PostToServerAndWait([u8_range, u8_text](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText(u8_text, u8_range));
  });

  input_method_context_->SetSurroundingText(u16_text, u16_range);
  connection_->Flush();

  PostToServerAndWait([u8_range](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_manager_v1()->text_input());

    // Specify the whole range as a new preedit region.
    zcr_extended_text_input_v1_send_set_preedit_region(
        server->text_input_extension_v1()->extended_text_input()->resource(),
        -static_cast<int32_t>(u8_range.length()), u8_range.length());
  });

  EXPECT_TRUE(
      input_method_context_delegate_->was_on_set_preedit_region_called());
}

TEST_F(WaylandInputMethodContextTest,
       OnSetPreeditRegion_GraphemeClusterIndependeceCombined) {
  // Decomposed code point representation of é.
  constexpr char16_t u16_text[] = u"\u0065\u0301";
  constexpr char u8_text[] = "\x65\xCC\x81";  // In UTF-8 encode.

  constexpr gfx::Range u16_range(0, 2);
  constexpr gfx::Range u8_range(0, 3);

  // Double check the text has one grapheme cluster.
  ASSERT_EQ(1u, CountGraphemeCluster(u16_text));

  // SetSurroundingText should be called in UTF-8.
  PostToServerAndWait([u8_range, u8_text](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText(u8_text, u8_range));
  });

  input_method_context_->SetSurroundingText(u16_text, u16_range);
  connection_->Flush();

  PostToServerAndWait([u8_range](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_manager_v1()->text_input());

    // Specify the whole range as a new preedit region.
    zcr_extended_text_input_v1_send_set_preedit_region(
        server->text_input_extension_v1()->extended_text_input()->resource(),
        -static_cast<int32_t>(u8_range.length()), u8_range.length());
  });

  EXPECT_TRUE(
      input_method_context_delegate_->was_on_set_preedit_region_called());
}

TEST_F(WaylandInputMethodContextTest, OnClearGrammarFragments) {
  input_method_context_->OnClearGrammarFragments(gfx::Range(1, 5));
  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
  EXPECT_TRUE(
      input_method_context_delegate_->was_on_clear_grammar_fragments_called());
}

TEST_F(WaylandInputMethodContextTest, OnAddGrammarFragments) {
  input_method_context_->OnAddGrammarFragment(
      ui::GrammarFragment(gfx::Range(1, 5), "test"));
  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
  EXPECT_TRUE(
      input_method_context_delegate_->was_on_add_grammar_fragment_called());
}

TEST_F(WaylandInputMethodContextTest, OnSetAutocorrectRange) {
  input_method_context_->OnSetAutocorrectRange(gfx::Range(1, 5));
  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
  EXPECT_TRUE(
      input_method_context_delegate_->was_on_set_autocorrect_range_called());
}

TEST_F(WaylandInputMethodContextTest, OnSetVirtualKeyboardOccludedBounds) {
  constexpr gfx::Rect kBounds(10, 20, 300, 400);
  input_method_context_->OnSetVirtualKeyboardOccludedBounds(kBounds);
  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
  EXPECT_EQ(input_method_context_delegate_->virtual_keyboard_bounds(), kBounds);
}

TEST_F(WaylandInputMethodContextTest,
       OnSetVirtualKeyboardOccludedBoundsUpdatesPastTextInputClients) {
  auto client1 = std::make_unique<MockTextInputClient>(TEXT_INPUT_TYPE_TEXT);
  auto client2 = std::make_unique<MockTextInputClient>(TEXT_INPUT_TYPE_URL);

  input_method_context_->WillUpdateFocus(client1.get(), client2.get());
  input_method_context_->UpdateFocus(true, client1->GetTextInputType(),
                                     client2->GetTextInputType());
  input_method_context_->WillUpdateFocus(client2.get(), nullptr);
  input_method_context_->UpdateFocus(false, client2->GetTextInputType(),
                                     ui::TEXT_INPUT_TYPE_NONE);

  // Clients should get further bounds updates.
  constexpr gfx::Rect kBounds(10, 20, 300, 400);
  EXPECT_CALL(*client1, EnsureCaretNotInRect(kBounds));
  EXPECT_CALL(*client2, EnsureCaretNotInRect(kBounds));
  input_method_context_->OnSetVirtualKeyboardOccludedBounds(kBounds);
  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
  Mock::VerifyAndClearExpectations(client1.get());
  Mock::VerifyAndClearExpectations(client2.get());

  // Clients should get the empty bounds then be removed.
  const gfx::Rect kBoundsEmpty(0, 30, 0, 0);
  EXPECT_CALL(*client1, EnsureCaretNotInRect(kBoundsEmpty));
  EXPECT_CALL(*client2, EnsureCaretNotInRect(kBoundsEmpty));
  input_method_context_->OnSetVirtualKeyboardOccludedBounds(kBoundsEmpty);
  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
  Mock::VerifyAndClearExpectations(client1.get());
  Mock::VerifyAndClearExpectations(client2.get());

  // Verify client no longer gets bounds updates.
  const gfx::Rect kBounds2(0, 40, 100, 200);
  EXPECT_CALL(*client1, EnsureCaretNotInRect).Times(0);
  EXPECT_CALL(*client2, EnsureCaretNotInRect).Times(0);
  input_method_context_->OnSetVirtualKeyboardOccludedBounds(kBounds2);
  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
  Mock::VerifyAndClearExpectations(client1.get());
  Mock::VerifyAndClearExpectations(client2.get());
}

TEST_F(WaylandInputMethodContextTest,
       OnSetVirtualKeyboardOccludedBoundsWithDeletedPastTextInputClient) {
  auto client = std::make_unique<MockTextInputClient>(TEXT_INPUT_TYPE_TEXT);

  input_method_context_->WillUpdateFocus(client.get(), nullptr);
  input_method_context_->UpdateFocus(false, client->GetTextInputType(),
                                     ui::TEXT_INPUT_TYPE_NONE);

  const gfx::Rect kBounds(10, 20, 300, 400);
  EXPECT_CALL(*client, EnsureCaretNotInRect(kBounds));
  input_method_context_->OnSetVirtualKeyboardOccludedBounds(kBounds);
  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
  Mock::VerifyAndClearExpectations(client.get());

  client.reset();
  input_method_context_->OnSetVirtualKeyboardOccludedBounds(kBounds);
  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
}

TEST_F(WaylandInputMethodContextTest, DisplayVirtualKeyboard) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                ShowInputPanel())
        .Times(1);
  });
  EXPECT_TRUE(input_method_context_->DisplayVirtualKeyboard());
  connection_->Flush();
  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
}

TEST_F(WaylandInputMethodContextTest, DismissVirtualKeyboard) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                HideInputPanel());
  });
  input_method_context_->DismissVirtualKeyboard();
  connection_->Flush();
  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
}

TEST_F(WaylandInputMethodContextTest, UpdateVirtualKeyboardState) {
  EXPECT_FALSE(input_method_context_->IsKeyboardVisible());
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    zwp_text_input_v1_send_input_panel_state(
        server->text_input_manager_v1()->text_input()->resource(), 1);
  });

  EXPECT_TRUE(input_method_context_->IsKeyboardVisible());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    zwp_text_input_v1_send_input_panel_state(
        server->text_input_manager_v1()->text_input()->resource(), 0);
  });

  EXPECT_FALSE(input_method_context_->IsKeyboardVisible());
}

class WaylandInputMethodContextNoKeyboardTest
    : public WaylandInputMethodContextTest {
 public:
  void SetUp() override {
    // Call the skip base implementation to avoid setting up the keyboard.
    WaylandTestSimple::SetUp();

    ASSERT_FALSE(connection_->seat()->keyboard());

    SetUpInternal();
  }
};

TEST_F(WaylandInputMethodContextNoKeyboardTest, ActivateDeactivate) {
  const uint32_t surface_id = window_->root_surface()->get_surface_id();

  // Because there is no keyboard, Activate is called as soon as InputMethod's
  // TextInputClient focus is met.

  InSequence s;
  PostToServerAndWait([id = surface_id](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v1()->text_input();
    EXPECT_CALL(*zwp_text_input,
                Activate(server->GetObject<wl::MockSurface>(id)->resource()));
    EXPECT_CALL(*zwp_text_input, ShowInputPanel());
  });

  input_method_context_->UpdateFocus(true, ui::TEXT_INPUT_TYPE_NONE,
                                     ui::TEXT_INPUT_TYPE_TEXT);
  connection_->Flush();
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(zwp_text_input);

    EXPECT_CALL(*zwp_text_input, HideInputPanel());
    EXPECT_CALL(*zwp_text_input, Deactivate());
  });

  input_method_context_->UpdateFocus(false, ui::TEXT_INPUT_TYPE_TEXT,
                                     ui::TEXT_INPUT_TYPE_NONE);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_manager_v1()->text_input());
  });
}

TEST_F(WaylandInputMethodContextNoKeyboardTest, UpdateFocusBetweenTextFields) {
  const uint32_t surface_id = window_->root_surface()->get_surface_id();

  // Because there is no keyboard, Activate is called as soon as InputMethod's
  // TextInputClient focus is met.

  InSequence s;
  PostToServerAndWait([id = surface_id](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v1()->text_input();
    EXPECT_CALL(*zwp_text_input,
                Activate(server->GetObject<wl::MockSurface>(id)->resource()));
    EXPECT_CALL(*zwp_text_input, ShowInputPanel());
  });

  input_method_context_->UpdateFocus(true, ui::TEXT_INPUT_TYPE_NONE,
                                     ui::TEXT_INPUT_TYPE_TEXT);
  connection_->Flush();

  PostToServerAndWait([id = surface_id](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(zwp_text_input);

    // Make sure virtual keyboard is not unnecessarily hidden.
    EXPECT_CALL(*zwp_text_input, HideInputPanel()).Times(0);
    EXPECT_CALL(*zwp_text_input, Deactivate());
    EXPECT_CALL(*zwp_text_input,
                Activate(server->GetObject<wl::MockSurface>(id)->resource()));
    EXPECT_CALL(*zwp_text_input, ShowInputPanel()).Times(0);
  });

  input_method_context_->UpdateFocus(false, ui::TEXT_INPUT_TYPE_TEXT,
                                     ui::TEXT_INPUT_TYPE_TEXT);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_manager_v1()->text_input());
  });
}

}  // namespace ui
