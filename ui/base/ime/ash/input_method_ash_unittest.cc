// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/input_method_ash.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <queue>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/char_iterator.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/mock_ime_candidate_window_handler.h"
#include "ui/base/ime/ash/mock_ime_engine_handler.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/events.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/test/keyboard_layout.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

using ::base::UTF16ToUTF8;
using ::ui::CompositionText;
using ::ui::FakeTextInputClient;
using ::ui::TextInputClient;

const std::u16string kSampleText = u"あいうえお";

using KeyEventCallback = TextInputMethod::KeyEventDoneCallback;

uint32_t GetOffsetInUTF16(const std::u16string& utf16_string,
                          uint32_t utf8_offset) {
  DCHECK_LT(utf8_offset, utf16_string.size());
  base::i18n::UTF16CharIterator char_iterator(utf16_string);
  for (size_t i = 0; i < utf8_offset; ++i) {
    char_iterator.Advance();
  }
  return char_iterator.array_pos();
}

}  // namespace

class TestableInputMethodAsh : public InputMethodAsh {
 public:
  explicit TestableInputMethodAsh(
      ui::ImeKeyEventDispatcher* ime_key_event_dispatcher)
      : InputMethodAsh(ime_key_event_dispatcher),
        process_key_event_post_ime_call_count_(0) {}

  struct ProcessKeyEventPostIMEArgs {
    ProcessKeyEventPostIMEArgs()
        : event(ui::EventType::kUnknown,
                ui::VKEY_UNKNOWN,
                ui::DomCode::NONE,
                ui::EF_NONE),
          handled_state(ui::ime::KeyEventHandledState::kNotHandled) {}
    ui::KeyEvent event;
    ui::ime::KeyEventHandledState handled_state;
  };

  // Overridden from InputMethodAsh:
  ui::EventDispatchDetails ProcessKeyEventPostIME(
      ui::KeyEvent* key_event,
      ui::ime::KeyEventHandledState handled_state,
      bool stopped_propagation) override {
    ui::EventDispatchDetails details = InputMethodAsh::ProcessKeyEventPostIME(
        key_event, handled_state, stopped_propagation);
    process_key_event_post_ime_args_.event = *key_event;
    process_key_event_post_ime_args_.handled_state = handled_state;
    ++process_key_event_post_ime_call_count_;
    return details;
  }
  void CommitText(
      const std::u16string& text,
      TextInputClient::InsertTextCursorBehavior cursor_behavior) override {
    InputMethodAsh::CommitText(text, cursor_behavior);
    text_committed_ = text;
  }

  void SetEditableSelectionRange(gfx::Range range) {
    GetTextInputClient()->SetEditableSelectionRange(range);
  }

  void ResetCallCount() { process_key_event_post_ime_call_count_ = 0; }

  const ProcessKeyEventPostIMEArgs& process_key_event_post_ime_args() const {
    return process_key_event_post_ime_args_;
  }

  int process_key_event_post_ime_call_count() const {
    return process_key_event_post_ime_call_count_;
  }

  const std::u16string& text_committed() const { return text_committed_; }

  // Change access rights for testing.
  using InputMethodAsh::ExtractCompositionText;
  using InputMethodAsh::ResetContext;

 private:
  ProcessKeyEventPostIMEArgs process_key_event_post_ime_args_;
  int process_key_event_post_ime_call_count_;
  std::u16string text_committed_;
};

class SetSurroundingTextVerifier {
 public:
  SetSurroundingTextVerifier(const std::string& expected_surrounding_text,
                             uint32_t expected_cursor_position,
                             uint32_t expected_anchor_position)
      : expected_surrounding_text_(expected_surrounding_text),
        expected_cursor_position_(expected_cursor_position),
        expected_anchor_position_(expected_anchor_position) {}

  SetSurroundingTextVerifier(const SetSurroundingTextVerifier&) = delete;
  SetSurroundingTextVerifier& operator=(const SetSurroundingTextVerifier&) =
      delete;

  void Verify(const std::string& text,
              uint32_t cursor_pos,
              uint32_t anchor_pos) {
    EXPECT_EQ(expected_surrounding_text_, text);
    EXPECT_EQ(expected_cursor_position_, cursor_pos);
    EXPECT_EQ(expected_anchor_position_, anchor_pos);
  }

 private:
  const std::string expected_surrounding_text_;
  const uint32_t expected_cursor_position_;
  const uint32_t expected_anchor_position_;
};

class TestInputMethodManager : public input_method::MockInputMethodManager {
  class TestState : public MockInputMethodManager::State {
   public:
    TestState() { Reset(); }

    TestState(const TestState&) = delete;
    TestState& operator=(const TestState&) = delete;

    // InputMethodManager::State:
    void ChangeInputMethodToJpKeyboard() override {
      is_jp_kbd_ = true;
      is_jp_ime_ = false;
    }
    void ChangeInputMethodToJpIme() override {
      is_jp_kbd_ = false;
      is_jp_ime_ = true;
    }
    void ToggleInputMethodForJpIme() override {
      if (!is_jp_ime_) {
        is_jp_kbd_ = false;
        is_jp_ime_ = true;
      } else {
        is_jp_kbd_ = true;
        is_jp_ime_ = false;
      }
    }
    void Reset() {
      is_jp_kbd_ = false;
      is_jp_ime_ = false;
    }
    bool is_jp_kbd() const { return is_jp_kbd_; }
    bool is_jp_ime() const { return is_jp_ime_; }

   protected:
    ~TestState() override = default;

   private:
    bool is_jp_kbd_ = false;
    bool is_jp_ime_ = false;
  };

 private:
  scoped_refptr<TestState> state_;

 public:
  TestInputMethodManager() {
    state_ = scoped_refptr<TestState>(new TestState());
  }

  TestInputMethodManager(const TestInputMethodManager&) = delete;
  TestInputMethodManager& operator=(const TestInputMethodManager&) = delete;

  TestState* state() { return state_.get(); }

  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override {
    return scoped_refptr<InputMethodManager::State>(state_.get());
  }
};

class NiceMockIMEEngine : public MockIMEEngineHandler {
 public:
  MOCK_METHOD1(Focus, void(const InputContext&));
  MOCK_METHOD0(Blur, void());
  MOCK_METHOD3(SetSurroundingText,
               void(const std::u16string&, gfx::Range, uint32_t));
};

class InputMethodAshTest : public ui::ImeKeyEventDispatcher,
                           public testing::Test,
                           public ui::DummyTextInputClient {
 public:
  InputMethodAshTest()
      : dispatched_key_event_(ui::EventType::kUnknown,
                              ui::VKEY_UNKNOWN,
                              ui::EF_NONE),
        stop_propagation_post_ime_(false) {
    ResetFlags();
  }

  InputMethodAshTest(const InputMethodAshTest&) = delete;
  InputMethodAshTest& operator=(const InputMethodAshTest&) = delete;

  ~InputMethodAshTest() override = default;

  void SetUp() override {
    mock_ime_engine_handler_ = std::make_unique<MockIMEEngineHandler>();
    IMEBridge::Get()->SetCurrentEngineHandler(mock_ime_engine_handler_.get());

    mock_ime_candidate_window_handler_ =
        std::make_unique<MockIMECandidateWindowHandler>();
    IMEBridge::Get()->SetCandidateWindowHandler(
        mock_ime_candidate_window_handler_.get());

    input_method_ash_ = std::make_unique<TestableInputMethodAsh>(this);
    input_method_ash_->SetFocusedTextInputClient(this);

    // InputMethodManager owns and delete it in InputMethodManager::Shutdown().
    input_method_manager_ = new TestInputMethodManager();
    input_method::InputMethodManager::Initialize(input_method_manager_);
  }

  void TearDown() override {
    if (input_method_ash_.get()) {
      input_method_ash_->SetFocusedTextInputClient(nullptr);
    }
    input_method_ash_.reset();
    IMEBridge::Get()->SetCurrentEngineHandler(nullptr);
    IMEBridge::Get()->SetCandidateWindowHandler(nullptr);
    mock_ime_engine_handler_.reset();
    mock_ime_candidate_window_handler_.reset();
    input_method::InputMethodManager::Shutdown();

    ResetFlags();
  }

  // Overridden from ui::ImeKeyEventDispatcher:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) override {
    dispatched_key_event_ = *event;
    if (stop_propagation_post_ime_) {
      event->StopPropagation();
    }
    return ui::EventDispatchDetails();
  }

  // Overridden from ui::TextInputClient:
  void SetCompositionText(const CompositionText& composition) override {
    composition_text_ = composition;
  }
  size_t ConfirmCompositionText(bool keep_selection) override {
    // TODO(b/134473433) Modify this function so that the selection is not
    // changed when text committed
    NOTIMPLEMENTED_LOG_ONCE();
    confirmed_text_ = composition_text_;
    composition_text_ = CompositionText();
    return confirmed_text_.text.length();
  }
  void ClearCompositionText() override {
    composition_text_ = CompositionText();
  }
  void InsertText(
      const std::u16string& text,
      TextInputClient::InsertTextCursorBehavior cursor_behavior) override {
    inserted_text_ = text;
  }
  void InsertChar(const ui::KeyEvent& event) override {
    inserted_char_ = event.GetCharacter();
    inserted_char_flags_ = event.flags();
  }
  ui::TextInputType GetTextInputType() const override { return input_type_; }
  ui::TextInputMode GetTextInputMode() const override { return input_mode_; }
  bool CanComposeInline() const override { return can_compose_inline_; }
  gfx::Rect GetCaretBounds() const override { return caret_bounds_; }
  bool HasCompositionText() const override {
    return composition_text_ != CompositionText();
  }
  bool GetTextRange(gfx::Range* range) const override {
    *range = text_range_;
    return true;
  }
  bool GetEditableSelectionRange(gfx::Range* range) const override {
    *range = selection_range_;
    return true;
  }
  bool GetTextFromRange(const gfx::Range& range,
                        std::u16string* text) const override {
    *text = surrounding_text_.substr(range.GetMin(), range.length());
    return true;
  }
  void OnInputMethodChanged() override {
    ++on_input_method_changed_call_count_;
  }
  bool SetCompositionFromExistingText(
      const gfx::Range& range,
      const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) override {
    composition_text_ = CompositionText();
    GetTextFromRange(range, &composition_text_.text);
    return true;
  }
  bool SetAutocorrectRange(const gfx::Range& range) override {
    // TODO(crbug.com/1277388): This is a workaround to ensure that the range is
    // valid in the text. Change to use FakeTextInputClient instead of
    // `ui::DummyTextInputClient` so that the text contents can be queried
    // accurately.
    if (!inserted_text_.empty() || inserted_char_ != 0) {
      return ui::DummyTextInputClient::SetAutocorrectRange(range);
    }
    return range.is_empty();
  }

  bool HasNativeEvent() const { return dispatched_key_event_.HasNativeEvent(); }

  void ResetFlags() {
    dispatched_key_event_ =
        ui::KeyEvent(ui::EventType::kUnknown, ui::VKEY_UNKNOWN, ui::EF_NONE);

    composition_text_ = CompositionText();
    confirmed_text_ = CompositionText();
    inserted_text_.clear();
    inserted_char_ = 0;
    inserted_char_flags_ = 0;
    on_input_method_changed_call_count_ = 0;

    input_type_ = ui::TEXT_INPUT_TYPE_NONE;
    input_mode_ = ui::TEXT_INPUT_MODE_DEFAULT;
    can_compose_inline_ = true;
    caret_bounds_ = gfx::Rect();

    set_autocorrect_enabled(true);
  }

 protected:
  std::unique_ptr<TestableInputMethodAsh> input_method_ash_;

  // Copy of the dispatched key event.
  ui::KeyEvent dispatched_key_event_;

  // Variables for remembering the parameters that are passed to
  // ui::TextInputClient functions.
  CompositionText composition_text_;
  CompositionText confirmed_text_;
  std::u16string inserted_text_;
  char16_t inserted_char_;
  unsigned int on_input_method_changed_call_count_;
  int inserted_char_flags_;

  // Variables that will be returned from the ui::TextInputClient functions.
  ui::TextInputType input_type_;
  ui::TextInputMode input_mode_;
  bool can_compose_inline_;
  gfx::Rect caret_bounds_;
  gfx::Range text_range_;
  gfx::Range selection_range_;
  std::u16string surrounding_text_;

  std::unique_ptr<MockIMEEngineHandler> mock_ime_engine_handler_;
  std::unique_ptr<MockIMECandidateWindowHandler>
      mock_ime_candidate_window_handler_;

  bool stop_propagation_post_ime_;

  raw_ptr<TestInputMethodManager, DanglingUntriaged> input_method_manager_;

  base::test::TaskEnvironment task_environment_;
};

// Tests public APIs in `ui::InputMethod` first.

TEST_F(InputMethodAshTest, GetInputTextType) {
  InputMethodAsh ime(this);
  FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  EXPECT_EQ(ime.GetTextInputType(), ui::TEXT_INPUT_TYPE_TEXT);

  ime.SetFocusedTextInputClient(nullptr);
}

TEST_F(InputMethodAshTest, OnTextInputTypeChangedChangesInputType) {
  InputMethodAsh ime(this);
  FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ime.SetFocusedTextInputClient(&fake_text_input_client);
  fake_text_input_client.set_text_input_type(ui::TEXT_INPUT_TYPE_PASSWORD);

  ime.OnTextInputTypeChanged(&fake_text_input_client);

  EXPECT_EQ(ime.GetTextInputType(), ui::TEXT_INPUT_TYPE_PASSWORD);

  ime.SetFocusedTextInputClient(nullptr);
}

TEST_F(InputMethodAshTest, HasBeenPasswordShouldTriggerPassowrd) {
  InputMethodAsh ime(this);
  FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  fake_text_input_client.SetFlags(ui::TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD);

  ime.SetFocusedTextInputClient(&fake_text_input_client);

  EXPECT_EQ(mock_ime_engine_handler_->last_text_input_context().type,
            ui::TEXT_INPUT_TYPE_PASSWORD);

  ime.SetFocusedTextInputClient(nullptr);
}

TEST_F(InputMethodAshTest, GetTextInputClient) {
  EXPECT_EQ(this, input_method_ash_->GetTextInputClient());
  input_method_ash_->SetFocusedTextInputClient(nullptr);
  EXPECT_EQ(nullptr, input_method_ash_->GetTextInputClient());
}

TEST_F(InputMethodAshTest, GetInputTextType_WithoutFocusedClient) {
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, input_method_ash_->GetTextInputType());
  input_method_ash_->SetFocusedTextInputClient(nullptr);
  input_type_ = ui::TEXT_INPUT_TYPE_PASSWORD;
  input_method_ash_->OnTextInputTypeChanged(this);
  // The OnTextInputTypeChanged() call above should be ignored since |this| is
  // not the current focused client.
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, input_method_ash_->GetTextInputType());

  input_method_ash_->SetFocusedTextInputClient(this);
  input_method_ash_->OnTextInputTypeChanged(this);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD,
            input_method_ash_->GetTextInputType());
}

TEST_F(InputMethodAshTest, OnWillChangeFocusedClientClearAutocorrectRange) {
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->SetFocusedTextInputClient(this);
  input_method_ash_->CommitText(
      u"hello",
      TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  input_method_ash_->SetAutocorrectRange(gfx::Range(0, 5), base::DoNothing());
  EXPECT_EQ(gfx::Range(0, 5), this->GetAutocorrectRange());

  input_method_ash_->SetFocusedTextInputClient(nullptr);
  EXPECT_EQ(gfx::Range(), this->GetAutocorrectRange());
}

// Confirm that IBusClient::Focus is called on "connected" if input_type_ is
// TEXT.
TEST_F(InputMethodAshTest, Focus_Text) {
  // A context shouldn't be created since the daemon is not running.
  EXPECT_EQ(0U, on_input_method_changed_call_count_);
  // Click a text input form.
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);
  // Since a form has focus, IBusClient::Focus() should be called.
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(1, mock_ime_candidate_window_handler_
                   ->set_cursor_and_composition_bounds_call_count());
  // ui::TextInputClient::OnInputMethodChanged() should be called when
  // `InputMethodAsh` connects/disconnects to/from ibus-daemon and the
  // current text input type is not NONE.
  EXPECT_EQ(1U, on_input_method_changed_call_count_);
}

// Confirm that InputMethodEngine::Focus is called on "connected" even if
// input_type_ is PASSWORD.
TEST_F(InputMethodAshTest, Focus_Password) {
  EXPECT_EQ(0U, on_input_method_changed_call_count_);
  input_type_ = ui::TEXT_INPUT_TYPE_PASSWORD;
  input_method_ash_->OnTextInputTypeChanged(this);
  // InputMethodEngine::Focus() should be called even for password field.
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(1U, on_input_method_changed_call_count_);
}

// Confirm that IBusClient::Blur is called as expected.
TEST_F(InputMethodAshTest, Blur_None) {
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(0, mock_ime_engine_handler_->focus_out_call_count());
  input_type_ = ui::TEXT_INPUT_TYPE_NONE;
  input_method_ash_->OnTextInputTypeChanged(this);
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_out_call_count());
}

// Confirm that IBusClient::Blur is called as expected.
TEST_F(InputMethodAshTest, Blur_Password) {
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(0, mock_ime_engine_handler_->focus_out_call_count());
  input_type_ = ui::TEXT_INPUT_TYPE_PASSWORD;
  input_method_ash_->OnTextInputTypeChanged(this);
  EXPECT_EQ(2, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_out_call_count());
}

// Focus/Blur scenario test
TEST_F(InputMethodAshTest, Focus_Scenario) {
  // Confirm that both Focus and Blur are NOT called.
  EXPECT_EQ(0, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(0, mock_ime_engine_handler_->focus_out_call_count());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            mock_ime_engine_handler_->last_text_input_context().type);
  EXPECT_EQ(ui::TEXT_INPUT_MODE_DEFAULT,
            mock_ime_engine_handler_->last_text_input_context().mode);

  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_mode_ = ui::TEXT_INPUT_MODE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);
  // Confirm that only Focus is called, the TextInputType is TEXT and the
  // TextInputMode is LATIN..
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(0, mock_ime_engine_handler_->focus_out_call_count());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            mock_ime_engine_handler_->last_text_input_context().type);
  EXPECT_EQ(ui::TEXT_INPUT_MODE_TEXT,
            mock_ime_engine_handler_->last_text_input_context().mode);

  input_mode_ = ui::TEXT_INPUT_MODE_SEARCH;
  input_method_ash_->OnTextInputTypeChanged(this);
  // Confirm that both Focus and Blur are called for mode change.
  EXPECT_EQ(2, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_out_call_count());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            mock_ime_engine_handler_->last_text_input_context().type);
  EXPECT_EQ(ui::TEXT_INPUT_MODE_SEARCH,
            mock_ime_engine_handler_->last_text_input_context().mode);

  input_type_ = ui::TEXT_INPUT_TYPE_URL;
  input_method_ash_->OnTextInputTypeChanged(this);
  // Confirm that both Focus and Blur are called and the TextInputType is
  // changed to URL.
  EXPECT_EQ(3, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(2, mock_ime_engine_handler_->focus_out_call_count());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_URL,
            mock_ime_engine_handler_->last_text_input_context().type);
  EXPECT_EQ(ui::TEXT_INPUT_MODE_SEARCH,
            mock_ime_engine_handler_->last_text_input_context().mode);

  // Confirm that Blur is called when set focus to NULL client.
  input_method_ash_->SetFocusedTextInputClient(nullptr);
  EXPECT_EQ(3, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(3, mock_ime_engine_handler_->focus_out_call_count());
  // Confirm that Focus is called when set focus to this client.
  input_method_ash_->SetFocusedTextInputClient(this);
  EXPECT_EQ(4, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(3, mock_ime_engine_handler_->focus_out_call_count());
}

// Test if the new |caret_bounds_| is correctly sent to ibus-daemon.
TEST_F(InputMethodAshTest, OnCaretBoundsChanged) {
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);
  EXPECT_EQ(1, mock_ime_candidate_window_handler_
                   ->set_cursor_and_composition_bounds_call_count());
  caret_bounds_ = gfx::Rect(1, 2, 3, 4);
  input_method_ash_->OnCaretBoundsChanged(this);
  EXPECT_EQ(2, mock_ime_candidate_window_handler_
                   ->set_cursor_and_composition_bounds_call_count());
  caret_bounds_ = gfx::Rect(0, 2, 3, 4);
  input_method_ash_->OnCaretBoundsChanged(this);
  EXPECT_EQ(3, mock_ime_candidate_window_handler_
                   ->set_cursor_and_composition_bounds_call_count());
  caret_bounds_ = gfx::Rect(0, 2, 3, 4);  // unchanged
  input_method_ash_->OnCaretBoundsChanged(this);
  // Current InputMethodAsh implementation performs the IPC
  // regardless of the bounds are changed or not.
  EXPECT_EQ(4, mock_ime_candidate_window_handler_
                   ->set_cursor_and_composition_bounds_call_count());
}

TEST_F(InputMethodAshTest, ExtractCompositionTextTest_NoAttribute) {
  const std::u16string kSampleAsciiText = u"Sample Text";
  const uint32_t kCursorPos = 2UL;

  CompositionText ash_composition_text;
  ash_composition_text.text = kSampleAsciiText;

  CompositionText composition_text = input_method_ash_->ExtractCompositionText(
      ash_composition_text, kCursorPos);
  EXPECT_EQ(kSampleAsciiText, composition_text.text);
  // If there is no selection, |selection| represents cursor position.
  EXPECT_EQ(kCursorPos, composition_text.selection.start());
  EXPECT_EQ(kCursorPos, composition_text.selection.end());
  // If there is no underline, |ime_text_spans| contains one underline and it is
  // whole text underline.
  ASSERT_EQ(1UL, composition_text.ime_text_spans.size());
  EXPECT_EQ(0UL, composition_text.ime_text_spans[0].start_offset);
  EXPECT_EQ(kSampleAsciiText.size(),
            composition_text.ime_text_spans[0].end_offset);
  EXPECT_EQ(ui::ImeTextSpan::Thickness::kThin,
            composition_text.ime_text_spans[0].thickness);
}

TEST_F(InputMethodAshTest, SetCompositionTextFails) {
  InputMethodAsh ime(this);
  FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  EXPECT_EQ(ime.GetTextInputType(), ui::TEXT_INPUT_TYPE_TEXT);
  // Intentionally have a range start that does not exist.
  EXPECT_FALSE(ime.SetCompositionRange(10000, 5, {}));

  ime.SetFocusedTextInputClient(nullptr);
}

TEST_F(InputMethodAshTest, ExtractCompositionTextTest_SingleUnderline) {
  const uint32_t kCursorPos = 2UL;

  // Set up Ash composition text with one underline attribute.
  CompositionText composition_text;
  composition_text.text = kSampleText;
  ui::ImeTextSpan underline(ui::ImeTextSpan::Type::kComposition, 1UL, 4UL,
                            ui::ImeTextSpan::Thickness::kThin,
                            ui::ImeTextSpan::UnderlineStyle::kSolid,
                            SK_ColorTRANSPARENT);
  composition_text.ime_text_spans.push_back(underline);

  CompositionText composition_text2 =
      input_method_ash_->ExtractCompositionText(composition_text, kCursorPos);
  EXPECT_EQ(kSampleText, composition_text2.text);
  // If there is no selection, |selection| represents cursor position.
  EXPECT_EQ(kCursorPos, composition_text2.selection.start());
  EXPECT_EQ(kCursorPos, composition_text2.selection.end());
  ASSERT_EQ(1UL, composition_text2.ime_text_spans.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.start_offset),
            composition_text2.ime_text_spans[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.end_offset),
            composition_text2.ime_text_spans[0].end_offset);
  // Single underline represents as thin line with text color.
  EXPECT_EQ(SK_ColorTRANSPARENT,
            composition_text2.ime_text_spans[0].underline_color);
  EXPECT_EQ(ui::ImeTextSpan::Thickness::kThin,
            composition_text2.ime_text_spans[0].thickness);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            composition_text2.ime_text_spans[0].background_color);
}

TEST_F(InputMethodAshTest, ExtractCompositionTextTest_DoubleUnderline) {
  const uint32_t kCursorPos = 2UL;

  // Set up Ash composition text with one underline attribute.
  CompositionText composition_text;
  composition_text.text = kSampleText;
  ui::ImeTextSpan underline(ui::ImeTextSpan::Type::kComposition, 1UL, 4UL,
                            ui::ImeTextSpan::Thickness::kThick,
                            ui::ImeTextSpan::UnderlineStyle::kSolid,
                            SK_ColorTRANSPARENT);
  composition_text.ime_text_spans.push_back(underline);

  CompositionText composition_text2 =
      input_method_ash_->ExtractCompositionText(composition_text, kCursorPos);
  EXPECT_EQ(kSampleText, composition_text2.text);
  // If there is no selection, |selection| represents cursor position.
  EXPECT_EQ(kCursorPos, composition_text2.selection.start());
  EXPECT_EQ(kCursorPos, composition_text2.selection.end());
  ASSERT_EQ(1UL, composition_text2.ime_text_spans.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.start_offset),
            composition_text2.ime_text_spans[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.end_offset),
            composition_text2.ime_text_spans[0].end_offset);
  // Double underline represents as thick line with text color.
  EXPECT_EQ(SK_ColorTRANSPARENT,
            composition_text2.ime_text_spans[0].underline_color);
  EXPECT_EQ(ui::ImeTextSpan::Thickness::kThick,
            composition_text2.ime_text_spans[0].thickness);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            composition_text2.ime_text_spans[0].background_color);
}

TEST_F(InputMethodAshTest, ExtractCompositionTextTest_ErrorUnderline) {
  const uint32_t kCursorPos = 2UL;

  // Set up Ash composition text with one underline attribute.
  CompositionText composition_text;
  composition_text.text = kSampleText;
  ui::ImeTextSpan underline(ui::ImeTextSpan::Type::kComposition, 1UL, 4UL,
                            ui::ImeTextSpan::Thickness::kThin,
                            ui::ImeTextSpan::UnderlineStyle::kSolid,
                            SK_ColorTRANSPARENT);
  underline.underline_color = SK_ColorRED;
  composition_text.ime_text_spans.push_back(underline);

  CompositionText composition_text2 =
      input_method_ash_->ExtractCompositionText(composition_text, kCursorPos);
  EXPECT_EQ(kSampleText, composition_text2.text);
  EXPECT_EQ(kCursorPos, composition_text2.selection.start());
  EXPECT_EQ(kCursorPos, composition_text2.selection.end());
  ASSERT_EQ(1UL, composition_text2.ime_text_spans.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.start_offset),
            composition_text2.ime_text_spans[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.end_offset),
            composition_text2.ime_text_spans[0].end_offset);
  // Error underline represents as red thin line.
  EXPECT_EQ(SK_ColorRED, composition_text2.ime_text_spans[0].underline_color);
  EXPECT_EQ(ui::ImeTextSpan::Thickness::kThin,
            composition_text2.ime_text_spans[0].thickness);
}

TEST_F(InputMethodAshTest, ExtractCompositionTextTest_Selection) {
  const uint32_t kCursorPos = 2UL;

  // Set up Ash composition text with one underline attribute.
  CompositionText composition_text;
  composition_text.text = kSampleText;
  composition_text.selection.set_start(1UL);
  composition_text.selection.set_end(4UL);

  CompositionText composition_text2 =
      input_method_ash_->ExtractCompositionText(composition_text, kCursorPos);
  EXPECT_EQ(kSampleText, composition_text2.text);
  EXPECT_EQ(kCursorPos, composition_text2.selection.start());
  EXPECT_EQ(kCursorPos, composition_text2.selection.end());
  ASSERT_EQ(1UL, composition_text2.ime_text_spans.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, composition_text.selection.start()),
            composition_text2.ime_text_spans[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, composition_text.selection.end()),
            composition_text2.ime_text_spans[0].end_offset);
  EXPECT_EQ(SK_ColorTRANSPARENT,
            composition_text2.ime_text_spans[0].underline_color);
  EXPECT_EQ(ui::ImeTextSpan::Thickness::kThick,
            composition_text2.ime_text_spans[0].thickness);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            composition_text2.ime_text_spans[0].background_color);
}

TEST_F(InputMethodAshTest,
       ExtractCompositionTextTest_SelectionStartWithCursor) {
  const uint32_t kCursorPos = 1UL;

  // Set up Ash composition text with one underline attribute.
  CompositionText composition_text;
  composition_text.text = kSampleText;
  composition_text.selection.set_start(kCursorPos);
  composition_text.selection.set_end(4UL);

  CompositionText composition_text2 =
      input_method_ash_->ExtractCompositionText(composition_text, kCursorPos);
  EXPECT_EQ(kSampleText, composition_text2.text);
  // If the cursor position is same as selection bounds, selection start
  // position become opposit side of selection from cursor.
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, composition_text.selection.end()),
            composition_text2.selection.start());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, kCursorPos),
            composition_text2.selection.end());
  ASSERT_EQ(1UL, composition_text2.ime_text_spans.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, composition_text.selection.start()),
            composition_text2.ime_text_spans[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, composition_text.selection.end()),
            composition_text2.ime_text_spans[0].end_offset);
  EXPECT_EQ(SK_ColorTRANSPARENT,
            composition_text2.ime_text_spans[0].underline_color);
  EXPECT_EQ(ui::ImeTextSpan::Thickness::kThick,
            composition_text2.ime_text_spans[0].thickness);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            composition_text2.ime_text_spans[0].background_color);
}

TEST_F(InputMethodAshTest, ExtractCompositionTextTest_SelectionEndWithCursor) {
  const uint32_t kCursorPos = 4UL;

  // Set up Ash composition text with one underline attribute.
  CompositionText composition_text;
  composition_text.text = kSampleText;
  composition_text.selection.set_start(1UL);
  composition_text.selection.set_end(kCursorPos);

  CompositionText composition_text2 =
      input_method_ash_->ExtractCompositionText(composition_text, kCursorPos);
  EXPECT_EQ(kSampleText, composition_text2.text);
  // If the cursor position is same as selection bounds, selection start
  // position become opposit side of selection from cursor.
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, composition_text.selection.start()),
            composition_text2.selection.start());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, kCursorPos),
            composition_text2.selection.end());
  ASSERT_EQ(1UL, composition_text2.ime_text_spans.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, composition_text.selection.start()),
            composition_text2.ime_text_spans[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, composition_text.selection.end()),
            composition_text2.ime_text_spans[0].end_offset);
  EXPECT_EQ(SK_ColorTRANSPARENT,
            composition_text2.ime_text_spans[0].underline_color);
  EXPECT_EQ(ui::ImeTextSpan::Thickness::kThick,
            composition_text2.ime_text_spans[0].thickness);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            composition_text2.ime_text_spans[0].background_color);
}

TEST_F(InputMethodAshTest, SurroundingText_NoSelectionTest) {
  // Click a text input form.
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  // Set the TextInputClient behaviors.
  surrounding_text_ = u"abcdef";
  text_range_ = gfx::Range(0, 6);
  selection_range_ = gfx::Range(3, 3);

  // Set the verifier for SetSurroundingText mock call.
  SetSurroundingTextVerifier verifier(UTF16ToUTF8(surrounding_text_), 3, 3);

  input_method_ash_->OnCaretBoundsChanged(this);

  // Check the call count.
  EXPECT_EQ(1, mock_ime_engine_handler_->set_surrounding_text_call_count());
  EXPECT_EQ(surrounding_text_,
            mock_ime_engine_handler_->last_set_surrounding_text());
  EXPECT_EQ(gfx::Range(3),
            mock_ime_engine_handler_->last_set_selection_range());
}

TEST_F(InputMethodAshTest, SurroundingText_SelectionTest) {
  // Click a text input form.
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  // Set the TextInputClient behaviors.
  surrounding_text_ = u"abcdef";
  text_range_ = gfx::Range(0, 6);
  selection_range_ = gfx::Range(2, 5);

  // Set the verifier for SetSurroundingText mock call.
  SetSurroundingTextVerifier verifier(UTF16ToUTF8(surrounding_text_), 2, 5);

  input_method_ash_->OnCaretBoundsChanged(this);

  // Check the call count.
  EXPECT_EQ(1, mock_ime_engine_handler_->set_surrounding_text_call_count());
  EXPECT_EQ(surrounding_text_,
            mock_ime_engine_handler_->last_set_surrounding_text());
  EXPECT_EQ(gfx::Range(2, 5),
            mock_ime_engine_handler_->last_set_selection_range());
}

TEST_F(InputMethodAshTest, SurroundingText_PartialText) {
  // Click a text input form.
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  // Set the TextInputClient behaviors.
  surrounding_text_ = u"abcdefghij";
  text_range_ = gfx::Range(5, 10);
  selection_range_ = gfx::Range(7, 9);

  input_method_ash_->OnCaretBoundsChanged(this);

  // Check the call count.
  EXPECT_EQ(1, mock_ime_engine_handler_->set_surrounding_text_call_count());
  // Set the verifier for SetSurroundingText mock call.
  // Here (2, 4) is selection range in expected surrounding text coordinates.
  EXPECT_EQ(u"fghij", mock_ime_engine_handler_->last_set_surrounding_text());
  EXPECT_EQ(gfx::Range(2, 4),
            mock_ime_engine_handler_->last_set_selection_range());
}

TEST_F(InputMethodAshTest, SurroundingText_BecomeEmptyText) {
  // Click a text input form.
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  // Set the TextInputClient behaviors.
  // If the surrounding text becomes empty, text_range become (0, 0) and
  // selection range become invalid.
  surrounding_text_ = u"";
  text_range_ = gfx::Range(0, 0);
  selection_range_ = gfx::Range::InvalidRange();

  input_method_ash_->OnCaretBoundsChanged(this);

  // Check the call count.
  EXPECT_EQ(0, mock_ime_engine_handler_->set_surrounding_text_call_count());

  // Should not be called twice with same condition.
  input_method_ash_->OnCaretBoundsChanged(this);
  EXPECT_EQ(0, mock_ime_engine_handler_->set_surrounding_text_call_count());
}

TEST_F(InputMethodAshTest, SurroundingText_EventOrder) {
  ::testing::NiceMock<NiceMockIMEEngine> mock_engine;
  IMEBridge::Get()->SetCurrentEngineHandler(&mock_engine);

  {
    // Switches the text input client.
    ::testing::InSequence seq;
    EXPECT_CALL(mock_engine, Blur);
    EXPECT_CALL(mock_engine, Focus);
    EXPECT_CALL(mock_engine, SetSurroundingText);

    surrounding_text_ = u"a";
    text_range_ = gfx::Range(0, 1);
    selection_range_ = gfx::Range(0, 0);

    input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
    input_method_ash_->OnWillChangeFocusedClient(nullptr, this);
    input_method_ash_->OnDidChangeFocusedClient(nullptr, this);
  }

  {
    // Changes text input type.
    ::testing::InSequence seq;
    EXPECT_CALL(mock_engine, Blur);
    EXPECT_CALL(mock_engine, Focus);
    EXPECT_CALL(mock_engine, SetSurroundingText);

    surrounding_text_ = u"b";
    text_range_ = gfx::Range(0, 1);
    selection_range_ = gfx::Range(0, 0);

    input_type_ = ui::TEXT_INPUT_TYPE_EMAIL;
    input_method_ash_->OnTextInputTypeChanged(this);
  }
  IMEBridge::Get()->SetCurrentEngineHandler(nullptr);
}

TEST_F(InputMethodAshTest, SetCompositionRange_InvalidRange) {
  // Focus on a text field.
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  // Insert some text and place the cursor.
  surrounding_text_ = u"abc";
  text_range_ = gfx::Range(0, 3);
  selection_range_ = gfx::Range(1, 1);

  EXPECT_FALSE(input_method_ash_->SetCompositionRange(0, 4, {}));
  EXPECT_EQ(0U, composition_text_.text.length());
}

TEST_F(InputMethodAshTest,
       SetCompositionRangeWithSelectedTextAccountsForSelection) {
  FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  fake_text_input_client.SetTextAndSelection(u"01234", gfx::Range(1, 4));
  InputMethodAsh ime(this);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  // before/after are relative to the selection start/end, respectively.
  EXPECT_TRUE(ime.SetCompositionRange(/*before=*/1, /*after=*/1, {}));

  EXPECT_EQ(fake_text_input_client.composition_range(), gfx::Range(0, 5));
  EXPECT_THAT(fake_text_input_client.ime_text_spans(),
              testing::ElementsAre(
                  ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition,
                                  /*start_offset=*/0, /*end_offset=*/5)));
}

TEST_F(InputMethodAshTest, ConfirmComposition_NoComposition) {
  // Focus on a text field.
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  input_method_ash_->ConfirmComposition(/* reset_engine */ true);

  EXPECT_TRUE(confirmed_text_.text.empty());
  EXPECT_TRUE(composition_text_.text.empty());
}

TEST_F(InputMethodAshTest, ConfirmComposition_SetComposition) {
  // Focus on a text field.
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  CompositionText composition_text;
  composition_text.text = u"hello";
  SetCompositionText(composition_text);
  input_method_ash_->ConfirmComposition(/* reset_engine */ true);

  EXPECT_EQ(u"hello", confirmed_text_.text);
  EXPECT_TRUE(composition_text_.text.empty());
}

TEST_F(InputMethodAshTest, ConfirmComposition_SetCompositionRange) {
  // Focus on a text field.
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  // Place some text.
  surrounding_text_ = u"abc";
  text_range_ = gfx::Range(0, 3);

  // "abc" is in composition. Put the two characters in composition.
  input_method_ash_->SetCompositionRange(0, 2, {});
  input_method_ash_->ConfirmComposition(/* reset_engine */ true);

  EXPECT_EQ(u"ab", confirmed_text_.text);
  EXPECT_TRUE(composition_text_.text.empty());
}

TEST_F(InputMethodAshTest, SetAutocorrectRange_SuccessfulSet) {
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  InsertText(u"a",
             TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  bool callback_called = false;
  bool callback_result = false;

  input_method_ash_->SetAutocorrectRange(
      gfx::Range(0, 1), base::BindOnce(
                            [](bool* called, bool* result, bool success) {
                              *called = true;
                              *result = success;
                            },
                            &callback_called, &callback_result));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
  EXPECT_EQ(gfx::Range(0, 1), GetAutocorrectRange());
}

TEST_F(InputMethodAshTest, SetAutocorrectRange_FailedSet) {
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  bool callback_called = false;
  bool callback_result = false;

  // Set range on empty text must fail.
  input_method_ash_->SetAutocorrectRange(
      gfx::Range(0, 1), base::BindOnce(
                            [](bool* called, bool* result, bool success) {
                              *called = true;
                              *result = success;
                            },
                            &callback_called, &callback_result));

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(callback_result);
  EXPECT_TRUE(GetAutocorrectRange().is_empty());
}

class InputMethodAshKeyEventTest : public InputMethodAshTest {
 public:
  InputMethodAshKeyEventTest() = default;

  InputMethodAshKeyEventTest(const InputMethodAshKeyEventTest&) = delete;
  InputMethodAshKeyEventTest& operator=(const InputMethodAshKeyEventTest&) =
      delete;

  ~InputMethodAshKeyEventTest() override = default;
};

TEST_F(InputMethodAshKeyEventTest, KeyEventDelayResponseTest) {
  const int kFlags = ui::EF_SHIFT_DOWN;
  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_A, kFlags);

  // Do key event.
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);
  input_method_ash_->DispatchKeyEvent(&event);

  // Check before state.
  const ui::KeyEvent* key_event =
      mock_ime_engine_handler_->last_processed_key_event();
  EXPECT_EQ(1, mock_ime_engine_handler_->process_key_event_call_count());
  EXPECT_EQ(ui::VKEY_A, key_event->key_code());
  EXPECT_EQ(kFlags, key_event->flags());
  EXPECT_EQ(0, input_method_ash_->process_key_event_post_ime_call_count());

  static_cast<TextInputTarget*>(input_method_ash_.get())
      ->CommitText(
          u"A",
          TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  EXPECT_EQ(0, inserted_char_);

  // Do callback.
  std::move(mock_ime_engine_handler_->last_passed_callback())
      .Run(ui::ime::KeyEventHandledState::kHandledByIME);

  // Check the results
  EXPECT_EQ(1, input_method_ash_->process_key_event_post_ime_call_count());
  const ui::KeyEvent stored_event =
      input_method_ash_->process_key_event_post_ime_args().event;
  EXPECT_EQ(ui::VKEY_A, stored_event.key_code());
  EXPECT_EQ(kFlags, stored_event.flags());
  EXPECT_EQ(input_method_ash_->process_key_event_post_ime_args().handled_state,
            ui::ime::KeyEventHandledState::kHandledByIME);

  EXPECT_EQ(L'A', inserted_char_);
}

TEST_F(InputMethodAshKeyEventTest, MultiKeyEventDelayResponseTest) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

  // Preparation
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  const int kFlags = ui::EF_SHIFT_DOWN;
  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_B, kFlags);

  // Do key event.
  input_method_ash_->DispatchKeyEvent(&event);
  const ui::KeyEvent* key_event =
      mock_ime_engine_handler_->last_processed_key_event();
  EXPECT_EQ(ui::VKEY_B, key_event->key_code());
  EXPECT_EQ(kFlags, key_event->flags());

  KeyEventCallback first_callback =
      mock_ime_engine_handler_->last_passed_callback();

  // Do key event again.
  ui::KeyEvent event2(ui::EventType::kKeyPressed, ui::VKEY_C, kFlags);

  input_method_ash_->DispatchKeyEvent(&event2);
  const ui::KeyEvent* key_event2 =
      mock_ime_engine_handler_->last_processed_key_event();
  EXPECT_EQ(ui::VKEY_C, key_event2->key_code());
  EXPECT_EQ(kFlags, key_event2->flags());

  // Check before state.
  EXPECT_EQ(2, mock_ime_engine_handler_->process_key_event_call_count());
  EXPECT_EQ(0, input_method_ash_->process_key_event_post_ime_call_count());

  CompositionText comp;
  comp.text = u"B";
  (static_cast<TextInputTarget*>(input_method_ash_.get()))
      ->UpdateCompositionText(comp, comp.text.length(), true);

  EXPECT_EQ(0, composition_text_.text[0]);

  // Do callback for first key event.
  std::move(first_callback).Run(ui::ime::KeyEventHandledState::kHandledByIME);

  EXPECT_EQ(comp.text, composition_text_.text);

  // Check the results for first key event.
  EXPECT_EQ(1, input_method_ash_->process_key_event_post_ime_call_count());
  ui::KeyEvent stored_event =
      input_method_ash_->process_key_event_post_ime_args().event;
  EXPECT_EQ(ui::VKEY_B, stored_event.key_code());
  EXPECT_EQ(kFlags, stored_event.flags());
  EXPECT_EQ(input_method_ash_->process_key_event_post_ime_args().handled_state,
            ui::ime::KeyEventHandledState::kHandledByIME);
  EXPECT_EQ(0, inserted_char_);

  // Do callback for second key event.
  mock_ime_engine_handler_->last_passed_callback().Run(
      ui::ime::KeyEventHandledState::kNotHandled);

  // Check the results for second key event.
  EXPECT_EQ(2, input_method_ash_->process_key_event_post_ime_call_count());
  stored_event = input_method_ash_->process_key_event_post_ime_args().event;
  EXPECT_EQ(ui::VKEY_C, stored_event.key_code());
  EXPECT_EQ(kFlags, stored_event.flags());
  EXPECT_EQ(input_method_ash_->process_key_event_post_ime_args().handled_state,
            ui::ime::KeyEventHandledState::kNotHandled);

  EXPECT_EQ(L'C', inserted_char_);
}

TEST_F(InputMethodAshKeyEventTest, StopPropagationTest) {
  // Preparation
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  // Do key event with event being stopped propagation.
  stop_propagation_post_ime_ = true;
  ui::KeyEvent eventA(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  eventA.set_character(L'A');
  input_method_ash_->DispatchKeyEvent(&eventA);
  mock_ime_engine_handler_->last_passed_callback().Run(
      ui::ime::KeyEventHandledState::kNotHandled);

  const ui::KeyEvent* key_event =
      mock_ime_engine_handler_->last_processed_key_event();
  EXPECT_EQ(ui::VKEY_A, key_event->key_code());
  EXPECT_EQ(0, inserted_char_);

  // Do key event with event not being stopped propagation.
  stop_propagation_post_ime_ = false;
  input_method_ash_->DispatchKeyEvent(&eventA);
  mock_ime_engine_handler_->last_passed_callback().Run(
      ui::ime::KeyEventHandledState::kNotHandled);

  key_event = mock_ime_engine_handler_->last_processed_key_event();
  EXPECT_EQ(ui::VKEY_A, key_event->key_code());
  EXPECT_EQ(L'A', inserted_char_);
}

TEST_F(InputMethodAshKeyEventTest, DeadKeyPressTest) {
  base::test::ScopedFeatureList feature_list(features::kInputMethodDeadKeyFix);
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  ui::KeyEvent eventA(ui::EventType::kKeyPressed,
                      ui::VKEY_OEM_4,  // '['
                      ui::DomCode::BRACKET_LEFT, 0,
                      ui::DomKey::DeadKeyFromCombiningCharacter('^'),
                      ui::EventTimeForNow());
  input_method_ash_->ProcessKeyEventPostIME(
      &eventA, ui::ime::KeyEventHandledState::kHandledByIME, true);

  const ui::KeyEvent& key_event = dispatched_key_event_;

  EXPECT_EQ(ui::EventType::kKeyPressed, key_event.type());
  EXPECT_EQ(eventA.key_code(), key_event.key_code());
  EXPECT_EQ(eventA.code(), key_event.code());
  EXPECT_EQ(eventA.flags(), key_event.flags());
  EXPECT_EQ(eventA.GetDomKey(), key_event.GetDomKey());
  EXPECT_EQ(eventA.time_stamp(), key_event.time_stamp());
}

TEST_F(InputMethodAshTest, UnhandledDeadKeyForNonTerminalSendsDeadKeys) {
  base::test::ScopedFeatureList feature_list(features::kInputMethodDeadKeyFix);

  for (const GURL& url : {
           GURL("chrome-untrusted://emoji"),
           GURL("chrome://crosh"),
           GURL("chrome://terminal"),
       }) {
    FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
    fake_text_input_client.SetUrl(url);
    InputMethodAsh ime(this);
    ime.SetFocusedTextInputClient(&fake_text_input_client);

    ui::KeyEvent key_press(ui::EventType::kKeyPressed,
                           ui::VKEY_OEM_4,  // '['
                           ui::DomCode::BRACKET_LEFT, 0,
                           ui::DomKey::DeadKeyFromCombiningCharacter('^'),
                           ui::EventTimeForNow());
    ime.DispatchKeyEvent(&key_press);
    std::move(mock_ime_engine_handler_->last_passed_callback())
        .Run(ui::ime::KeyEventHandledState::kNotHandled);
    const ui::KeyEvent dispatched_key_press = dispatched_key_event_;

    ui::KeyEvent key_release(ui::EventType::kKeyReleased,
                             ui::VKEY_OEM_4,  // '['
                             ui::DomCode::BRACKET_LEFT, 0,
                             ui::DomKey::DeadKeyFromCombiningCharacter('^'),
                             ui::EventTimeForNow());
    ime.DispatchKeyEvent(&key_release);
    std::move(mock_ime_engine_handler_->last_passed_callback())
        .Run(ui::ime::KeyEventHandledState::kNotHandled);
    const ui::KeyEvent dispatched_key_release = dispatched_key_event_;

    EXPECT_EQ(dispatched_key_press.type(), ui::EventType::kKeyPressed);
    EXPECT_EQ(dispatched_key_press.key_code(), ui::VKEY_OEM_4);
    EXPECT_EQ(dispatched_key_press.code(), ui::DomCode::BRACKET_LEFT);
    EXPECT_EQ(dispatched_key_press.GetDomKey(),
              ui::DomKey::DeadKeyFromCombiningCharacter('^'));
    EXPECT_EQ(dispatched_key_release.type(), ui::EventType::kKeyReleased);
    EXPECT_EQ(dispatched_key_release.key_code(), ui::VKEY_OEM_4);
    EXPECT_EQ(dispatched_key_release.code(), ui::DomCode::BRACKET_LEFT);
    EXPECT_EQ(dispatched_key_release.GetDomKey(),
              ui::DomKey::DeadKeyFromCombiningCharacter('^'));
  }
}

TEST_F(InputMethodAshTest, UnhandledDeadKeyForTerminalSendsDeadKeys) {
  for (const GURL& url : {
           GURL("chrome-untrusted://crosh"),
           GURL("chrome-untrusted://croshy"),
           GURL("chrome-untrusted://crosh/"),
           GURL("chrome-untrusted://crosh/a?b=1&c=2#d"),
           GURL("chrome-untrusted://terminal"),
           GURL("chrome-untrusted://terminaly"),
           GURL("chrome-untrusted://terminal/"),
           GURL("chrome-untrusted://terminal/a?b=1&c=2#d"),
       }) {
    FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
    fake_text_input_client.SetUrl(url);
    InputMethodAsh ime(this);
    ime.SetFocusedTextInputClient(&fake_text_input_client);

    ui::KeyEvent key_press(ui::EventType::kKeyPressed,
                           ui::VKEY_OEM_4,  // '['
                           ui::DomCode::BRACKET_LEFT, 0,
                           ui::DomKey::DeadKeyFromCombiningCharacter('^'),
                           ui::EventTimeForNow());
    ime.DispatchKeyEvent(&key_press);
    std::move(mock_ime_engine_handler_->last_passed_callback())
        .Run(ui::ime::KeyEventHandledState::kNotHandled);
    const ui::KeyEvent dispatched_key_press = dispatched_key_event_;

    ui::KeyEvent key_release(ui::EventType::kKeyReleased,
                             ui::VKEY_OEM_4,  // '['
                             ui::DomCode::BRACKET_LEFT, 0,
                             ui::DomKey::DeadKeyFromCombiningCharacter('^'),
                             ui::EventTimeForNow());
    ime.DispatchKeyEvent(&key_release);
    std::move(mock_ime_engine_handler_->last_passed_callback())
        .Run(ui::ime::KeyEventHandledState::kNotHandled);
    const ui::KeyEvent dispatched_key_release = dispatched_key_event_;

    EXPECT_EQ(dispatched_key_press.type(), ui::EventType::kKeyPressed);
    EXPECT_EQ(dispatched_key_press.key_code(), ui::VKEY_OEM_4);
    EXPECT_EQ(dispatched_key_press.code(), ui::DomCode::BRACKET_LEFT);
    EXPECT_EQ(dispatched_key_press.GetDomKey(),
              ui::DomKey::DeadKeyFromCombiningCharacter('^'));
    EXPECT_EQ(dispatched_key_release.type(), ui::EventType::kKeyReleased);
    EXPECT_EQ(dispatched_key_release.key_code(), ui::VKEY_OEM_4);
    EXPECT_EQ(dispatched_key_release.code(), ui::DomCode::BRACKET_LEFT);
    EXPECT_EQ(dispatched_key_release.GetDomKey(),
              ui::DomKey::DeadKeyFromCombiningCharacter('^'));
  }
}

TEST_F(InputMethodAshTest, DeadKeyHandledByAssistiveSendsProcessKey) {
  FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  fake_text_input_client.SetUrl(GURL("chrome-untrusted://crosh"));
  InputMethodAsh ime(this);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  ui::KeyEvent key_press(ui::EventType::kKeyPressed,
                         ui::VKEY_OEM_4,  // '['
                         ui::DomCode::BRACKET_LEFT, 0,
                         ui::DomKey::DeadKeyFromCombiningCharacter('^'),
                         ui::EventTimeForNow());
  ime.DispatchKeyEvent(&key_press);
  std::move(mock_ime_engine_handler_->last_passed_callback())
      .Run(ui::ime::KeyEventHandledState::kHandledByAssistiveSuggester);
  const ui::KeyEvent dispatched_key_press = dispatched_key_event_;

  EXPECT_EQ(dispatched_key_press.type(), ui::EventType::kKeyPressed);
  EXPECT_EQ(dispatched_key_press.key_code(), ui::VKEY_PROCESSKEY);
  EXPECT_EQ(dispatched_key_press.code(), ui::DomCode::BRACKET_LEFT);
  EXPECT_EQ(dispatched_key_press.GetDomKey(), ui::DomKey::PROCESS);
}

TEST_F(InputMethodAshKeyEventTest, KeyboardImeFlags) {
  // Preparation.
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  {
    ui::KeyEvent eventA(ui::EventType::kKeyPressed, ui::VKEY_A,
                        ui::DomCode::US_A, 0, ui::DomKey::FromCharacter('a'),
                        ui::EventTimeForNow());
    input_method_ash_->ProcessKeyEventPostIME(
        &eventA, ui::ime::KeyEventHandledState::kHandledByIME, true);

    const ui::KeyEvent& key_event = dispatched_key_event_;
    EXPECT_EQ(ui::kPropertyKeyboardImeHandledFlag,
              ui::GetKeyboardImeFlags(key_event));
  }

  {
    ui::KeyEvent eventA(ui::EventType::kKeyPressed, ui::VKEY_A,
                        ui::DomCode::US_A, 0, ui::DomKey::FromCharacter('a'),
                        ui::EventTimeForNow());
    input_method_ash_->ProcessKeyEventPostIME(
        &eventA, ui::ime::KeyEventHandledState::kNotHandled, true);

    const ui::KeyEvent& key_event = dispatched_key_event_;
    EXPECT_EQ(ui::kPropertyKeyboardImeIgnoredFlag,
              ui::GetKeyboardImeFlags(key_event));
  }
}

TEST_F(InputMethodAshKeyEventTest, HandledKeyEventDoesNotSuppressAutoRepeat) {
  // Preparation.
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  {
    ui::KeyEvent eventA(ui::EventType::kKeyPressed, ui::VKEY_A,
                        ui::DomCode::US_A, 0, ui::DomKey::FromCharacter('a'),
                        ui::EventTimeForNow());
    input_method_ash_->ProcessKeyEventPostIME(
        &eventA, ui::ime::KeyEventHandledState::kHandledByIME,
        /*stopped_propagation=*/true);

    EXPECT_FALSE(
        ui::HasKeyEventSuppressAutoRepeat(*dispatched_key_event_.properties()));
  }

  {
    ui::KeyEvent eventA(ui::EventType::kKeyPressed, ui::VKEY_A,
                        ui::DomCode::US_A, 0, ui::DomKey::FromCharacter('a'),
                        ui::EventTimeForNow());
    input_method_ash_->ProcessKeyEventPostIME(
        &eventA, ui::ime::KeyEventHandledState::kHandledByAssistiveSuggester,
        /*stopped_propagation=*/true);

    EXPECT_FALSE(
        ui::HasKeyEventSuppressAutoRepeat(*dispatched_key_event_.properties()));
  }
}

TEST_F(InputMethodAshKeyEventTest,
       NotHandledKeyEventDoesNotSuppressAutoRepeat) {
  // Preparation.
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  ui::KeyEvent eventA(ui::EventType::kKeyPressed, ui::VKEY_A, ui::DomCode::US_A,
                      0, ui::DomKey::FromCharacter('a'), ui::EventTimeForNow());
  input_method_ash_->ProcessKeyEventPostIME(
      &eventA, ui::ime::KeyEventHandledState::kNotHandled,
      /*stopped_propagation=*/false);

  EXPECT_FALSE(
      ui::HasKeyEventSuppressAutoRepeat(*dispatched_key_event_.properties()));
}

TEST_F(InputMethodAshKeyEventTest,
       NotHandledSuppressKeyEventSuppressesAutoRepeat) {
  // Preparation.
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);

  ui::KeyEvent eventA(ui::EventType::kKeyPressed, ui::VKEY_A, ui::DomCode::US_A,
                      0, ui::DomKey::FromCharacter('a'), ui::EventTimeForNow());
  input_method_ash_->ProcessKeyEventPostIME(
      &eventA, ui::ime::KeyEventHandledState::kNotHandledSuppressAutoRepeat,
      /*stopped_propagation=*/false);

  EXPECT_TRUE(
      ui::HasKeyEventSuppressAutoRepeat(*dispatched_key_event_.properties()));
}

TEST_F(InputMethodAshKeyEventTest,
       SingleCharAssistiveSuggesterKeyEventDispatchesProcessKey) {
  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;

  input_method_ash_->OnTextInputTypeChanged(this);
  input_method_ash_->DispatchKeyEvent(&event);
  static_cast<TextInputTarget*>(input_method_ash_.get())
      ->CommitText(
          u"b",
          TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  std::move(mock_ime_engine_handler_->last_passed_callback())
      .Run(ui::ime::KeyEventHandledState::kHandledByAssistiveSuggester);

  const ui::KeyEvent& key_event = dispatched_key_event_;
  EXPECT_EQ(ui::EventType::kKeyPressed, key_event.type());
  EXPECT_EQ(ui::VKEY_PROCESSKEY, key_event.key_code());
  EXPECT_EQ(event.code(), key_event.code());
  EXPECT_EQ(event.flags(), key_event.flags());
  EXPECT_EQ(ui::DomKey::PROCESS, key_event.GetDomKey());
  EXPECT_EQ(event.time_stamp(), key_event.time_stamp());
  EXPECT_EQ(ui::kPropertyKeyboardImeHandledFlag,
            ui::GetKeyboardImeFlags(key_event));
}

TEST_F(InputMethodAshKeyEventTest, JP106KeyTest) {
  ui::KeyEvent eventConvert(ui::EventType::kKeyPressed, ui::VKEY_CONVERT,
                            ui::EF_NONE);
  input_method_ash_->DispatchKeyEvent(&eventConvert);
  EXPECT_FALSE(input_method_manager_->state()->is_jp_kbd());
  EXPECT_TRUE(input_method_manager_->state()->is_jp_ime());

  ui::KeyEvent eventNonConvert(ui::EventType::kKeyPressed, ui::VKEY_NONCONVERT,
                               ui::EF_NONE);
  input_method_ash_->DispatchKeyEvent(&eventNonConvert);
  EXPECT_TRUE(input_method_manager_->state()->is_jp_kbd());
  EXPECT_FALSE(input_method_manager_->state()->is_jp_ime());

  ui::KeyEvent eventDbeSbc(ui::EventType::kKeyPressed, ui::VKEY_DBE_SBCSCHAR,
                           ui::EF_NONE);
  input_method_ash_->DispatchKeyEvent(&eventDbeSbc);
  EXPECT_FALSE(input_method_manager_->state()->is_jp_kbd());
  EXPECT_TRUE(input_method_manager_->state()->is_jp_ime());

  ui::KeyEvent eventDbeDbc(ui::EventType::kKeyPressed, ui::VKEY_DBE_DBCSCHAR,
                           ui::EF_NONE);
  input_method_ash_->DispatchKeyEvent(&eventDbeDbc);
  EXPECT_TRUE(input_method_manager_->state()->is_jp_kbd());
  EXPECT_FALSE(input_method_manager_->state()->is_jp_ime());
}

TEST_F(InputMethodAshKeyEventTest, SetAutocorrectRangeRunsAfterKeyEvent) {
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);
  input_method_ash_->CommitText(
      u"a", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::DomCode::US_A,
                     ui::EF_NONE, ui::DomKey::FromCharacter('a'),
                     ui::EventTimeForNow());
  input_method_ash_->DispatchKeyEvent(&event);

  bool callback_called = false;
  bool callback_result = false;

  input_method_ash_->SetAutocorrectRange(
      gfx::Range(0, 1), base::BindOnce(
                            [](bool* called, bool* result, bool success) {
                              *called = true;
                              *result = success;
                            },
                            &callback_called, &callback_result));
  EXPECT_FALSE(callback_called);

  std::move(mock_ime_engine_handler_->last_passed_callback())
      .Run(ui::ime::KeyEventHandledState::kHandledByIME);

  EXPECT_EQ(gfx::Range(0, 1), GetAutocorrectRange());
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(InputMethodAshKeyEventTest, SetAutocorrectRangeRunsAfterCommitText) {
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);
  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  input_method_ash_->DispatchKeyEvent(&event);

  input_method_ash_->CommitText(
      u"a", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  bool callback_called = false;
  input_method_ash_->SetAutocorrectRange(
      gfx::Range(0, 1),
      base::BindOnce([](bool* called, bool result) { *called = true; },
                     (&callback_called)));
  EXPECT_FALSE(callback_called);

  std::move(mock_ime_engine_handler_->last_passed_callback())
      .Run(ui::ime::KeyEventHandledState::kHandledByIME);

  EXPECT_EQ(L'a', inserted_char_);
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(gfx::Range(0, 1), GetAutocorrectRange());
}

TEST_F(InputMethodAshKeyEventTest,
       SetAutocorrectRangeCallsCallbackOnFailureAfterKeyEvent) {
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);
  input_method_ash_->CommitText(
      u"a", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  // Disable autocorrect range to make it return false.
  set_autocorrect_enabled(false);

  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::DomCode::US_A,
                     ui::EF_NONE, ui::DomKey::FromCharacter('a'),
                     ui::EventTimeForNow());
  input_method_ash_->DispatchKeyEvent(&event);

  bool callback_called = false;
  bool callback_result = false;

  input_method_ash_->SetAutocorrectRange(
      gfx::Range(0, 1), base::BindOnce(
                            [](bool* called, bool* result, bool success) {
                              *called = true;
                              *result = success;
                            },
                            &callback_called, &callback_result));
  EXPECT_FALSE(callback_called);

  std::move(mock_ime_engine_handler_->last_passed_callback())
      .Run(ui::ime::KeyEventHandledState::kHandledByIME);

  EXPECT_EQ(gfx::Range(), GetAutocorrectRange());
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(callback_result);
}

TEST_F(
    InputMethodAshKeyEventTest,
    LatestSetAutocorrectRangeOverridesPreviousRequestsWhileHandlingKeyEvent) {
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ash_->OnTextInputTypeChanged(this);
  input_method_ash_->CommitText(
      u"a", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::DomCode::US_A,
                     ui::EF_NONE, ui::DomKey::FromCharacter('a'),
                     ui::EventTimeForNow());
  input_method_ash_->DispatchKeyEvent(&event);

  bool first_set_callback_called = false;
  bool first_set_callback_result = false;
  bool second_set_callback_called = false;
  bool second_set_callback_result = false;

  auto set_range_callback = [](bool* called, bool* result, bool success) {
    *called = true;
    *result = success;
  };

  input_method_ash_->SetAutocorrectRange(
      gfx::Range(0, 1),
      base::BindOnce(set_range_callback, &first_set_callback_called,
                     &first_set_callback_result));
  EXPECT_FALSE(first_set_callback_called);

  // Override first call.
  input_method_ash_->SetAutocorrectRange(
      gfx::Range(0, 1),
      base::BindOnce(set_range_callback, &second_set_callback_called,
                     &second_set_callback_result));
  EXPECT_FALSE(second_set_callback_called);
  EXPECT_TRUE(first_set_callback_called);
  EXPECT_FALSE(first_set_callback_result);

  std::move(mock_ime_engine_handler_->last_passed_callback())
      .Run(ui::ime::KeyEventHandledState::kHandledByIME);

  EXPECT_EQ(gfx::Range(0, 1), GetAutocorrectRange());
  EXPECT_TRUE(second_set_callback_result);
}

TEST_F(InputMethodAshKeyEventTest,
       MultipleCommitTextsWhileHandlingKeyEventCoalescesIntoOne) {
  FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh ime(this);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::DomCode::US_A,
                     ui::EF_NONE, ui::DomKey::FromCharacter('a'),
                     ui::EventTimeForNow());
  ime.DispatchKeyEvent(&event);
  ime.CommitText(
      u"a", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  ime.CommitText(
      u"b", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  ime.CommitText(
      u"cde", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  std::move(mock_ime_engine_handler_->last_passed_callback())
      .Run(ui::ime::KeyEventHandledState::kHandledByIME);

  EXPECT_EQ(fake_text_input_client.text(), u"abcde");
  EXPECT_EQ(fake_text_input_client.selection(), gfx::Range(5, 5));
}

TEST_F(InputMethodAshKeyEventTest,
       MultipleCommitTextsWhileHandlingKeyEventCoalescesByCaretBehavior) {
  FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh ime(this);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::DomCode::US_A,
                     ui::EF_NONE, ui::DomKey::FromCharacter('a'),
                     ui::EventTimeForNow());
  ime.DispatchKeyEvent(&event);
  ime.CommitText(
      u"a", TextInputClient::InsertTextCursorBehavior::kMoveCursorBeforeText);
  ime.CommitText(
      u"b", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  ime.CommitText(
      u"c", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  ime.CommitText(
      u"d", TextInputClient::InsertTextCursorBehavior::kMoveCursorBeforeText);
  ime.CommitText(
      u"e", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  std::move(mock_ime_engine_handler_->last_passed_callback())
      .Run(ui::ime::KeyEventHandledState::kHandledByIME);

  EXPECT_EQ(fake_text_input_client.text(), u"bceda");
  EXPECT_EQ(fake_text_input_client.selection(), gfx::Range(3, 3));
}

TEST_F(InputMethodAshKeyEventTest, CommitTextEmptyRunsAfterKeyEvent) {
  FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh ime(this);
  ime.SetFocusedTextInputClient(&fake_text_input_client);
  ui::CompositionText composition;
  composition.text = u"hello";
  ime.UpdateCompositionText(composition, /*cursor_pos=*/5, /*visible=*/true);

  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::DomCode::US_A,
                     ui::EF_NONE, ui::DomKey::FromCharacter('a'),
                     ui::EventTimeForNow());
  ime.DispatchKeyEvent(&event);
  ime.CommitText(
      u"", TextInputClient::InsertTextCursorBehavior::kMoveCursorBeforeText);
  std::move(mock_ime_engine_handler_->last_passed_callback())
      .Run(ui::ime::KeyEventHandledState::kHandledByIME);

  EXPECT_EQ(fake_text_input_client.text(), u"");
  EXPECT_FALSE(fake_text_input_client.HasCompositionText());
  EXPECT_EQ(fake_text_input_client.selection(), gfx::Range(0, 0));
}

TEST_F(InputMethodAshTest, CommitTextReplacesSelection) {
  FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  fake_text_input_client.SetTextAndSelection(u"hello", gfx::Range(0, 5));
  InputMethodAsh ime(this);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  ime.CommitText(
      u"", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  EXPECT_EQ(fake_text_input_client.text(), u"");
}

TEST_F(InputMethodAshTest, ResetsEngineWithComposition) {
  FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  fake_text_input_client.SetTextAndSelection(u"hello ", gfx::Range(6, 6));
  InputMethodAsh ime(this);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  ui::CompositionText composition;
  composition.text = u"world";
  ime.UpdateCompositionText(composition, /*cursor_pos=*/5, /*visible=*/true);
  ime.CancelComposition(&fake_text_input_client);

  EXPECT_EQ(mock_ime_engine_handler_->reset_call_count(), 1);
}

TEST_F(InputMethodAshTest, DoesNotResetEngineWithNoComposition) {
  FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh ime(this);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  ime.CommitText(
      u"hello",
      TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  ime.CancelComposition(&fake_text_input_client);

  EXPECT_EQ(mock_ime_engine_handler_->reset_call_count(), 0);
}

TEST_F(InputMethodAshTest, CommitTextThenKeyEventOnlyInsertsOnce) {
  FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh ime(this);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  ime.CommitText(
      u"a", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::DomCode::US_A,
                     ui::EF_NONE, ui::DomKey::FromCharacter('a'),
                     ui::EventTimeForNow());
  ime.DispatchKeyEvent(&event);
  std::move(mock_ime_engine_handler_->last_passed_callback())
      .Run(ui::ime::KeyEventHandledState::kHandledByIME);

  EXPECT_EQ(fake_text_input_client.text(), u"a");
}

TEST_F(InputMethodAshTest, AddsAndClearsGrammarFragments) {
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  std::vector<ui::GrammarFragment> fragments;
  fragments.emplace_back(gfx::Range(0, 1), "fake");
  fragments.emplace_back(gfx::Range(3, 10), "test");
  input_method_ash_->AddGrammarFragments(fragments);
  EXPECT_EQ(get_grammar_fragments(), fragments);
  input_method_ash_->ClearGrammarFragments(gfx::Range(0, 10));
  EXPECT_EQ(get_grammar_fragments().size(), 0u);
}

TEST_F(InputMethodAshTest, GetsGrammarFragments) {
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  ui::GrammarFragment fragment(gfx::Range(0, 5), "fake");
  input_method_ash_->AddGrammarFragments({fragment});

  input_method_ash_->SetEditableSelectionRange(gfx::Range(3, 3));
  EXPECT_EQ(input_method_ash_->GetGrammarFragmentAtCursor(), fragment);
  input_method_ash_->SetEditableSelectionRange(gfx::Range(2, 4));
  EXPECT_EQ(input_method_ash_->GetGrammarFragmentAtCursor(), fragment);

  input_method_ash_->SetEditableSelectionRange(gfx::Range(7, 7));
  EXPECT_EQ(input_method_ash_->GetGrammarFragmentAtCursor(), std::nullopt);
  input_method_ash_->SetEditableSelectionRange(gfx::Range(4, 7));
  EXPECT_EQ(input_method_ash_->GetGrammarFragmentAtCursor(), std::nullopt);
}

}  // namespace ash
