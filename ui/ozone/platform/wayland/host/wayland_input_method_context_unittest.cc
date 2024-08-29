// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_input_method_context.h"

#include <text-input-unstable-v1-server-protocol.h>
#include <wayland-server.h>

#include <memory>
#include <optional>
#include <string_view>

#include "base/environment.h"
#include "base/i18n/break_iterator.h"
#include "base/memory/raw_ptr.h"
#include "base/nix/xdg_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_zcr_extended_text_input.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_text_input.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zcr_text_input_extension.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Optional;
using ::testing::SaveArg;
using ::testing::Values;

namespace ui {

// Returns the number of grapheme clusters in the text.
std::optional<size_t> CountGraphemeCluster(std::u16string_view text) {
  base::i18n::BreakIterator iter(text,
                                 base::i18n::BreakIterator::BREAK_CHARACTER);
  if (!iter.Init())
    return std::nullopt;
  size_t result = 0;
  while (iter.Advance())
    ++result;
  return result;
}

// TODO(crbug.com/40240866): Subclass FakeTextInputClient after pruning deps.
class MockTextInputClient : public TextInputClient {
 public:
  explicit MockTextInputClient(TextInputType text_input_type) {
    text_input_type_ = text_input_type;
  }
  MockTextInputClient(const MockTextInputClient& other) = delete;
  MockTextInputClient& operator=(const MockTextInputClient& other) = delete;
  ~MockTextInputClient() override = default;

  TextInputType GetTextInputType() const override { return text_input_type_; }

  base::WeakPtr<TextInputClient> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

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
              (std::optional<gfx::Rect> * control_bounds,
               std::optional<gfx::Rect>* selection_bounds),
              (override));
#endif

 private:
  TextInputType text_input_type_;
  base::WeakPtrFactory<MockTextInputClient> weak_ptr_factory_{this};
};

class MockZWPTextInputWrapper : public ZWPTextInputWrapper {
 public:
  ~MockZWPTextInputWrapper() override = default;

  MOCK_METHOD(void, Reset, (), (override));

  MOCK_METHOD(void,
              Activate,
              (WaylandWindow * window, ui::TextInputClient::FocusReason reason),
              (override));
  MOCK_METHOD(void, Deactivate, (), (override));

  MOCK_METHOD(void, ShowInputPanel, (), (override));
  MOCK_METHOD(void, HideInputPanel, (), (override));

  MOCK_METHOD(void, SetCursorRect, (const gfx::Rect& rect), (override));
  MOCK_METHOD(void,
              SetSurroundingText,
              (const std::string& text,
               const gfx::Range& preedit_range,
               const gfx::Range& selection_range),
              (override));
  MOCK_METHOD(bool, HasAdvancedSurroundingTextSupport, (), (const override));
  MOCK_METHOD(void,
              SetSurroundingTextOffsetUtf16,
              (uint32_t offset_utf16),
              (override));
  MOCK_METHOD(void,
              SetContentType,
              (ui::TextInputType type,
               ui::TextInputMode mode,
               uint32_t flags,
               bool should_do_learning,
               bool can_compose_inline),
              (override));

  MOCK_METHOD(void,
              SetGrammarFragmentAtCursor,
              (const ui::GrammarFragment& fragment),
              (override));
  MOCK_METHOD(void,
              SetAutocorrectInfo,
              (const gfx::Range& autocorrect_range,
               const gfx::Rect& autocorrect_bounds),
              (override));
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
    last_commit_text_ = text;
  }
  void OnConfirmCompositionText(bool keep_selection) override {
    last_on_confirm_composition_arg_ = keep_selection;
  }
  void OnPreeditChanged(const ui::CompositionText& composition_text) override {
    was_on_preedit_changed_called_ = true;
    last_on_preedit_changed_args_ = composition_text;
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
  void OnInsertImage(const GURL& src) override {
    was_on_insert_image_range_called_ = true;
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

  const std::optional<ui::CompositionText>& last_preedit() {
    return last_on_preedit_changed_args_;
  }

  std::optional<std::u16string> last_commit_text() const {
    return last_commit_text_;
  }

  const std::optional<bool>& last_on_confirm_composition_arg() const {
    return last_on_confirm_composition_arg_;
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

  bool was_on_insert_image_called() const {
    return was_on_insert_image_range_called_;
  }

  const std::optional<std::pair<size_t, size_t>>&
  last_on_delete_surrounding_text_args() const {
    return last_on_delete_surrounding_text_args_;
  }

  const std::optional<gfx::Rect>& virtual_keyboard_bounds() const {
    return virtual_keyboard_bounds_;
  }

 private:
  bool was_on_commit_called_ = false;
  std::optional<std::u16string> last_commit_text_;
  std::optional<bool> last_on_confirm_composition_arg_;
  bool was_on_preedit_changed_called_ = false;
  bool was_on_set_preedit_region_called_ = false;
  bool was_on_clear_grammar_fragments_called_ = false;
  bool was_on_add_grammar_fragment_called_ = false;
  bool was_on_set_autocorrect_range_called_ = false;
  bool was_on_insert_image_range_called_ = false;
  std::optional<ui::CompositionText> last_on_preedit_changed_args_;
  std::optional<std::pair<size_t, size_t>>
      last_on_delete_surrounding_text_args_;
  std::optional<gfx::Rect> virtual_keyboard_bounds_;
};

class TestKeyboardDelegate : public WaylandKeyboard::Delegate {
 public:
  TestKeyboardDelegate() = default;
  TestKeyboardDelegate(const TestKeyboardDelegate&) = delete;
  TestKeyboardDelegate& operator=(const TestKeyboardDelegate&) = delete;
  ~TestKeyboardDelegate() override = default;

  void OnKeyboardFocusChanged(WaylandWindow* window, bool focused) override {}
  void OnKeyboardModifiersChanged(int modifiers) override {}
  uint32_t OnKeyboardKeyEvent(EventType type,
                              DomCode dom_code,
                              bool repeat,
                              std::optional<uint32_t> serial,
                              base::TimeTicks timestamp,
                              int device_id,
                              WaylandKeyboard::KeyEventKind kind) override {
    last_event_timestamp_ = timestamp;
    return 0;
  }
  void OnSynthesizedKeyPressEvent(DomCode dom_code,
                                  base::TimeTicks timestamp) override {}

  base::TimeTicks last_event_timestamp() const { return last_event_timestamp_; }

 private:
  base::TimeTicks last_event_timestamp_;
};

class WaylandInputMethodContextTestBase : public WaylandTest {
 public:
  void SetUp() override {
    WaylandTest::SetUp();

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

  wl::TestZcrTextInputExtensionV1::Version GetApiVersion() {
    return GetParam().text_input_extension_version;
  }

 protected:
  void SetUpInternal() {
    input_method_context_delegate_ =
        std::make_unique<TestInputMethodContextDelegate>();
    keyboard_delegate_ = std::make_unique<TestKeyboardDelegate>();
    input_method_context_ = std::make_unique<WaylandInputMethodContext>(
        connection_.get(), keyboard_delegate_.get(),
        input_method_context_delegate_.get());
    input_method_context_->Init(
        true, nullptr,
        // Ensure by default it doesn't pick the current desktop from the system
        // the tests are running on.
        base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_OTHER);
    connection_->Flush();

    WaylandTestBase::SyncDisplay();

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
  std::unique_ptr<TestKeyboardDelegate> keyboard_delegate_;
  std::unique_ptr<WaylandInputMethodContext> input_method_context_;

  uint32_t surface_id_ = 0u;
};

using WaylandInputMethodContextTest = WaylandInputMethodContextTestBase;
using WaylandInputMethodContextOldServerTest =
    WaylandInputMethodContextTestBase;

INSTANTIATE_TEST_SUITE_P(
    TextInputExtensionLatestVersion,
    WaylandInputMethodContextTest,
    ::testing::Values(
        wl::ServerConfig{
            .text_input_extension_version =
                wl::TestZcrTextInputExtensionV1::Version::kV8,
        },
        wl::ServerConfig{}));

INSTANTIATE_TEST_SUITE_P(
    TextInputExtensionV7,
    WaylandInputMethodContextOldServerTest,
    ::testing::Values(wl::ServerConfig{
        .text_input_extension_version =
            wl::TestZcrTextInputExtensionV1::Version::kV7}));

TEST_P(WaylandInputMethodContextOldServerTest, SetInputType) {
  connection_->window_manager()->SetKeyboardFocusedWindow(window_.get());
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_extension_v1()->extended_text_input(),
                DeprecatedSetInputType(
                    ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_TYPE_URL,
                    ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_MODE_DEFAULT,
                    ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_FLAGS_AUTOCOMPLETE_ON,
                    ZCR_EXTENDED_TEXT_INPUT_V1_LEARNING_MODE_ENABLED))
        .Times(1);
  });
  LinuxInputMethodContext::TextInputClientAttributes attributes;
  attributes.input_type = TEXT_INPUT_TYPE_URL;
  attributes.input_mode = TEXT_INPUT_MODE_DEFAULT;
  attributes.flags = TEXT_INPUT_FLAG_AUTOCOMPLETE_ON;
  attributes.should_do_learning = true;
  attributes.can_compose_inline = false;

  input_method_context_->UpdateFocus(
      /*has_client=*/true, TEXT_INPUT_TYPE_NONE, attributes,
      TextInputClient::FOCUS_REASON_OTHER);

  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_extension_v1()->extended_text_input());
  });
}

TEST_P(WaylandInputMethodContextTest, ActivateDeactivate) {
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

  LinuxInputMethodContext::TextInputClientAttributes attributes;
  attributes.input_type = TEXT_INPUT_TYPE_TEXT;
  input_method_context_->UpdateFocus(true, ui::TEXT_INPUT_TYPE_NONE, attributes,
                                     ui::TextInputClient::FOCUS_REASON_OTHER);
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

  attributes.input_type = TEXT_INPUT_TYPE_NONE;
  input_method_context_->UpdateFocus(true, ui::TEXT_INPUT_TYPE_TEXT, attributes,
                                     ui::TextInputClient::FOCUS_REASON_NONE);
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

  attributes.input_type = TEXT_INPUT_TYPE_TEXT;
  input_method_context_->UpdateFocus(true, ui::TEXT_INPUT_TYPE_NONE, attributes,
                                     ui::TextInputClient::FOCUS_REASON_OTHER);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(zwp_text_input);

    EXPECT_CALL(*zwp_text_input, HideInputPanel());
    EXPECT_CALL(*zwp_text_input, Deactivate());
  });

  attributes.input_type = TEXT_INPUT_TYPE_NONE;
  input_method_context_->UpdateFocus(true, ui::TEXT_INPUT_TYPE_TEXT, attributes,
                                     ui::TextInputClient::FOCUS_REASON_NONE);
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

TEST_P(WaylandInputMethodContextTest, Reset) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(), Reset());
  });
  input_method_context_->Reset();
  connection_->Flush();
}

TEST_P(WaylandInputMethodContextTest, SetCursorLocation) {
  constexpr gfx::Rect cursor_location(50, 20, 1, 1);
  constexpr gfx::Rect window_bounds(20, 10, 100, 100);
  PostToServerAndWait(
      [cursor_location, window_bounds](wl::TestWaylandServerThread* server) {
        EXPECT_CALL(
            *server->text_input_manager_v1()->text_input(),
            SetCursorRect(cursor_location.x() - window_bounds.x(),
                          cursor_location.y() - window_bounds.y(),
                          cursor_location.width(), cursor_location.height()));
      });
  window_->SetBoundsInDIP(window_bounds);
  connection_->window_manager()->SetKeyboardFocusedWindow(window_.get());
  input_method_context_->SetCursorLocation(cursor_location);
  connection_->Flush();
}

TEST_P(WaylandInputMethodContextTest, SetSurroundingTextForShortText) {
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

  input_method_context_->SetSurroundingText(text, gfx::Range(0, 50),
                                            gfx::Range::InvalidRange(), range,
                                            std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            range);
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
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      std::u16string(40, u'あ'));
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(20));
}

TEST_P(WaylandInputMethodContextTest, SetSurroundingTextForLongText) {
  const std::u16string text(5000, u'あ');
  constexpr gfx::Range range(2800, 3200);

  std::string expected_sent_text;
  gfx::Range expected_sent_range;
  if (GetApiVersion() == wl::TestZcrTextInputExtensionV1::Version::kV8) {
    // In the old protocol, the text sent as wayland protocol must be at most
    // 4000 byte and long enough in the limitation.
    expected_sent_text = base::UTF16ToUTF8(std::u16string(1332, u'あ'));
    // The selection range must be relocated accordingly to the sent text.
    expected_sent_range = gfx::Range(1398, 2598);
  } else {
    // In the new protocol, the whole selection text with 500 bytes buffers are
    // sent.
    expected_sent_text = base::UTF16ToUTF8(std::u16string(732, u'あ'));
    expected_sent_range = gfx::Range(498, 1698);
  }

  PostToServerAndWait([expected_sent_text, expected_sent_range](
                          wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText(expected_sent_text, expected_sent_range))
        .Times(1);
  });

  input_method_context_->SetSurroundingText(text, gfx::Range(0, 5000),
                                            gfx::Range::InvalidRange(), range,
                                            std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            range);
  connection_->Flush();

  PostToServerAndWait(
      [expected_sent_range](wl::TestWaylandServerThread* server) {
        auto* text_input = server->text_input_manager_v1()->text_input();
        Mock::VerifyAndClearExpectations(text_input);

        // Test OnDeleteSurroundingText with this input.
        zwp_text_input_v1_send_delete_surrounding_text(
            text_input->resource(), expected_sent_range.start(),
            expected_sent_range.length());
      });

  EXPECT_EQ(
      input_method_context_delegate_->last_on_delete_surrounding_text_args(),
      (std::pair<size_t, size_t>(0, 0)));
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      std::u16string(4600, u'あ'));
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(2800));
}

TEST_P(WaylandInputMethodContextTest, SetSurroundingTextForLongTextInLeftEdge) {
  const std::u16string text(5000, u'あ');
  constexpr gfx::Range range(0, 500);

  std::string expected_sent_text;
  gfx::Range expected_sent_range;
  if (GetApiVersion() == wl::TestZcrTextInputExtensionV1::Version::kV8) {
    // In old protocol, the text sent as wayland protocol must be at most 4000
    // byte and large enough in the limitation.
    expected_sent_text = base::UTF16ToUTF8(std::u16string(1333, u'あ'));
    // The selection range must be relocated accordingly to the sent text.
    expected_sent_range = gfx::Range(0, 1500);
  } else {
    // In the new protocol, whole selection range + at most 500 bytes buffers
    // are sent.
    expected_sent_text = base::UTF16ToUTF8(std::u16string(666, u'あ'));
    expected_sent_range = gfx::Range(0, 1500);
  }

  PostToServerAndWait([expected_sent_text, expected_sent_range](
                          wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText(expected_sent_text, expected_sent_range))
        .Times(1);
  });

  input_method_context_->SetSurroundingText(text, gfx::Range(0, 5000),
                                            gfx::Range::InvalidRange(), range,
                                            std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            range);
  connection_->Flush();

  PostToServerAndWait(
      [expected_sent_range](wl::TestWaylandServerThread* server) {
        auto* text_input = server->text_input_manager_v1()->text_input();
        Mock::VerifyAndClearExpectations(text_input);

        // Test OnDeleteSurroundingText with this input.
        zwp_text_input_v1_send_delete_surrounding_text(
            text_input->resource(), expected_sent_range.start(),
            expected_sent_range.length());
      });

  EXPECT_EQ(
      input_method_context_delegate_->last_on_delete_surrounding_text_args(),
      (std::pair<size_t, size_t>(0, 0)));
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      std::u16string(4500, u'あ'));
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(0));
}

TEST_P(WaylandInputMethodContextTest,
       SetSurroundingTextForLongTextInRightEdge) {
  const std::u16string text(5000, u'あ');
  constexpr gfx::Range range(4500, 5000);

  std::string expected_sent_text;
  gfx::Range expected_sent_range;
  if (GetApiVersion() == wl::TestZcrTextInputExtensionV1::Version::kV8) {
    // In the old protocol, the text sent as wayland protocol must be at most
    // 4000 byte and large enough in the limitation.
    expected_sent_text = base::UTF16ToUTF8(std::u16string(1333, u'あ'));
    // The selection range must be relocated accordingly to the sent text.
    expected_sent_range = gfx::Range(2499, 3999);
  } else {
    // In the new protocol, whole selection + at most 500 bytes buffers are
    // sent.
    expected_sent_text = base::UTF16ToUTF8(std::u16string(666, u'あ'));
    expected_sent_range = gfx::Range(498, 1998);
  }

  PostToServerAndWait([expected_sent_text, expected_sent_range](
                          wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText(expected_sent_text, expected_sent_range))
        .Times(1);
  });

  input_method_context_->SetSurroundingText(text, gfx::Range(0, 5000),
                                            gfx::Range::InvalidRange(), range,
                                            std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            range);
  connection_->Flush();

  PostToServerAndWait(
      [expected_sent_range](wl::TestWaylandServerThread* server) {
        auto* text_input = server->text_input_manager_v1()->text_input();
        Mock::VerifyAndClearExpectations(text_input);

        // Test OnDeleteSurroundingText with this input.
        zwp_text_input_v1_send_delete_surrounding_text(
            text_input->resource(), expected_sent_range.start(),
            expected_sent_range.length());
      });

  EXPECT_EQ(
      input_method_context_delegate_->last_on_delete_surrounding_text_args(),
      (std::pair<size_t, size_t>(0, 0)));
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      std::u16string(4500, u'あ'));
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(4500));
}

// TODO(crbug.com/354862211): This test is Lacros-specific and should be
// removed. It fails without Lacros-specific patches to libwayland, which
// shouldn't be applied when updating to a new Wayland of version as Lacros is
// being sunset.
TEST_P(WaylandInputMethodContextTest, DISABLED_SetSurroundingTextForLongRange) {
  const std::u16string text(5000, u'あ');
  constexpr gfx::Range range(1000, 4000);

  if (GetApiVersion() == wl::TestZcrTextInputExtensionV1::Version::kV8) {
    // set_surrounding_text request should be skipped when the selection range
    // in UTF8 form is longer than 4000 byte.
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                  SetSurroundingText(_, _))
          .Times(0);
    });

    input_method_context_->SetSurroundingText(text, gfx::Range(0, 5000),
                                              gfx::Range::InvalidRange(), range,
                                              std::nullopt, std::nullopt);
    // Predicted state in SurroundingTextTracker is reset when the range is
    // longer than wayland message size maximum.
    EXPECT_EQ(
        input_method_context_->predicted_state_for_testing().surrounding_text,
        u"");
    EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
              gfx::Range(0));
    connection_->Flush();

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      Mock::VerifyAndClearExpectations(
          server->text_input_manager_v1()->text_input());
    });
  } else {
    // In the new protocol, we can send large selection range.
    const std::string kExpectedSentText =
        base::UTF16ToUTF8(std::u16string(3332, u'あ'));
    constexpr gfx::Range kExpectedSentRange(498, 9498);

    PostToServerAndWait([kExpectedSentText, kExpectedSentRange](
                            wl::TestWaylandServerThread* server) {
      EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                  SetSurroundingText(kExpectedSentText, kExpectedSentRange))
          .Times(1);
    });

    input_method_context_->SetSurroundingText(text, gfx::Range(0, 5000),
                                              gfx::Range::InvalidRange(), range,
                                              std::nullopt, std::nullopt);
    EXPECT_EQ(
        input_method_context_->predicted_state_for_testing().surrounding_text,
        text);
    EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
              range);
    connection_->Flush();

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      Mock::VerifyAndClearExpectations(
          server->text_input_manager_v1()->text_input());
    });
  }
}

TEST_P(WaylandInputMethodContextTest,
       SetSurroundingTextForShortTextWithGrammmarFragment) {
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
    EXPECT_CALL(*server->text_input_extension_v1()->extended_text_input(),
                SetGrammarFragmentAtCursor(gfx::Range(0, 30), "abc"))
        .Times(1);
  });

  input_method_context_->SetSurroundingText(
      text, gfx::Range(0, 50), gfx::Range::InvalidRange(), range,
      GrammarFragment(gfx::Range(0, 10), "abc"), std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            range);
  connection_->Flush();
}

TEST_P(WaylandInputMethodContextTest,
       SetSurroundingTextForLongTextWithGrammmarFragment) {
  const std::u16string text(5000, u'あ');
  constexpr gfx::Range range(2800, 3200);

  std::string expected_sent_text;
  gfx::Range expected_sent_range;
  gfx::Range expected_fragment_range;
  if (GetApiVersion() == wl::TestZcrTextInputExtensionV1::Version::kV8) {
    // In the old protocol, the text sent as wayland protocol must be at
    // most 4000 byte and long enough in the limitation.
    expected_sent_text = base::UTF16ToUTF8(std::u16string(1332, u'あ'));
    // The selection range must be relocated accordingly to the sent text.
    expected_sent_range = gfx::Range(1398, 2598);
    expected_fragment_range = gfx::Range(1098, 1128);
  } else {
    // In the new protocol, whole selection range and grammar fragment are
    // sent with at most 500 bytes buffer.
    expected_sent_text = base::UTF16ToUTF8(std::u16string(832, u'あ'));
    expected_sent_range = gfx::Range(798, 1998);
    expected_fragment_range = gfx::Range(498, 528);
  }

  PostToServerAndWait(
      [expected_sent_text, expected_sent_range,
       expected_fragment_range](wl::TestWaylandServerThread* server) {
        EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                    SetSurroundingText(expected_sent_text, expected_sent_range))
            .Times(1);
        EXPECT_CALL(*server->text_input_extension_v1()->extended_text_input(),
                    SetGrammarFragmentAtCursor(expected_fragment_range, "abc"))
            .Times(1);
      });

  input_method_context_->SetSurroundingText(
      text, gfx::Range(0, 5000), gfx::Range::InvalidRange(), range,
      GrammarFragment(gfx::Range(2700, 2710), "abc"), std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            range);
  connection_->Flush();
}

TEST_P(WaylandInputMethodContextTest,
       SetSurroundingTextForShortTextWithAutocorrect) {
  const std::u16string text(50, u'あ');
  constexpr gfx::Range range(20, 30);

  const std::string kExpectedSentText(base::UTF16ToUTF8(text));
  constexpr gfx::Range kExpectedSentRange(60, 90);

  PostToServerAndWait([this, kExpectedSentText, kExpectedSentRange](
                          wl::TestWaylandServerThread* server) {
    // The text and range sent as wayland protocol must be same to the original
    // text and range where the original text is shorter than 4000 byte.
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText(kExpectedSentText, kExpectedSentRange))
        .Times(1);
    gfx::Range autocorrect_range;
    if (GetApiVersion() == wl::TestZcrTextInputExtensionV1::Version::kV8) {
      // In older protocol, the autocorrection range is not converted.
      autocorrect_range = gfx::Range(15, 18);
    } else {
      // In new protocol, it is byte offsets within the surrounding text.
      autocorrect_range = gfx::Range(45, 54);
    }

    EXPECT_CALL(*server->text_input_extension_v1()->extended_text_input(),
                SetAutocorrectInfo(autocorrect_range, gfx::Rect(10, 20)));
  });

  input_method_context_->SetSurroundingText(
      text, gfx::Range(0, 50), gfx::Range::InvalidRange(), range, std::nullopt,
      AutocorrectInfo{gfx::Range(15, 18), gfx::Rect(10, 20)});
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            range);
  connection_->Flush();
}

TEST_P(WaylandInputMethodContextTest, DeleteSurroundingTextWithExtendedRange) {
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

  input_method_context_->SetSurroundingText(text, gfx::Range(0, 5000),
                                            gfx::Range::InvalidRange(), range,
                                            std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            range);
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
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      std::u16string(38, u'あ'));
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(19));
}

TEST_P(WaylandInputMethodContextTest, DeleteSurroundingTextInIncorrectOrder) {
  // This test aims to check the scenario where OnDeleteSurroundingText event is
  // not received in correct order due to the timing issue.

  constexpr char16_t text[] = u"aあb";
  const gfx::Range range(3);

  input_method_context_->SetSurroundingText(text, gfx::Range(0, 3),
                                            gfx::Range::InvalidRange(), range,
                                            std::nullopt, std::nullopt);
  connection_->Flush();

  // 1. Delete the second character 'b'.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(text_input);

    zwp_text_input_v1_send_delete_surrounding_text(text_input->resource(), 4,
                                                   1);
  });
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"aあ");
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(2));

  // 2. Delete the third character 'あ'.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(text_input);

    zwp_text_input_v1_send_delete_surrounding_text(text_input->resource(), 1,
                                                   3);
  });

  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"a");
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(1));

  // 3. Set surrounding text for step 1. Ideally this thould be called before
  // step 2, but the order could be different due to the timing issue.
  input_method_context_->SetSurroundingText(
      u"aあ", gfx::Range(0, 2), gfx::Range::InvalidRange(), gfx::Range(2),
      std::nullopt, std::nullopt);
  connection_->Flush();

  // Surrounding text tracker should predict "a" instead of "aあ" here as that
  // is the correct state on server. On setting "aあ" as a surrounding text,
  // surrounding text tracker looks up the expected state queue and consumes the
  // state of "aあ" .
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"a");
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(1));

  // 4. Set surrounding text for step 2.
  input_method_context_->SetSurroundingText(
      u"a", gfx::Range(0, 1), gfx::Range::InvalidRange(), gfx::Range(1),
      std::nullopt, std::nullopt);
  connection_->Flush();

  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"a");
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(1));
}

TEST_P(WaylandInputMethodContextTest,
       DeleteSurroundingTextAndCommitInIncorrectOrder) {
  // This test aims to check the scenario where SetSurroundingText event is
  // received from application later than receiving delete/commit event from
  // server.

  // 1. Set CommitString as a initial state. Cursor is between "Commit" and
  // "String".
  input_method_context_->SetSurroundingText(
      u"CommitString", gfx::Range(0, 12), gfx::Range::InvalidRange(),
      gfx::Range(6), std::nullopt, std::nullopt);
  connection_->Flush();

  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"CommitString");
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(6));

  // 2. Delete surrounding text for "Commit" received.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(text_input);

    zwp_text_input_v1_send_delete_surrounding_text(text_input->resource(), 0,
                                                   6);
  });

  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"String");
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(0));

  // 3. Commit for "Updated" received.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(text_input);

    zwp_text_input_v1_send_commit_string(
        server->text_input_manager_v1()->text_input()->resource(),
        server->GetNextSerial(), "Updated");
  });

  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"UpdatedString");
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(7));

  // 4. Set surrounding text for step 2. Ideally this should be sent before step
  // 3.
  input_method_context_->SetSurroundingText(
      u"String", gfx::Range(0, 6), gfx::Range::InvalidRange(), gfx::Range(0),
      std::nullopt, std::nullopt);
  connection_->Flush();

  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"UpdatedString");
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(7));

  // 5. Set surrounding text for step 3.
  input_method_context_->SetSurroundingText(
      u"UpdatedString", gfx::Range(0, 13), gfx::Range::InvalidRange(),
      gfx::Range(7), std::nullopt, std::nullopt);
  connection_->Flush();

  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"UpdatedString");
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(7));
}

TEST_P(WaylandInputMethodContextTest, SetInputType) {
  connection_->window_manager()->SetKeyboardFocusedWindow(window_.get());
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(
        *server->text_input_extension_v1()->extended_text_input(),
        SetInputType(
            ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_TYPE_URL,
            ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_MODE_DEFAULT,
            ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_FLAGS_AUTOCOMPLETE_ON,
            ZCR_EXTENDED_TEXT_INPUT_V1_LEARNING_MODE_ENABLED,
            ZCR_EXTENDED_TEXT_INPUT_V1_INLINE_COMPOSITION_SUPPORT_SUPPORTED))
        .Times(1);
  });
  LinuxInputMethodContext::TextInputClientAttributes attributes;
  attributes.input_type = TEXT_INPUT_TYPE_URL;
  attributes.input_mode = TEXT_INPUT_MODE_DEFAULT;
  attributes.flags = TEXT_INPUT_FLAG_AUTOCOMPLETE_ON;
  attributes.should_do_learning = true;
  attributes.can_compose_inline = true;

  input_method_context_->UpdateFocus(
      /*has_client=*/true, TEXT_INPUT_TYPE_NONE, attributes,
      TextInputClient::FOCUS_REASON_OTHER);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_extension_v1()->extended_text_input());
  });
}

TEST_P(WaylandInputMethodContextTest, SetInputTypeWithoutLearning) {
  connection_->window_manager()->SetKeyboardFocusedWindow(window_.get());
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(
        *server->text_input_extension_v1()->extended_text_input(),
        SetInputType(
            ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_TYPE_URL,
            ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_MODE_DEFAULT,
            ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_FLAGS_AUTOCOMPLETE_ON,
            ZCR_EXTENDED_TEXT_INPUT_V1_LEARNING_MODE_DISABLED,
            ZCR_EXTENDED_TEXT_INPUT_V1_INLINE_COMPOSITION_SUPPORT_SUPPORTED))
        .Times(1);
  });
  LinuxInputMethodContext::TextInputClientAttributes attributes;
  attributes.input_type = TEXT_INPUT_TYPE_URL;
  attributes.input_mode = TEXT_INPUT_MODE_DEFAULT;
  attributes.flags = TEXT_INPUT_FLAG_AUTOCOMPLETE_ON;
  attributes.should_do_learning = false;
  attributes.can_compose_inline = true;

  input_method_context_->UpdateFocus(
      /*has_client=*/true, TEXT_INPUT_TYPE_NONE, attributes,
      TextInputClient::FOCUS_REASON_OTHER);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_extension_v1()->extended_text_input());
  });
}

TEST_P(WaylandInputMethodContextTest,
       SetInputTypeWithoutInlineCompositionSupport) {
  connection_->window_manager()->SetKeyboardFocusedWindow(window_.get());
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(
        *server->text_input_extension_v1()->extended_text_input(),
        SetInputType(
            ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_TYPE_URL,
            ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_MODE_DEFAULT,
            ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_FLAGS_AUTOCOMPLETE_ON,
            ZCR_EXTENDED_TEXT_INPUT_V1_LEARNING_MODE_ENABLED,
            ZCR_EXTENDED_TEXT_INPUT_V1_INLINE_COMPOSITION_SUPPORT_UNSUPPORTED))
        .Times(1);
  });
  LinuxInputMethodContext::TextInputClientAttributes attributes;
  attributes.input_type = TEXT_INPUT_TYPE_URL;
  attributes.input_mode = TEXT_INPUT_MODE_DEFAULT;
  attributes.flags = TEXT_INPUT_FLAG_AUTOCOMPLETE_ON;
  attributes.should_do_learning = true;
  attributes.can_compose_inline = false;

  input_method_context_->UpdateFocus(
      /*has_client=*/true, TEXT_INPUT_TYPE_NONE, attributes,
      TextInputClient::FOCUS_REASON_OTHER);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_extension_v1()->extended_text_input());
  });
}

TEST_P(WaylandInputMethodContextTest, SetInputTypeAfterFocus) {
  LinuxInputMethodContext::TextInputClientAttributes attributes;
  attributes.input_type = TEXT_INPUT_TYPE_URL;
  attributes.input_mode = TEXT_INPUT_MODE_DEFAULT;
  attributes.flags = TEXT_INPUT_FLAG_AUTOCOMPLETE_ON;
  attributes.should_do_learning = true;
  attributes.can_compose_inline = false;

  input_method_context_->UpdateFocus(
      /*has_client=*/true, TEXT_INPUT_TYPE_NONE, attributes,
      TextInputClient::FOCUS_REASON_OTHER);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_extension_v1()->extended_text_input());
  });

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(
        *server->text_input_extension_v1()->extended_text_input(),
        SetInputType(
            ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_TYPE_URL,
            ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_MODE_DEFAULT,
            ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_FLAGS_AUTOCOMPLETE_ON,
            ZCR_EXTENDED_TEXT_INPUT_V1_LEARNING_MODE_ENABLED,
            ZCR_EXTENDED_TEXT_INPUT_V1_INLINE_COMPOSITION_SUPPORT_UNSUPPORTED))
        .Times(1);
  });

  connection_->window_manager()->SetKeyboardFocusedWindow(window_.get());
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_extension_v1()->extended_text_input());
  });
}

TEST_P(WaylandInputMethodContextTest, OnPreeditChanged) {
  constexpr std::string_view kPreeditString("PreeditString");
  constexpr gfx::Range kSelection{7, 13};
  input_method_context_->OnPreeditString(
      kPreeditString,
      {{0,
        static_cast<uint32_t>(kPreeditString.size()),
        {{ImeTextSpan::Type::kComposition, ImeTextSpan::Thickness::kThin}}}},
      kSelection);
  EXPECT_TRUE(input_method_context_delegate_->was_on_preedit_changed_called());
  EXPECT_EQ(input_method_context_delegate_->last_preedit()->ime_text_spans,
            ImeTextSpans{ImeTextSpan(ImeTextSpan::Type::kComposition, 0,
                                     kPreeditString.size(),
                                     ImeTextSpan::Thickness::kThin)});
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"PreeditString");
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().composition,
            gfx::Range(0, kPreeditString.size()));
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            kSelection);
}

TEST_P(WaylandInputMethodContextTest, OnCommit) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    zwp_text_input_v1_send_commit_string(
        server->text_input_manager_v1()->text_input()->resource(),
        server->GetNextSerial(), "CommitString");
  });
  EXPECT_TRUE(input_method_context_delegate_->was_on_commit_called());
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"CommitString");
  // On commit string, selection is placed next to the last character unless the
  // cursor position is specified by OnCursorPosition.
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(12));
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().composition,
            gfx::Range(0));
}

// Regression test for crbug.com/40263583
TEST_P(WaylandInputMethodContextTest,
       OnCommitAfterEmptyPreeditStringWithoutCursor) {
  input_method_context_->OnPreeditString("", {}, gfx::Range::InvalidRange());
  EXPECT_TRUE(input_method_context_delegate_->was_on_preedit_changed_called());
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"");
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().composition,
            gfx::Range(0, 0));
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(0));
  input_method_context_->OnCommitString("CommitString");
  EXPECT_TRUE(input_method_context_delegate_->was_on_commit_called());
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"CommitString");
  // On commit string, selection is placed next to the last character unless the
  // cursor position is specified by OnCursorPosition.
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(12));
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().composition,
            gfx::Range(0));
}

TEST_P(WaylandInputMethodContextTest, OnCommitAfterPreeditStringWithoutCursor) {
  input_method_context_->OnPreeditString("PreeditString", {},
                                         gfx::Range::InvalidRange());
  EXPECT_TRUE(input_method_context_delegate_->was_on_preedit_changed_called());
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"PreeditString");
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().composition,
            gfx::Range(0, 13));
  // Cursor should be at the end of preedit when cursor position is not
  // specified.
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(13));
  input_method_context_->OnCommitString("CommitString");
  EXPECT_TRUE(input_method_context_delegate_->was_on_commit_called());
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"CommitString");
  // On commit string, selection is placed next to the last character unless the
  // cursor position is specified by OnCursorPosition.
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(12));
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().composition,
            gfx::Range(0));
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_P(WaylandInputMethodContextTest, OnConfirmCompositionText) {
  constexpr char16_t text[] = u"ab😀cあdef";
  constexpr gfx::Range range(5, 6);  // あ is selected.

  // SetSurroundingText should be called in UTF-8.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText("ab😀cあdef", gfx::Range(7, 10)));
  });
  input_method_context_->SetSurroundingText(text, gfx::Range(0, 9),
                                            gfx::Range::InvalidRange(), range,
                                            std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            range);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(text_input);

    const gfx::Range sent_range(10, 7);
    zwp_text_input_v1_send_cursor_position(
        text_input->resource(), sent_range.start(), sent_range.end());
    zwp_text_input_v1_send_commit_string(text_input->resource(), 0,
                                         "ab😀cあdef");
  });

  EXPECT_THAT(input_method_context_delegate_->last_on_confirm_composition_arg(),
              Optional(true));
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  // Cursor position is set to `range` position explicitly by OnCursorPosition.
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            range);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().composition,
            gfx::Range(0));
}

TEST_P(WaylandInputMethodContextTest,
       OnConfirmCompositionTextExtendedKeepSelectionNoComposition) {
  input_method_context_->SetSurroundingText(
      u"abcd", gfx::Range(0, 4), gfx::Range::InvalidRange(), gfx::Range(0, 4),
      std::nullopt, std::nullopt);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    zcr_extended_text_input_v1_send_confirm_preedit(
        server->text_input_extension_v1()->extended_text_input()->resource(),
        /*selection_behavior=*/
        ZCR_EXTENDED_TEXT_INPUT_V1_CONFIRM_PREEDIT_SELECTION_BEHAVIOR_UNCHANGED);
  });

  EXPECT_THAT(input_method_context_delegate_->last_on_confirm_composition_arg(),
              Optional(true));
  // Selection range should not be changed.
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(0, 4));
}

TEST_P(WaylandInputMethodContextTest,
       OnConfirmCompositionTextExtendedKeepSelectionComposition) {
  input_method_context_->SetSurroundingText(
      u"abcd", gfx::Range(0, 4), gfx::Range::InvalidRange(), gfx::Range(2),
      std::nullopt, std::nullopt);
  input_method_context_->OnPreeditString("xyz", {}, gfx::Range(1));
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    zcr_extended_text_input_v1_send_confirm_preedit(
        server->text_input_extension_v1()->extended_text_input()->resource(),
        /*selection_behavior=*/
        ZCR_EXTENDED_TEXT_INPUT_V1_CONFIRM_PREEDIT_SELECTION_BEHAVIOR_UNCHANGED);
  });

  EXPECT_THAT(input_method_context_delegate_->last_on_confirm_composition_arg(),
              Optional(true));
  // Selection range should not be changed.
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(3));
}

TEST_P(WaylandInputMethodContextTest,
       OnConfirmCompositionTextExtendedDontKeepSelectionNoComposition) {
  input_method_context_->SetSurroundingText(
      u"abcd", gfx::Range(0, 4), gfx::Range::InvalidRange(), gfx::Range(0, 4),
      std::nullopt, std::nullopt);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    zcr_extended_text_input_v1_send_confirm_preedit(
        server->text_input_extension_v1()->extended_text_input()->resource(),
        /*selection_behavior=*/
        ZCR_EXTENDED_TEXT_INPUT_V1_CONFIRM_PREEDIT_SELECTION_BEHAVIOR_AFTER_PREEDIT);
  });

  EXPECT_THAT(input_method_context_delegate_->last_on_confirm_composition_arg(),
              Optional(false));
  // Selection range should not be changed.
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(0, 4));
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().composition,
            gfx::Range(0));
}

TEST_P(WaylandInputMethodContextTest,
       OnConfirmCompositionTextExtendedDontKeepSelectionComposition) {
  input_method_context_->SetSurroundingText(
      u"abcd", gfx::Range(0, 4), gfx::Range::InvalidRange(), gfx::Range(2),
      std::nullopt, std::nullopt);
  input_method_context_->OnPreeditString("xyz", {}, gfx::Range(1));
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    zcr_extended_text_input_v1_send_confirm_preedit(
        server->text_input_extension_v1()->extended_text_input()->resource(),
        /*selection_behavior=*/
        ZCR_EXTENDED_TEXT_INPUT_V1_CONFIRM_PREEDIT_SELECTION_BEHAVIOR_AFTER_PREEDIT);
  });

  EXPECT_THAT(input_method_context_delegate_->last_on_confirm_composition_arg(),
              Optional(false));
  // Selection range should move to the end of commit.
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(5));
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().composition,
            gfx::Range(0));
}

TEST_P(WaylandInputMethodContextTest, OnConfirmCompositionTextForLongRange) {
  const std::u16string text(5000, u'あ');
  constexpr gfx::Range range(4000, 4500);

  std::string expected_sent_text;
  gfx::Range expected_sent_range;
  if (GetApiVersion() == wl::TestZcrTextInputExtensionV1::Version::kV8) {
    // In old protocol, even if the range covers, the surrounding text
    // longer than 4000 bytes is trimmed to meet the limitation.
    // Selection range is also adjusted by the trimmed text before sendin to
    // Exo.
    expected_sent_text = base::UTF16ToUTF8(std::u16string(1332, u'あ'));
    expected_sent_range = gfx::Range(1248, 2748);
  } else {
    // In new protocol, the surrounding text is trimmed around selection with
    // at most 500 bytes buffer.
    expected_sent_text = base::UTF16ToUTF8(std::u16string(832, u'あ'));
    expected_sent_range = gfx::Range(498, 1998);
  }

  // SetSurroundingText should be called in UTF-8.
  PostToServerAndWait([expected_sent_text, expected_sent_range](
                          wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText(expected_sent_text, expected_sent_range));
  });
  input_method_context_->SetSurroundingText(text, gfx::Range(0, 5000),
                                            gfx::Range::InvalidRange(), range,
                                            std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            range);
  connection_->Flush();

  PostToServerAndWait([expected_sent_text, expected_sent_range](
                          wl::TestWaylandServerThread* server) {
    auto* text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(text_input);

    gfx::Range range =
        gfx::Range(expected_sent_range.end(), expected_sent_range.start());

    zwp_text_input_v1_send_cursor_position(text_input->resource(),
                                           range.start(), range.end());
    zwp_text_input_v1_send_commit_string(text_input->resource(), 0,
                                         expected_sent_text.c_str());
  });

  EXPECT_THAT(input_method_context_delegate_->last_on_confirm_composition_arg(),
              Optional(true));
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  // Cursor position is set to `range` position explicitly by OnCursorPosition.
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            range);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().composition,
            gfx::Range(0));
}
#endif

TEST_P(WaylandInputMethodContextTest, OnSetPreeditRegion_Success) {
  constexpr char16_t text[] = u"abcあdef";
  const gfx::Range range(3, 4);  // あ is selected.

  // SetSurroundingText should be called in UTF-8.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                SetSurroundingText("abcあdef", gfx::Range(3, 6)));
  });

  input_method_context_->SetSurroundingText(text, gfx::Range(0, 7),
                                            gfx::Range::InvalidRange(), range,
                                            std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            range);
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
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().composition,
            gfx::Range(2, 5));
}

TEST_P(WaylandInputMethodContextTest, OnSetPreeditRegion_NoSurroundingText) {
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
TEST_P(WaylandInputMethodContextTest,
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

  input_method_context_->SetSurroundingText(
      u16_text, gfx::Range(0, 1), gfx::Range::InvalidRange(), u16_range,
      std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u16_text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            u16_range);
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
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().composition,
            u16_range);
}

TEST_P(WaylandInputMethodContextTest,
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

  input_method_context_->SetSurroundingText(
      u16_text, gfx::Range(0, 2), gfx::Range::InvalidRange(), u16_range,
      std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u16_text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            u16_range);
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
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().composition,
            u16_range);
}

TEST_P(WaylandInputMethodContextTest, OnClearGrammarFragments) {
  input_method_context_->OnClearGrammarFragments(gfx::Range(1, 5));
  WaylandTestBase::SyncDisplay();
  EXPECT_TRUE(
      input_method_context_delegate_->was_on_clear_grammar_fragments_called());
}

TEST_P(WaylandInputMethodContextTest, OnAddGrammarFragments) {
  input_method_context_->OnAddGrammarFragment(
      ui::GrammarFragment(gfx::Range(1, 5), "test"));
  WaylandTestBase::SyncDisplay();
  EXPECT_TRUE(
      input_method_context_delegate_->was_on_add_grammar_fragment_called());
}

TEST_P(WaylandInputMethodContextTest, OnInsertImage) {
  const GURL some_image_url = GURL("");
  input_method_context_->OnInsertImage(some_image_url);
  WaylandTestBase::SyncDisplay();
  EXPECT_TRUE(input_method_context_delegate_->was_on_insert_image_called());
}

TEST_P(WaylandInputMethodContextTest, OnSetAutocorrectRange) {
  input_method_context_->OnSetAutocorrectRange(gfx::Range(1, 5));
  WaylandTestBase::SyncDisplay();
  EXPECT_TRUE(
      input_method_context_delegate_->was_on_set_autocorrect_range_called());
}

TEST_P(WaylandInputMethodContextTest, OnSetVirtualKeyboardOccludedBounds) {
  constexpr gfx::Rect kBounds(10, 20, 300, 400);
  input_method_context_->OnSetVirtualKeyboardOccludedBounds(kBounds);
  WaylandTestBase::SyncDisplay();
  EXPECT_EQ(input_method_context_delegate_->virtual_keyboard_bounds(), kBounds);
}

TEST_P(WaylandInputMethodContextTest,
       OnSetVirtualKeyboardOccludedBoundsUpdatesPastTextInputClients) {
  auto client1 = std::make_unique<MockTextInputClient>(TEXT_INPUT_TYPE_TEXT);
  auto client2 = std::make_unique<MockTextInputClient>(TEXT_INPUT_TYPE_URL);

  input_method_context_->WillUpdateFocus(client1.get(), client2.get());
  LinuxInputMethodContext::TextInputClientAttributes attributes;
  attributes.input_type = client2->GetTextInputType();
  input_method_context_->UpdateFocus(true, client1->GetTextInputType(),
                                     attributes,
                                     ui::TextInputClient::FOCUS_REASON_OTHER);
  input_method_context_->WillUpdateFocus(client2.get(), nullptr);
  attributes.input_type = TEXT_INPUT_TYPE_NONE;
  input_method_context_->UpdateFocus(false, client2->GetTextInputType(),
                                     attributes,
                                     ui::TextInputClient::FOCUS_REASON_NONE);

  // Clients should get further bounds updates.
  constexpr gfx::Rect kBounds(10, 20, 300, 400);
  EXPECT_CALL(*client1, EnsureCaretNotInRect(kBounds));
  EXPECT_CALL(*client2, EnsureCaretNotInRect(kBounds));
  input_method_context_->OnSetVirtualKeyboardOccludedBounds(kBounds);
  WaylandTestBase::SyncDisplay();
  Mock::VerifyAndClearExpectations(client1.get());
  Mock::VerifyAndClearExpectations(client2.get());

  // Clients should get the empty bounds then be removed.
  const gfx::Rect kBoundsEmpty(0, 30, 0, 0);
  EXPECT_CALL(*client1, EnsureCaretNotInRect(kBoundsEmpty));
  EXPECT_CALL(*client2, EnsureCaretNotInRect(kBoundsEmpty));
  input_method_context_->OnSetVirtualKeyboardOccludedBounds(kBoundsEmpty);
  WaylandTestBase::SyncDisplay();
  Mock::VerifyAndClearExpectations(client1.get());
  Mock::VerifyAndClearExpectations(client2.get());

  // Verify client no longer gets bounds updates.
  const gfx::Rect kBounds2(0, 40, 100, 200);
  EXPECT_CALL(*client1, EnsureCaretNotInRect).Times(0);
  EXPECT_CALL(*client2, EnsureCaretNotInRect).Times(0);
  input_method_context_->OnSetVirtualKeyboardOccludedBounds(kBounds2);
  WaylandTestBase::SyncDisplay();
  Mock::VerifyAndClearExpectations(client1.get());
  Mock::VerifyAndClearExpectations(client2.get());
}

TEST_P(WaylandInputMethodContextTest,
       OnSetVirtualKeyboardOccludedBoundsWithDeletedPastTextInputClient) {
  auto client = std::make_unique<MockTextInputClient>(TEXT_INPUT_TYPE_TEXT);

  input_method_context_->WillUpdateFocus(client.get(), nullptr);
  LinuxInputMethodContext::TextInputClientAttributes attributes;
  attributes.input_type = TEXT_INPUT_TYPE_NONE;
  input_method_context_->UpdateFocus(false, client->GetTextInputType(),
                                     attributes,
                                     ui::TextInputClient::FOCUS_REASON_NONE);

  const gfx::Rect kBounds(10, 20, 300, 400);
  EXPECT_CALL(*client, EnsureCaretNotInRect(kBounds));
  input_method_context_->OnSetVirtualKeyboardOccludedBounds(kBounds);
  WaylandTestBase::SyncDisplay();
  Mock::VerifyAndClearExpectations(client.get());

  client.reset();
  input_method_context_->OnSetVirtualKeyboardOccludedBounds(kBounds);
  WaylandTestBase::SyncDisplay();
}

TEST_P(WaylandInputMethodContextTest, DisplayVirtualKeyboard) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                ShowInputPanel())
        .Times(1);
  });
  EXPECT_TRUE(input_method_context_->DisplayVirtualKeyboard());
  connection_->Flush();
  WaylandTestBase::SyncDisplay();
}

TEST_P(WaylandInputMethodContextTest, DismissVirtualKeyboard) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->text_input_manager_v1()->text_input(),
                HideInputPanel());
  });
  input_method_context_->DismissVirtualKeyboard();
  connection_->Flush();
  WaylandTestBase::SyncDisplay();
}

TEST_P(WaylandInputMethodContextTest, UpdateVirtualKeyboardState) {
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

TEST_P(WaylandInputMethodContextTest, OnKeySym) {
#if BUILDFLAG(USE_XKBCOMMON)
  MaybeSetUpXkb();

  uint32_t test_timestamp = 100;
  input_method_context_->OnKeysym(
      XKB_KEY_Shift_L, wl_keyboard_key_state::WL_KEYBOARD_KEY_STATE_PRESSED, 0,
      test_timestamp);

  ASSERT_EQ(wl::EventMillisecondsToTimeTicks(test_timestamp),
            keyboard_delegate_->last_event_timestamp());
#endif
}

namespace {

std::unique_ptr<KeyEvent> CreateKeyEventForCharacterComposer(
    KeyboardCode keyboard_code,
    DomCode dom_code,
    DomKey dom_key) {
  auto event =
      std::make_unique<KeyEvent>(EventType::kKeyPressed, keyboard_code,
                                 dom_code, EF_NONE, dom_key, EventTimeForNow());
  // We need to set this flag to make sure the event is sent to
  // CharacterComposer.
  ui::SetKeyboardImeFlags(event.get(), ui::kPropertyKeyboardImeIgnoredFlag);
  return event;
}

}  // namespace

TEST_P(WaylandInputMethodContextTest, CharacterComposerPreeditStringDeadKey) {
  const char16_t kCombiningAcute = 0x0301;

  auto event = CreateKeyEventForCharacterComposer(
      VKEY_UNKNOWN, DomCode::NONE,
      DomKey::DeadKeyFromCombiningCharacter(kCombiningAcute));
  EXPECT_TRUE(input_method_context_->DispatchKeyEvent(*event));
  EXPECT_TRUE(input_method_context_delegate_->was_on_preedit_changed_called());

  // Preedit string in sequence mode (i.e. using dead keys or the compose key)
  // should only be enabled on Linux ozone/wayland. Everywhere else, the preedit
  // string should always be empty.
#if BUILDFLAG(IS_LINUX)
  // The preedit string should be the non-combining variant of the dead key.
  const char16_t kAcute = 0x00B4;
  std::u16string preedit_string(1, kAcute);
#else
  std::u16string preedit_string = u"";
#endif  // BUILDFLAG(IS_LINUX)
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      preedit_string);

  event = CreateKeyEventForCharacterComposer(VKEY_A, DomCode::US_A,
                                             DomKey::FromCharacter('a'));
  EXPECT_TRUE(input_method_context_->DispatchKeyEvent(*event));
  EXPECT_TRUE(input_method_context_delegate_->was_on_preedit_changed_called());
  EXPECT_TRUE(input_method_context_delegate_->was_on_commit_called());
  // The composed text should be the same on all platforms.
  EXPECT_EQ(input_method_context_delegate_->last_commit_text(), u"á");
}

TEST_P(WaylandInputMethodContextTest,
       CharacterComposerPreeditStringComposeKey) {
  auto event = CreateKeyEventForCharacterComposer(
      VKEY_COMPOSE, DomCode::ALT_RIGHT, DomKey::COMPOSE);
  EXPECT_TRUE(input_method_context_->DispatchKeyEvent(*event));
  EXPECT_TRUE(input_method_context_delegate_->was_on_preedit_changed_called());

#if BUILDFLAG(IS_LINUX)
  std::u16string preedit_string(
      1, ui::CharacterComposer::kPreeditStringComposeKeySymbol);
#else
  std::u16string preedit_string = u"";
#endif  // BUILDFLAG(IS_LINUX)
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      preedit_string);

  event = CreateKeyEventForCharacterComposer(VKEY_OEM_7, DomCode::QUOTE,
                                             DomKey::FromCharacter('\''));
  EXPECT_TRUE(input_method_context_->DispatchKeyEvent(*event));
  EXPECT_TRUE(input_method_context_delegate_->was_on_preedit_changed_called());

#if BUILDFLAG(IS_LINUX)
  preedit_string = u"'";
#endif  // BUILDFLAG(IS_LINUX)
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      preedit_string);

  event = CreateKeyEventForCharacterComposer(VKEY_A, DomCode::US_A,
                                             DomKey::FromCharacter('a'));
  EXPECT_TRUE(input_method_context_->DispatchKeyEvent(*event));
  EXPECT_TRUE(input_method_context_delegate_->was_on_preedit_changed_called());
  EXPECT_TRUE(input_method_context_delegate_->was_on_commit_called());
  // The composed text should be the same on all platforms.
  EXPECT_EQ(input_method_context_delegate_->last_commit_text(), u"á");
}

class WaylandInputMethodContextNoKeyboardTest
    : public WaylandInputMethodContextTest {
 public:
  void SetUp() override {
    // Call the skip base implementation to avoid setting up the keyboard.
    WaylandTest::SetUp();

    ASSERT_FALSE(connection_->seat()->keyboard());

    SetUpInternal();
  }
};

INSTANTIATE_TEST_SUITE_P(TextInputExtensionLatestVersion,
                         WaylandInputMethodContextNoKeyboardTest,
                         ::testing::Values(wl::ServerConfig{}));

TEST_P(WaylandInputMethodContextNoKeyboardTest, ActivateDeactivate) {
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

  LinuxInputMethodContext::TextInputClientAttributes attributes;
  attributes.input_type = TEXT_INPUT_TYPE_TEXT;
  input_method_context_->UpdateFocus(true, ui::TEXT_INPUT_TYPE_NONE, attributes,
                                     ui::TextInputClient::FOCUS_REASON_OTHER);
  connection_->Flush();
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* zwp_text_input = server->text_input_manager_v1()->text_input();
    Mock::VerifyAndClearExpectations(zwp_text_input);

    EXPECT_CALL(*zwp_text_input, HideInputPanel());
    EXPECT_CALL(*zwp_text_input, Deactivate());
  });

  attributes.input_type = TEXT_INPUT_TYPE_NONE;
  input_method_context_->UpdateFocus(false, ui::TEXT_INPUT_TYPE_TEXT,
                                     attributes,
                                     ui::TextInputClient::FOCUS_REASON_NONE);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_manager_v1()->text_input());
  });
}

TEST_P(WaylandInputMethodContextNoKeyboardTest, UpdateFocusBetweenTextFields) {
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

  LinuxInputMethodContext::TextInputClientAttributes attributes;
  attributes.input_type = TEXT_INPUT_TYPE_TEXT;
  input_method_context_->UpdateFocus(true, ui::TEXT_INPUT_TYPE_NONE, attributes,
                                     ui::TextInputClient::FOCUS_REASON_OTHER);
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

  attributes.input_type = TEXT_INPUT_TYPE_TEXT;
  input_method_context_->UpdateFocus(false, ui::TEXT_INPUT_TYPE_TEXT,
                                     attributes,
                                     ui::TextInputClient::FOCUS_REASON_OTHER);
  connection_->Flush();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    Mock::VerifyAndClearExpectations(
        server->text_input_manager_v1()->text_input());
  });
}

// For use in tests that simply test the WaylandInputMethodContext in isolation
// without using a real v1/v3 wrapper.
class WaylandInputMethodContextWithMockWrapperTest : public WaylandTestSimple {
 public:
  void SetUp() override {
    WaylandTestSimple::SetUp();
    input_method_context_delegate_ =
        std::make_unique<TestInputMethodContextDelegate>();
    keyboard_delegate_ = std::make_unique<TestKeyboardDelegate>();
    input_method_context_ = std::make_unique<WaylandInputMethodContext>(
        connection_.get(), keyboard_delegate_.get(),
        input_method_context_delegate_.get());
    auto mock_wrapper = std::make_unique<MockZWPTextInputWrapper>();
    mock_wrapper_ = mock_wrapper.get();
    input_method_context_->Init(
        true, std::move(mock_wrapper),
        // Ensure by default it doesn't pick the current desktop from the system
        // the tests are running on.
        base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_OTHER);
  }

 protected:
  std::unique_ptr<TestInputMethodContextDelegate>
      input_method_context_delegate_;
  std::unique_ptr<TestKeyboardDelegate> keyboard_delegate_;
  std::unique_ptr<WaylandInputMethodContext> input_method_context_;
  raw_ptr<MockZWPTextInputWrapper> mock_wrapper_;
};

TEST_F(WaylandInputMethodContextWithMockWrapperTest,
       SetSurroundingShortTextWithCompositionRange) {
  const std::u16string text(50, u'あ');
  constexpr gfx::Range range(20, 30);

  const std::string kExpectedSentText(base::UTF16ToUTF8(text));
  constexpr gfx::Range kExpectedSentRange(60, 90);

  EXPECT_CALL(*mock_wrapper_,
              SetSurroundingText(kExpectedSentText, kExpectedSentRange,
                                 kExpectedSentRange));
  input_method_context_->SetSurroundingText(text, gfx::Range(0, 50), range,
                                            range, std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            range);
  connection_->Flush();
}

TEST_F(WaylandInputMethodContextWithMockWrapperTest,
       SetSurroundingLongTextWithCompositionRange) {
  const std::u16string text(5000, u'あ');
  constexpr gfx::Range kRange(2800, 3200);

  const std::string kExpectedSentText(
      base::UTF16ToUTF8(std::u16string(1332, u'あ')));
  constexpr gfx::Range kExpectedSentRange(1398, 2598);

  EXPECT_CALL(*mock_wrapper_,
              SetSurroundingText(kExpectedSentText, kExpectedSentRange,
                                 kExpectedSentRange));
  input_method_context_->SetSurroundingText(text, gfx::Range(0, 5000), kRange,
                                            kRange, std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            kRange);
}

TEST_F(WaylandInputMethodContextWithMockWrapperTest,
       SetSurroundingLongTextWithCompositionRangeOutsideSurroundingTextRange) {
  const std::u16string text(5000, u'あ');
  constexpr gfx::Range kSelectionRange(2800, 3200);
  // composition range before surrounding text range.
  constexpr gfx::Range kCompositionRange(10, 20);

  const std::string kExpectedSentText(
      base::UTF16ToUTF8(std::u16string(1332, u'あ')));
  constexpr gfx::Range kExpectedSentSelectionRange(1398, 2598);

  EXPECT_CALL(*mock_wrapper_,
              SetSurroundingText(kExpectedSentText, gfx::Range::InvalidRange(),
                                 kExpectedSentSelectionRange));
  input_method_context_->SetSurroundingText(text, gfx::Range(0, 5000),
                                            kCompositionRange, kSelectionRange,
                                            std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            kSelectionRange);

  // composition range after surrounding text range.
  constexpr gfx::Range kCompositionRange2(4500, 4600);

  EXPECT_CALL(*mock_wrapper_,
              SetSurroundingText(kExpectedSentText, gfx::Range::InvalidRange(),
                                 kExpectedSentSelectionRange));
  input_method_context_->SetSurroundingText(text, gfx::Range(0, 5000),
                                            kCompositionRange2, kSelectionRange,
                                            std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            kSelectionRange);
}

TEST_F(WaylandInputMethodContextWithMockWrapperTest,
       SetSurroundingWithCompositionRangeOutideText) {
  const std::u16string text(5000, u'あ');
  constexpr gfx::Range kSelectionRange(2800, 3200);

  EXPECT_CALL(*mock_wrapper_, SetSurroundingText(_, _, _)).Times(0);
  input_method_context_->SetSurroundingText(
      text, gfx::Range(0, 5000), gfx::Range(6000, 7000), kSelectionRange,
      std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            kSelectionRange);
}

TEST_F(WaylandInputMethodContextWithMockWrapperTest,
       SetSurroundingWithCompositionRangeInvalid) {
  const std::u16string text(5000, u'あ');
  constexpr gfx::Range kSelectionRange(2800, 3200);

  const std::string kExpectedSentText(
      base::UTF16ToUTF8(std::u16string(1332, u'あ')));
  constexpr gfx::Range kExpectedSentSelectionRange(1398, 2598);

  EXPECT_CALL(*mock_wrapper_,
              SetSurroundingText(kExpectedSentText, gfx::Range::InvalidRange(),
                                 kExpectedSentSelectionRange));
  // invalid composition range
  input_method_context_->SetSurroundingText(
      text, gfx::Range(0, 5000), gfx::Range::InvalidRange(), kSelectionRange,
      std::nullopt, std::nullopt);
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            kSelectionRange);
}

TEST_F(WaylandInputMethodContextWithMockWrapperTest, OnPreeditChanged) {
  const std::u16string text(50, u'あ');
  const std::string text_utf8 = base::UTF16ToUTF8(text);

  input_method_context_->OnPreeditString(text_utf8, {}, {30, 60});
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(10, 20));

  // cursor end before begin
  input_method_context_->OnPreeditString(text_utf8, {}, {60, 30});
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(20, 10));
}

TEST_F(WaylandInputMethodContextWithMockWrapperTest,
       OnPreeditChangedInvalidCursorEnd) {
  const std::u16string text(50, u'あ');
  const std::string text_utf8 = base::UTF16ToUTF8(text);

  // Cursor end is outside of preedit text. So neither surrounding text nor
  // selection should be updated.
  input_method_context_->OnPreeditString(text_utf8, {}, {30, 999999});
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"");
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(0, 0));

  // Cursor end is in the middle of a character. So neither surrounding text nor
  // selection should be updated.
  input_method_context_->OnPreeditString(text_utf8, {}, {30, 32});
  EXPECT_EQ(
      input_method_context_->predicted_state_for_testing().surrounding_text,
      u"");
  EXPECT_EQ(input_method_context_->predicted_state_for_testing().selection,
            gfx::Range(0, 0));
}

TEST_F(WaylandInputMethodContextWithMockWrapperTest,
       OnPreeditChangedGnomeWorkaround) {
  const std::u16string text(50, u'あ');
  const std::string text_utf8 = base::UTF16ToUTF8(text);

  // workaround should NOT be used when desktop is not gnome.
  std::unique_ptr<WaylandInputMethodContext> input_method_context;
  input_method_context = std::make_unique<WaylandInputMethodContext>(
      connection_.get(), keyboard_delegate_.get(),
      input_method_context_delegate_.get());
  auto mock_wrapper = std::make_unique<MockZWPTextInputWrapper>();
  input_method_context->Init(true, std::move(mock_wrapper),
                             base::nix::DESKTOP_ENVIRONMENT_KDE3);

  input_method_context->OnPreeditString(text_utf8, {}, {60, 30});
  EXPECT_EQ(
      input_method_context->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context->predicted_state_for_testing().selection,
            gfx::Range(20, 10));

  // workaround should be used when desktop is gnome.
  input_method_context = std::make_unique<WaylandInputMethodContext>(
      connection_.get(), keyboard_delegate_.get(),
      input_method_context_delegate_.get());
  mock_wrapper = std::make_unique<MockZWPTextInputWrapper>();
  input_method_context->Init(true, std::move(mock_wrapper),
                             base::nix::DESKTOP_ENVIRONMENT_GNOME);
  input_method_context->OnPreeditString(text_utf8, {}, {60, 30});
  EXPECT_EQ(
      input_method_context->predicted_state_for_testing().surrounding_text,
      text);
  EXPECT_EQ(input_method_context->predicted_state_for_testing().selection,
            gfx::Range(20, 20));
}

}  // namespace ui
