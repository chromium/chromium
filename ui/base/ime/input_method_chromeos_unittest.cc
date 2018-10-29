// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_chromeos.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <cstring>

#include "base/i18n/char_iterator.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/mock_ime_candidate_window_handler.h"
#include "ui/base/ime/chromeos/mock_ime_engine_handler.h"
#include "ui/base/ime/chromeos/mock_input_method_manager.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/ime_engine_handler_interface.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/rect.h"

using base::UTF8ToUTF16;
using base::UTF16ToUTF8;

namespace ui {
namespace {

const base::string16 kSampleText = base::UTF8ToUTF16(
    "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A");

typedef IMEEngineHandlerInterface::KeyEventDoneCallback KeyEventCallback;

uint32_t GetOffsetInUTF16(const base::string16& utf16_string,
                          uint32_t utf8_offset) {
  DCHECK_LT(utf8_offset, utf16_string.size());
  base::i18n::UTF16CharIterator char_iterator(&utf16_string);
  for (size_t i = 0; i < utf8_offset; ++i)
    char_iterator.Advance();
  return char_iterator.array_pos();
}

}  // namespace


class TestableInputMethodChromeOS : public InputMethodChromeOS {
 public:
  explicit TestableInputMethodChromeOS(internal::InputMethodDelegate* delegate)
      : InputMethodChromeOS(delegate),
        process_key_event_post_ime_call_count_(0) {
  }

  struct ProcessKeyEventPostIMEArgs {
    ProcessKeyEventPostIMEArgs()
        : event(ET_UNKNOWN, VKEY_UNKNOWN, DomCode::NONE, EF_NONE),
          handled(false) {}
    ui::KeyEvent event;
    bool handled;
  };

  // Overridden from InputMethodChromeOS:
  ui::EventDispatchDetails ProcessKeyEventPostIME(ui::KeyEvent* key_event,
                                                  AckCallback ack_callback,
                                                  bool skip_process_filtered,
                                                  bool handled) override {
    ui::EventDispatchDetails details =
        InputMethodChromeOS::ProcessKeyEventPostIME(
            key_event, std::move(ack_callback), skip_process_filtered, handled);
    if (!skip_process_filtered) {
      process_key_event_post_ime_args_.event = *key_event;
      process_key_event_post_ime_args_.handled = handled;
      ++process_key_event_post_ime_call_count_;
    }
    return details;
  }

  void ResetCallCount() {
    process_key_event_post_ime_call_count_ = 0;
  }

  const ProcessKeyEventPostIMEArgs& process_key_event_post_ime_args() const {
    return process_key_event_post_ime_args_;
  }

  int process_key_event_post_ime_call_count() const {
    return process_key_event_post_ime_call_count_;
  }

  // Change access rights for testing.
  using InputMethodChromeOS::ExtractCompositionText;
  using InputMethodChromeOS::ResetContext;

 private:
  ProcessKeyEventPostIMEArgs process_key_event_post_ime_args_;
  int process_key_event_post_ime_call_count_;
};

class SetSurroundingTextVerifier {
 public:
  SetSurroundingTextVerifier(const std::string& expected_surrounding_text,
                             uint32_t expected_cursor_position,
                             uint32_t expected_anchor_position)
      : expected_surrounding_text_(expected_surrounding_text),
        expected_cursor_position_(expected_cursor_position),
        expected_anchor_position_(expected_anchor_position) {}

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

  DISALLOW_COPY_AND_ASSIGN(SetSurroundingTextVerifier);
};

class TestInputMethodManager
    : public chromeos::input_method::MockInputMethodManager {
  class TestState : public MockInputMethodManager::State {
   public:
    TestState() { Reset(); }

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
    ~TestState() override {}

   private:
    bool is_jp_kbd_ = false;
    bool is_jp_ime_ = false;

    DISALLOW_COPY_AND_ASSIGN(TestState);
  };

 private:
  scoped_refptr<TestState> state_;

 public:
  TestInputMethodManager() {
    state_ = scoped_refptr<TestState>(new TestState());
  }

  TestState* state() { return state_.get(); }

  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override {
    return scoped_refptr<InputMethodManager::State>(state_.get());
  }

  DISALLOW_COPY_AND_ASSIGN(TestInputMethodManager);
};

class InputMethodChromeOSTest : public internal::InputMethodDelegate,
                                public testing::Test,
                                public DummyTextInputClient {
 public:
  InputMethodChromeOSTest()
      : dispatched_key_event_(ui::ET_UNKNOWN, ui::VKEY_UNKNOWN, ui::EF_NONE),
        stop_propagation_post_ime_(false) {
    ResetFlags();
  }

  ~InputMethodChromeOSTest() override {}

  void SetUp() override {
    IMEBridge::Initialize();

    mock_ime_engine_handler_.reset(
        new chromeos::MockIMEEngineHandler());
    IMEBridge::Get()->SetCurrentEngineHandler(
        mock_ime_engine_handler_.get());

    mock_ime_candidate_window_handler_.reset(
        new chromeos::MockIMECandidateWindowHandler());
    IMEBridge::Get()->SetCandidateWindowHandler(
        mock_ime_candidate_window_handler_.get());

    ime_.reset(new TestableInputMethodChromeOS(this));
    ime_->SetFocusedTextInputClient(this);

    // InputMethodManager owns and delete it in InputMethodManager::Shutdown().
    input_method_manager_ = new TestInputMethodManager();
    chromeos::input_method::InputMethodManager::Initialize(
        input_method_manager_);
  }

  void TearDown() override {
    if (ime_.get())
      ime_->SetFocusedTextInputClient(NULL);
    ime_.reset();
    IMEBridge::Get()->SetCurrentEngineHandler(NULL);
    IMEBridge::Get()->SetCandidateWindowHandler(NULL);
    mock_ime_engine_handler_.reset();
    mock_ime_candidate_window_handler_.reset();
    IMEBridge::Shutdown();
    chromeos::input_method::InputMethodManager::Shutdown();

    ResetFlags();
  }

  // Overridden from ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event,
      base::OnceCallback<void(bool)> ack_callback) override {
    dispatched_key_event_ = *event;
    if (stop_propagation_post_ime_)
      event->StopPropagation();
    CallDispatchKeyEventPostIMEAck(event, std::move(ack_callback));
    return ui::EventDispatchDetails();
  }

  // Overridden from ui::TextInputClient:
  void SetCompositionText(const CompositionText& composition) override {
    composition_text_ = composition;
  }
  void ConfirmCompositionText() override {
    confirmed_text_ = composition_text_;
    composition_text_ = CompositionText();
  }
  void ClearCompositionText() override {
    composition_text_ = CompositionText();
  }
  void InsertText(const base::string16& text) override {
    inserted_text_ = text;
  }
  void InsertChar(const KeyEvent& event) override {
    inserted_char_ = event.GetCharacter();
    inserted_char_flags_ = event.flags();
  }
  TextInputType GetTextInputType() const override { return input_type_; }
  TextInputMode GetTextInputMode() const override { return input_mode_; }
  bool CanComposeInline() const override { return can_compose_inline_; }
  gfx::Rect GetCaretBounds() const override { return caret_bounds_; }
  bool HasCompositionText() const override {
    return composition_text_ != CompositionText();
  }
  bool GetTextRange(gfx::Range* range) const override {
    *range = text_range_;
    return true;
  }
  bool GetSelectionRange(gfx::Range* range) const override {
    *range = selection_range_;
    return true;
  }
  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override {
    *text = surrounding_text_.substr(range.GetMin(), range.length());
    return true;
  }
  void OnInputMethodChanged() override {
    ++on_input_method_changed_call_count_;
  }

  bool HasNativeEvent() const {
    return dispatched_key_event_.HasNativeEvent();
  }

  void ResetFlags() {
    dispatched_key_event_ = ui::KeyEvent(ui::ET_UNKNOWN, ui::VKEY_UNKNOWN,
                                         ui::EF_NONE);

    composition_text_ = CompositionText();
    confirmed_text_ = CompositionText();
    inserted_text_.clear();
    inserted_char_ = 0;
    inserted_char_flags_ = 0;
    on_input_method_changed_call_count_ = 0;

    input_type_ = TEXT_INPUT_TYPE_NONE;
    input_mode_ = TEXT_INPUT_MODE_DEFAULT;
    can_compose_inline_ = true;
    caret_bounds_ = gfx::Rect();
  }

  std::unique_ptr<TestableInputMethodChromeOS> ime_;

  // Copy of the dispatched key event.
  ui::KeyEvent dispatched_key_event_;

  // Variables for remembering the parameters that are passed to
  // ui::TextInputClient functions.
  CompositionText composition_text_;
  CompositionText confirmed_text_;
  base::string16 inserted_text_;
  base::char16 inserted_char_;
  unsigned int on_input_method_changed_call_count_;
  int inserted_char_flags_;

  // Variables that will be returned from the ui::TextInputClient functions.
  TextInputType input_type_;
  TextInputMode input_mode_;
  bool can_compose_inline_;
  gfx::Rect caret_bounds_;
  gfx::Range text_range_;
  gfx::Range selection_range_;
  base::string16 surrounding_text_;

  std::unique_ptr<chromeos::MockIMEEngineHandler> mock_ime_engine_handler_;
  std::unique_ptr<chromeos::MockIMECandidateWindowHandler>
      mock_ime_candidate_window_handler_;

  bool stop_propagation_post_ime_;

  TestInputMethodManager* input_method_manager_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodChromeOSTest);
};

// Tests public APIs in ui::InputMethod first.

TEST_F(InputMethodChromeOSTest, GetInputTextType) {
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(TEXT_INPUT_TYPE_PASSWORD, ime_->GetTextInputType());
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(TEXT_INPUT_TYPE_TEXT, ime_->GetTextInputType());
}

TEST_F(InputMethodChromeOSTest, CanComposeInline) {
  EXPECT_TRUE(ime_->CanComposeInline());
  can_compose_inline_ = false;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_FALSE(ime_->CanComposeInline());
}

TEST_F(InputMethodChromeOSTest, GetTextInputClient) {
  EXPECT_EQ(this, ime_->GetTextInputClient());
  ime_->SetFocusedTextInputClient(NULL);
  EXPECT_EQ(NULL, ime_->GetTextInputClient());
}

TEST_F(InputMethodChromeOSTest, GetInputTextType_WithoutFocusedClient) {
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());
  ime_->SetFocusedTextInputClient(NULL);
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  // The OnTextInputTypeChanged() call above should be ignored since |this| is
  // not the current focused client.
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());

  ime_->SetFocusedTextInputClient(this);
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(TEXT_INPUT_TYPE_PASSWORD, ime_->GetTextInputType());
}

// Confirm that IBusClient::FocusIn is called on "connected" if input_type_ is
// TEXT.
TEST_F(InputMethodChromeOSTest, FocusIn_Text) {
  // A context shouldn't be created since the daemon is not running.
  EXPECT_EQ(0U, on_input_method_changed_call_count_);
  // Click a text input form.
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);
  // Since a form has focus, IBusClient::FocusIn() should be called.
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(
      1,
      mock_ime_candidate_window_handler_->set_cursor_bounds_call_count());
  // ui::TextInputClient::OnInputMethodChanged() should be called when
  // ui::InputMethodChromeOS connects/disconnects to/from ibus-daemon and the
  // current text input type is not NONE.
  EXPECT_EQ(1U, on_input_method_changed_call_count_);
}

// Confirm that InputMethodEngine::FocusIn is called on "connected" even if
// input_type_ is PASSWORD.
TEST_F(InputMethodChromeOSTest, FocusIn_Password) {
  EXPECT_EQ(0U, on_input_method_changed_call_count_);
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  // InputMethodEngine::FocusIn() should be called even for password field.
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(1U, on_input_method_changed_call_count_);
}

// Confirm that IBusClient::FocusOut is called as expected.
TEST_F(InputMethodChromeOSTest, FocusOut_None) {
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(0, mock_ime_engine_handler_->focus_out_call_count());
  input_type_ = TEXT_INPUT_TYPE_NONE;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_out_call_count());
}

// Confirm that IBusClient::FocusOut is called as expected.
TEST_F(InputMethodChromeOSTest, FocusOut_Password) {
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(0, mock_ime_engine_handler_->focus_out_call_count());
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(2, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_out_call_count());
}

// FocusIn/FocusOut scenario test
TEST_F(InputMethodChromeOSTest, Focus_Scenario) {
  // Confirm that both FocusIn and FocusOut are NOT called.
  EXPECT_EQ(0, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(0, mock_ime_engine_handler_->focus_out_call_count());
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE,
            mock_ime_engine_handler_->last_text_input_context().type);
  EXPECT_EQ(TEXT_INPUT_MODE_DEFAULT,
            mock_ime_engine_handler_->last_text_input_context().mode);

  input_type_ = TEXT_INPUT_TYPE_TEXT;
  input_mode_ = TEXT_INPUT_MODE_TEXT;
  ime_->OnTextInputTypeChanged(this);
  // Confirm that only FocusIn is called, the TextInputType is TEXT and the
  // TextInputMode is LATIN..
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(0, mock_ime_engine_handler_->focus_out_call_count());
  EXPECT_EQ(TEXT_INPUT_TYPE_TEXT,
            mock_ime_engine_handler_->last_text_input_context().type);
  EXPECT_EQ(TEXT_INPUT_MODE_TEXT,
            mock_ime_engine_handler_->last_text_input_context().mode);

  input_mode_ = TEXT_INPUT_MODE_SEARCH;
  ime_->OnTextInputTypeChanged(this);
  // Confirm that both FocusIn and FocusOut are called for mode change.
  EXPECT_EQ(2, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_out_call_count());
  EXPECT_EQ(TEXT_INPUT_TYPE_TEXT,
            mock_ime_engine_handler_->last_text_input_context().type);
  EXPECT_EQ(TEXT_INPUT_MODE_SEARCH,
            mock_ime_engine_handler_->last_text_input_context().mode);

  input_type_ = TEXT_INPUT_TYPE_URL;
  ime_->OnTextInputTypeChanged(this);
  // Confirm that both FocusIn and FocusOut are called and the TextInputType is
  // changed to URL.
  EXPECT_EQ(3, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(2, mock_ime_engine_handler_->focus_out_call_count());
  EXPECT_EQ(TEXT_INPUT_TYPE_URL,
            mock_ime_engine_handler_->last_text_input_context().type);
  EXPECT_EQ(TEXT_INPUT_MODE_SEARCH,
            mock_ime_engine_handler_->last_text_input_context().mode);

  // Confirm that FocusOut is called when set focus to NULL client.
  ime_->SetFocusedTextInputClient(NULL);
  EXPECT_EQ(3, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(3, mock_ime_engine_handler_->focus_out_call_count());
  // Confirm that FocusIn is called when set focus to this client.
  ime_->SetFocusedTextInputClient(this);
  EXPECT_EQ(4, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(3, mock_ime_engine_handler_->focus_out_call_count());
}

// Test if the new |caret_bounds_| is correctly sent to ibus-daemon.
TEST_F(InputMethodChromeOSTest, OnCaretBoundsChanged) {
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(
      1,
      mock_ime_candidate_window_handler_->set_cursor_bounds_call_count());
  caret_bounds_ = gfx::Rect(1, 2, 3, 4);
  ime_->OnCaretBoundsChanged(this);
  EXPECT_EQ(
      2,
      mock_ime_candidate_window_handler_->set_cursor_bounds_call_count());
  caret_bounds_ = gfx::Rect(0, 2, 3, 4);
  ime_->OnCaretBoundsChanged(this);
  EXPECT_EQ(
      3,
      mock_ime_candidate_window_handler_->set_cursor_bounds_call_count());
  caret_bounds_ = gfx::Rect(0, 2, 3, 4);  // unchanged
  ime_->OnCaretBoundsChanged(this);
  // Current InputMethodChromeOS implementation performs the IPC
  // regardless of the bounds are changed or not.
  EXPECT_EQ(
      4,
      mock_ime_candidate_window_handler_->set_cursor_bounds_call_count());
}

TEST_F(InputMethodChromeOSTest, ExtractCompositionTextTest_NoAttribute) {
  const base::string16 kSampleAsciiText = UTF8ToUTF16("Sample Text");
  const uint32_t kCursorPos = 2UL;

  CompositionText chromeos_composition_text;
  chromeos_composition_text.text = kSampleAsciiText;

  CompositionText composition_text;
  ime_->ExtractCompositionText(
      chromeos_composition_text, kCursorPos, &composition_text);
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

TEST_F(InputMethodChromeOSTest, ExtractCompositionTextTest_SingleUnderline) {
  const uint32_t kCursorPos = 2UL;

  // Set up chromeos composition text with one underline attribute.
  CompositionText composition_text;
  composition_text.text = kSampleText;
  ImeTextSpan underline(ImeTextSpan::Type::kComposition, 1UL, 4UL,
                        ui::ImeTextSpan::Thickness::kThin, SK_ColorTRANSPARENT);
  composition_text.ime_text_spans.push_back(underline);

  CompositionText composition_text2;
  ime_->ExtractCompositionText(composition_text, kCursorPos,
                               &composition_text2);
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

TEST_F(InputMethodChromeOSTest, ExtractCompositionTextTest_DoubleUnderline) {
  const uint32_t kCursorPos = 2UL;

  // Set up chromeos composition text with one underline attribute.
  CompositionText composition_text;
  composition_text.text = kSampleText;
  ImeTextSpan underline(ImeTextSpan::Type::kComposition, 1UL, 4UL,
                        ui::ImeTextSpan::Thickness::kThick,
                        SK_ColorTRANSPARENT);
  composition_text.ime_text_spans.push_back(underline);

  CompositionText composition_text2;
  ime_->ExtractCompositionText(composition_text, kCursorPos,
                               &composition_text2);
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

TEST_F(InputMethodChromeOSTest, ExtractCompositionTextTest_ErrorUnderline) {
  const uint32_t kCursorPos = 2UL;

  // Set up chromeos composition text with one underline attribute.
  CompositionText composition_text;
  composition_text.text = kSampleText;
  ImeTextSpan underline(ImeTextSpan::Type::kComposition, 1UL, 4UL,
                        ui::ImeTextSpan::Thickness::kThin, SK_ColorTRANSPARENT);
  underline.underline_color = SK_ColorRED;
  composition_text.ime_text_spans.push_back(underline);

  CompositionText composition_text2;
  ime_->ExtractCompositionText(composition_text, kCursorPos,
                               &composition_text2);
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

TEST_F(InputMethodChromeOSTest, ExtractCompositionTextTest_Selection) {
  const uint32_t kCursorPos = 2UL;

  // Set up chromeos composition text with one underline attribute.
  CompositionText composition_text;
  composition_text.text = kSampleText;
  composition_text.selection.set_start(1UL);
  composition_text.selection.set_end(4UL);

  CompositionText composition_text2;
  ime_->ExtractCompositionText(composition_text, kCursorPos,
                               &composition_text2);
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

TEST_F(InputMethodChromeOSTest,
       ExtractCompositionTextTest_SelectionStartWithCursor) {
  const uint32_t kCursorPos = 1UL;

  // Set up chromeos composition text with one underline attribute.
  CompositionText composition_text;
  composition_text.text = kSampleText;
  composition_text.selection.set_start(kCursorPos);
  composition_text.selection.set_end(4UL);

  CompositionText composition_text2;
  ime_->ExtractCompositionText(composition_text, kCursorPos,
                               &composition_text2);
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

TEST_F(InputMethodChromeOSTest,
       ExtractCompositionTextTest_SelectionEndWithCursor) {
  const uint32_t kCursorPos = 4UL;

  // Set up chromeos composition text with one underline attribute.
  CompositionText composition_text;
  composition_text.text = kSampleText;
  composition_text.selection.set_start(1UL);
  composition_text.selection.set_end(kCursorPos);

  CompositionText composition_text2;
  ime_->ExtractCompositionText(composition_text, kCursorPos,
                               &composition_text2);
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

TEST_F(InputMethodChromeOSTest, SurroundingText_NoSelectionTest) {
  // Click a text input form.
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);

  // Set the TextInputClient behaviors.
  surrounding_text_ = UTF8ToUTF16("abcdef");
  text_range_ = gfx::Range(0, 6);
  selection_range_ = gfx::Range(3, 3);

  // Set the verifier for SetSurroundingText mock call.
  SetSurroundingTextVerifier verifier(UTF16ToUTF8(surrounding_text_), 3, 3);


  ime_->OnCaretBoundsChanged(this);

  // Check the call count.
  EXPECT_EQ(1,
            mock_ime_engine_handler_->set_surrounding_text_call_count());
  EXPECT_EQ(UTF16ToUTF8(surrounding_text_),
            mock_ime_engine_handler_->last_set_surrounding_text());
  EXPECT_EQ(3U,
            mock_ime_engine_handler_->last_set_surrounding_cursor_pos());
  EXPECT_EQ(3U,
            mock_ime_engine_handler_->last_set_surrounding_anchor_pos());
}

TEST_F(InputMethodChromeOSTest, SurroundingText_SelectionTest) {
  // Click a text input form.
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);

  // Set the TextInputClient behaviors.
  surrounding_text_ = UTF8ToUTF16("abcdef");
  text_range_ = gfx::Range(0, 6);
  selection_range_ = gfx::Range(2, 5);

  // Set the verifier for SetSurroundingText mock call.
  SetSurroundingTextVerifier verifier(UTF16ToUTF8(surrounding_text_), 2, 5);

  ime_->OnCaretBoundsChanged(this);

  // Check the call count.
  EXPECT_EQ(1,
            mock_ime_engine_handler_->set_surrounding_text_call_count());
  EXPECT_EQ(UTF16ToUTF8(surrounding_text_),
            mock_ime_engine_handler_->last_set_surrounding_text());
  EXPECT_EQ(2U,
            mock_ime_engine_handler_->last_set_surrounding_cursor_pos());
  EXPECT_EQ(5U,
            mock_ime_engine_handler_->last_set_surrounding_anchor_pos());
}

TEST_F(InputMethodChromeOSTest, SurroundingText_PartialText) {
  // Click a text input form.
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);

  // Set the TextInputClient behaviors.
  surrounding_text_ = UTF8ToUTF16("abcdefghij");
  text_range_ = gfx::Range(5, 10);
  selection_range_ = gfx::Range(7, 9);

  ime_->OnCaretBoundsChanged(this);

  // Check the call count.
  EXPECT_EQ(1,
            mock_ime_engine_handler_->set_surrounding_text_call_count());
  // Set the verifier for SetSurroundingText mock call.
  // Here (2, 4) is selection range in expected surrounding text coordinates.
  EXPECT_EQ("fghij",
            mock_ime_engine_handler_->last_set_surrounding_text());
  EXPECT_EQ(2U,
            mock_ime_engine_handler_->last_set_surrounding_cursor_pos());
  EXPECT_EQ(4U,
            mock_ime_engine_handler_->last_set_surrounding_anchor_pos());
}

TEST_F(InputMethodChromeOSTest, SurroundingText_BecomeEmptyText) {
  // Click a text input form.
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);

  // Set the TextInputClient behaviors.
  // If the surrounding text becomes empty, text_range become (0, 0) and
  // selection range become invalid.
  surrounding_text_ = UTF8ToUTF16("");
  text_range_ = gfx::Range(0, 0);
  selection_range_ = gfx::Range::InvalidRange();

  ime_->OnCaretBoundsChanged(this);

  // Check the call count.
  EXPECT_EQ(0,
            mock_ime_engine_handler_->set_surrounding_text_call_count());

  // Should not be called twice with same condition.
  ime_->OnCaretBoundsChanged(this);
  EXPECT_EQ(0,
            mock_ime_engine_handler_->set_surrounding_text_call_count());
}

class InputMethodChromeOSKeyEventTest : public InputMethodChromeOSTest {
 public:
  InputMethodChromeOSKeyEventTest() {}
  ~InputMethodChromeOSKeyEventTest() override {}

  DISALLOW_COPY_AND_ASSIGN(InputMethodChromeOSKeyEventTest);
};

TEST_F(InputMethodChromeOSKeyEventTest, KeyEventDelayResponseTest) {
  const int kFlags = ui::EF_SHIFT_DOWN;
  ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_A, kFlags);

  // Do key event.
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);
  ime_->DispatchKeyEvent(&event);

  // Check before state.
  const ui::KeyEvent* key_event =
      mock_ime_engine_handler_->last_processed_key_event();
  EXPECT_EQ(1, mock_ime_engine_handler_->process_key_event_call_count());
  EXPECT_EQ(ui::VKEY_A, key_event->key_code());
  EXPECT_EQ(kFlags, key_event->flags());
  EXPECT_EQ(0, ime_->process_key_event_post_ime_call_count());

  (static_cast<IMEInputContextHandlerInterface*>(ime_.get()))
      ->CommitText("A");

  EXPECT_EQ(0, inserted_char_);

  // Do callback.
  std::move(mock_ime_engine_handler_->last_passed_callback()).Run(true);

  // Check the results
  EXPECT_EQ(1, ime_->process_key_event_post_ime_call_count());
  const ui::KeyEvent stored_event =
      ime_->process_key_event_post_ime_args().event;
  EXPECT_EQ(ui::VKEY_A, stored_event.key_code());
  EXPECT_EQ(kFlags, stored_event.flags());
  EXPECT_TRUE(ime_->process_key_event_post_ime_args().handled);

  EXPECT_EQ(L'A', inserted_char_);
}

TEST_F(InputMethodChromeOSKeyEventTest, MultiKeyEventDelayResponseTest) {
  // Preparation
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);

  const int kFlags = ui::EF_SHIFT_DOWN;
  ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_B, kFlags);

  // Do key event.
  ime_->DispatchKeyEvent(&event);
  const ui::KeyEvent* key_event =
      mock_ime_engine_handler_->last_processed_key_event();
  EXPECT_EQ(ui::VKEY_B, key_event->key_code());
  EXPECT_EQ(kFlags, key_event->flags());

  KeyEventCallback first_callback =
      mock_ime_engine_handler_->last_passed_callback();

  // Do key event again.
  ui::KeyEvent event2(ui::ET_KEY_PRESSED, ui::VKEY_C, kFlags);

  ime_->DispatchKeyEvent(&event2);
  const ui::KeyEvent* key_event2 =
      mock_ime_engine_handler_->last_processed_key_event();
  EXPECT_EQ(ui::VKEY_C, key_event2->key_code());
  EXPECT_EQ(kFlags, key_event2->flags());

  // Check before state.
  EXPECT_EQ(2,
            mock_ime_engine_handler_->process_key_event_call_count());
  EXPECT_EQ(0, ime_->process_key_event_post_ime_call_count());

  CompositionText comp;
  comp.text = base::ASCIIToUTF16("B");
  (static_cast<IMEInputContextHandlerInterface*>(ime_.get()))
      ->UpdateCompositionText(comp, comp.text.length(), true);

  EXPECT_EQ(0, composition_text_.text[0]);

  // Do callback for first key event.
  std::move(first_callback).Run(true);

  EXPECT_EQ(comp.text, composition_text_.text);

  // Check the results for first key event.
  EXPECT_EQ(1, ime_->process_key_event_post_ime_call_count());
  ui::KeyEvent stored_event = ime_->process_key_event_post_ime_args().event;
  EXPECT_EQ(ui::VKEY_B, stored_event.key_code());
  EXPECT_EQ(kFlags, stored_event.flags());
  EXPECT_TRUE(ime_->process_key_event_post_ime_args().handled);
  EXPECT_EQ(0, inserted_char_);

  // Do callback for second key event.
  mock_ime_engine_handler_->last_passed_callback().Run(false);

  // Check the results for second key event.
  EXPECT_EQ(2, ime_->process_key_event_post_ime_call_count());
  stored_event = ime_->process_key_event_post_ime_args().event;
  EXPECT_EQ(ui::VKEY_C, stored_event.key_code());
  EXPECT_EQ(kFlags, stored_event.flags());
  EXPECT_FALSE(ime_->process_key_event_post_ime_args().handled);

  EXPECT_EQ(L'C', inserted_char_);
}

TEST_F(InputMethodChromeOSKeyEventTest, StopPropagationTest) {
  // Preparation
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);

  // Do key event with event being stopped propagation.
  stop_propagation_post_ime_ = true;
  ui::KeyEvent eventA(ui::ET_KEY_PRESSED, ui::VKEY_A, EF_NONE);
  eventA.set_character(L'A');
  ime_->DispatchKeyEvent(&eventA);
  mock_ime_engine_handler_->last_passed_callback().Run(false);

  const ui::KeyEvent* key_event =
      mock_ime_engine_handler_->last_processed_key_event();
  EXPECT_EQ(ui::VKEY_A, key_event->key_code());
  EXPECT_EQ(0, inserted_char_);

  // Do key event with event not being stopped propagation.
  stop_propagation_post_ime_ = false;
  ime_->DispatchKeyEvent(&eventA);
  mock_ime_engine_handler_->last_passed_callback().Run(false);

  key_event = mock_ime_engine_handler_->last_processed_key_event();
  EXPECT_EQ(ui::VKEY_A, key_event->key_code());
  EXPECT_EQ(L'A', inserted_char_);
}

TEST_F(InputMethodChromeOSKeyEventTest, DeadKeyPressTest) {
  // Preparation.
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);

  ui::KeyEvent eventA(ET_KEY_PRESSED,
                      VKEY_OEM_4, // '['
                      DomCode::BRACKET_LEFT,
                      0,
                      DomKey::DeadKeyFromCombiningCharacter('^'),
                      EventTimeForNow());
  ime_->ProcessKeyEventPostIME(&eventA, InputMethodChromeOS::AckCallback(),
                               false, true);

  const ui::KeyEvent& key_event = dispatched_key_event_;

  EXPECT_EQ(ET_KEY_PRESSED, key_event.type());
  EXPECT_EQ(VKEY_PROCESSKEY, key_event.key_code());
  EXPECT_EQ(eventA.code(), key_event.code());
  EXPECT_EQ(eventA.flags(), key_event.flags());
  EXPECT_EQ(eventA.GetDomKey(), key_event.GetDomKey());
  EXPECT_EQ(eventA.time_stamp(), key_event.time_stamp());
}

TEST_F(InputMethodChromeOSKeyEventTest, JP106KeyTest) {
  ui::KeyEvent eventConvert(ET_KEY_PRESSED, VKEY_CONVERT, EF_NONE);
  ime_->DispatchKeyEvent(&eventConvert);
  EXPECT_FALSE(input_method_manager_->state()->is_jp_kbd());
  EXPECT_TRUE(input_method_manager_->state()->is_jp_ime());

  ui::KeyEvent eventNonConvert(ET_KEY_PRESSED, VKEY_NONCONVERT, EF_NONE);
  ime_->DispatchKeyEvent(&eventNonConvert);
  EXPECT_TRUE(input_method_manager_->state()->is_jp_kbd());
  EXPECT_FALSE(input_method_manager_->state()->is_jp_ime());

  ui::KeyEvent eventDbeSbc(ET_KEY_PRESSED, VKEY_DBE_SBCSCHAR, EF_NONE);
  ime_->DispatchKeyEvent(&eventDbeSbc);
  EXPECT_FALSE(input_method_manager_->state()->is_jp_kbd());
  EXPECT_TRUE(input_method_manager_->state()->is_jp_ime());

  ui::KeyEvent eventDbeDbc(ET_KEY_PRESSED, VKEY_DBE_DBCSCHAR, EF_NONE);
  ime_->DispatchKeyEvent(&eventDbeDbc);
  EXPECT_TRUE(input_method_manager_->state()->is_jp_kbd());
  EXPECT_FALSE(input_method_manager_->state()->is_jp_ime());
}

}  // namespace ui
