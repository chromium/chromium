// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/linux/input_method_auralinux.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/virtual_keyboard_controller_stub.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ui {
namespace {

const char16_t kActionCommit = L'C';
const char16_t kActionCompositionStart = L'S';
const char16_t kActionCompositionUpdate = L'U';
const char16_t kActionCompositionEnd = L'E';

class TestResult {
 public:
  static TestResult* GetInstance() {
    return base::Singleton<TestResult>::get();
  }

  void RecordAction(const std::u16string& action) {
    recorded_actions_.push_back(action);
  }

  void ExpectAction(const std::string& action) {
    expected_actions_.push_back(base::ASCIIToUTF16(action));
  }

  void Verify() {
    size_t len = recorded_actions_.size();
    size_t len_exp = expected_actions_.size();
    EXPECT_EQ(len_exp, len);
    for (size_t i = 0; i < len; i++)
      EXPECT_EQ(expected_actions_[i], recorded_actions_[i]);
    recorded_actions_.clear();
    expected_actions_.clear();
  }

 private:
  std::vector<std::u16string> recorded_actions_;
  std::vector<std::u16string> expected_actions_;
};

class LinuxInputMethodContextForTesting : public LinuxInputMethodContext {
 public:
  explicit LinuxInputMethodContextForTesting(
      LinuxInputMethodContextDelegate* delegate)
      : delegate_(delegate), is_sync_mode_(false), eat_key_(false) {}

  LinuxInputMethodContextForTesting(const LinuxInputMethodContextForTesting&) =
      delete;
  LinuxInputMethodContextForTesting& operator=(
      const LinuxInputMethodContextForTesting&) = delete;

  void SetSyncMode(bool is_sync_mode) { is_sync_mode_ = is_sync_mode; }
  void SetEatKey(bool eat_key) { eat_key_ = eat_key; }

  void AddCommitAction(const std::string& text) {
    actions_.push_back(u"C:" + base::ASCIIToUTF16(text));
  }

  void AddCompositionUpdateAction(const std::string& text) {
    actions_.push_back(u"U:" + base::ASCIIToUTF16(text));
  }

  void AddCompositionStartAction() { actions_.push_back(u"S"); }

  void AddCompositionEndAction() { actions_.push_back(u"E"); }

  VirtualKeyboardController* GetVirtualKeyboardController() override {
    return &virtual_keyboard_controller_;
  }

  TextInputType input_type() const { return input_type_; }
  TextInputMode input_mode() const { return input_mode_; }
  uint32_t input_flags() const { return input_flags_; }
  bool should_do_learning() const { return should_do_learning_; }
  TextInputClient* old_client() { return old_client_; }
  TextInputClient* new_client() { return new_client_; }

 protected:
  bool DispatchKeyEvent(const ui::KeyEvent& key_event) override {
    if (!is_sync_mode_) {
      actions_.clear();
      return eat_key_;
    }

    for (const auto& action : actions_) {
      std::vector<std::u16string> parts =
          base::SplitString(action, std::u16string(1, ':'),
                            base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      char16_t id = parts[0][0];
      std::u16string param;
      if (parts.size() > 1)
        param = parts[1];
      if (id == kActionCommit) {
        delegate_->OnCommit(param);
      } else if (id == kActionCompositionStart) {
        delegate_->OnPreeditStart();
      } else if (id == kActionCompositionUpdate) {
        CompositionText comp;
        comp.text = param;
        delegate_->OnPreeditChanged(comp);
      } else if (id == kActionCompositionEnd) {
        delegate_->OnPreeditEnd();
      }
    }

    actions_.clear();
    return eat_key_;
  }

  bool IsPeekKeyEvent(const ui::KeyEvent& key_event) override {
    const auto* properties = key_event.properties();
    // For the purposes of tests if kPropertyKeyboardImeFlag is not
    // explicitly set assume the event is not a key event.
    if (!properties)
      return true;
    auto it = properties->find(kPropertyKeyboardImeFlag);
    if (it == properties->end())
      return true;
    return !(it->second[0] & kPropertyKeyboardImeIgnoredFlag);
  }

  void Reset() override {}

  void WillUpdateFocus(TextInputClient* old_client,
                       TextInputClient* new_client) override {
    old_client_ = old_client;
    new_client_ = new_client;
  }

  void UpdateFocus(bool has_client,
                   TextInputType old_type,
                   TextInputType new_type) override {}

  void SetCursorLocation(const gfx::Rect& rect) override {
    cursor_position_ = rect;
  }

  void SetSurroundingText(const std::u16string& text,
                          const gfx::Range& selection_range) override {
    TestResult::GetInstance()->RecordAction(u"surroundingtext:" + text);

    std::stringstream rs;
    rs << "selectionrangestart:" << selection_range.start();
    std::stringstream re;
    re << "selectionrangeend:" << selection_range.end();
    TestResult::GetInstance()->RecordAction(base::ASCIIToUTF16(rs.str()));
    TestResult::GetInstance()->RecordAction(base::ASCIIToUTF16(re.str()));
  }

  void SetContentType(TextInputType type,
                      TextInputMode mode,
                      uint32_t flags,
                      bool should_do_learning) override {
    input_type_ = type;
    input_mode_ = mode;
    input_flags_ = flags;
    should_do_learning_ = should_do_learning;
  }

  void SetGrammarFragmentAtCursor(
      const ui::GrammarFragment& fragment) override {}
  void SetAutocorrectInfo(const gfx::Range& autocorrect_range,
                          const gfx::Rect& autocorrect_bounds) override {}

 private:
  raw_ptr<LinuxInputMethodContextDelegate> delegate_;
  VirtualKeyboardControllerStub virtual_keyboard_controller_;
  std::vector<std::u16string> actions_;
  bool is_sync_mode_;
  bool eat_key_;
  gfx::Rect cursor_position_;
  TextInputType input_type_;
  TextInputMode input_mode_;
  uint32_t input_flags_;
  bool should_do_learning_;
  raw_ptr<TextInputClient> old_client_ = nullptr;
  raw_ptr<TextInputClient> new_client_ = nullptr;
};

class InputMethodDelegateForTesting : public ImeKeyEventDispatcher {
 public:
  InputMethodDelegateForTesting() {}

  InputMethodDelegateForTesting(const InputMethodDelegateForTesting&) = delete;
  InputMethodDelegateForTesting& operator=(
      const InputMethodDelegateForTesting&) = delete;

  ~InputMethodDelegateForTesting() override {}

  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* key_event) override {
    std::string action;
    switch (key_event->type()) {
      case ET_KEY_PRESSED:
        action = "keydown:";
        break;
      case ET_KEY_RELEASED:
        action = "keyup:";
        break;
      default:
        break;
    }
    std::stringstream ss;
    ss << key_event->key_code();
    action += std::string(ss.str());
    TestResult::GetInstance()->RecordAction(base::ASCIIToUTF16(action));
    return ui::EventDispatchDetails();
  }
};

class TextInputClientForTesting : public DummyTextInputClient {
 public:
  explicit TextInputClientForTesting(TextInputType text_input_type)
      : DummyTextInputClient(text_input_type) {}

  std::u16string composition_text;
  gfx::Range text_range;
  gfx::Range selection_range;
  std::u16string surrounding_text;

  absl::optional<gfx::Rect> caret_not_in_rect;

 protected:
  void SetCompositionText(const CompositionText& composition) override {
    composition_text = composition.text;
    TestResult::GetInstance()->RecordAction(u"compositionstart");
    TestResult::GetInstance()->RecordAction(u"compositionupdate:" +
                                            composition.text);
  }

  bool HasCompositionText() const override { return !composition_text.empty(); }

  size_t ConfirmCompositionText(bool keep_selection) override {
    // TODO(b/134473433) Modify this function so that when keep_selection is
    // true, the selection is not changed when text committed
    if (keep_selection) {
      NOTIMPLEMENTED_LOG_ONCE();
    }
    TestResult::GetInstance()->RecordAction(u"compositionend");
    TestResult::GetInstance()->RecordAction(u"textinput:" + composition_text);
    const size_t composition_text_length = composition_text.length();
    composition_text.clear();
    return composition_text_length;
  }

  void ClearCompositionText() override {
    TestResult::GetInstance()->RecordAction(u"compositionend");
    composition_text.clear();
  }

  void InsertText(
      const std::u16string& text,
      TextInputClient::InsertTextCursorBehavior cursor_behavior) override {
    if (HasCompositionText()) {
      TestResult::GetInstance()->RecordAction(u"compositionend");
    }
    TestResult::GetInstance()->RecordAction(u"textinput:" + text);
    composition_text.clear();
  }

  void InsertChar(const ui::KeyEvent& event) override {
    std::stringstream ss;
    ss << static_cast<uint16_t>(event.GetCharacter());
    TestResult::GetInstance()->RecordAction(u"keypress:" +
                                            base::ASCIIToUTF16(ss.str()));
  }

  bool GetTextRange(gfx::Range* range) const override {
    *range = text_range;
    return true;
  }
  bool GetEditableSelectionRange(gfx::Range* range) const override {
    *range = selection_range;
    return true;
  }
  bool GetTextFromRange(const gfx::Range& range,
                        std::u16string* text) const override {
    if (surrounding_text.empty())
      return false;
    *text = surrounding_text.substr(range.GetMin(), range.length());
    return true;
  }

  void EnsureCaretNotInRect(const gfx::Rect& rect) override {
    caret_not_in_rect = rect;
  }
};

class InputMethodAuraLinuxTest : public testing::Test {
 public:
  InputMethodAuraLinuxTest(const InputMethodAuraLinuxTest&) = delete;
  InputMethodAuraLinuxTest& operator=(const InputMethodAuraLinuxTest&) = delete;

 protected:
  InputMethodAuraLinuxTest()
      : input_method_auralinux_(nullptr),
        delegate_(nullptr),
        context_(nullptr) {
    GetInputMethodContextFactoryForTest() =
        base::BindRepeating([](LinuxInputMethodContextDelegate* delegate)
                                -> std::unique_ptr<LinuxInputMethodContext> {
          return std::make_unique<LinuxInputMethodContextForTesting>(delegate);
        });
    test_result_ = TestResult::GetInstance();
  }
  ~InputMethodAuraLinuxTest() override {
    ShutdownInputMethodForTesting();
    test_result_ = nullptr;
  }

  void SetUp() override {
    delegate_ = new InputMethodDelegateForTesting();
    input_method_auralinux_ = new InputMethodAuraLinux(delegate_);
    input_method_auralinux_->OnFocus();
    context_ = static_cast<LinuxInputMethodContextForTesting*>(
        input_method_auralinux_->GetContextForTesting());
  }

  void TearDown() override {
    context_->SetSyncMode(false);
    context_->SetEatKey(false);
    context_ = nullptr;

    delete input_method_auralinux_;
    input_method_auralinux_ = nullptr;
    delete delegate_;
    delegate_ = nullptr;
  }

  raw_ptr<InputMethodAuraLinux> input_method_auralinux_;
  raw_ptr<InputMethodDelegateForTesting> delegate_;
  raw_ptr<LinuxInputMethodContextForTesting> context_;
  raw_ptr<TestResult> test_result_;
};

TEST_F(InputMethodAuraLinuxTest, BasicSyncModeTest) {
  context_->SetSyncMode(true);
  context_->SetEatKey(true);
  context_->AddCommitAction("a");

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  KeyEvent key_new(ET_KEY_PRESSED, VKEY_A, 0);
  key_new.set_character(L'a');

  KeyEvent key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:65");
  test_result_->ExpectAction("keypress:97");
  test_result_->Verify();

  input_method_auralinux_->DetachTextInputClient(client.get());
  client =
      std::make_unique<TextInputClientForTesting>(TEXT_INPUT_TYPE_PASSWORD);
  context_->SetEatKey(false);

  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());
  key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:65");
  test_result_->ExpectAction("keypress:97");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, BasicAsyncModeTest) {
  context_->SetSyncMode(false);
  context_->SetEatKey(true);

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());
  KeyEvent key_new(ET_KEY_PRESSED, VKEY_A, 0);
  key_new.set_character(L'a');
  KeyEvent key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);
  input_method_auralinux_->OnCommit(u"a");

  test_result_->ExpectAction("keydown:65");
  test_result_->ExpectAction("keypress:97");
  test_result_->Verify();

  key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);
  input_method_auralinux_->OnCommit(u"foo");

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("textinput:foo");
  test_result_->Verify();

  input_method_auralinux_->DetachTextInputClient(client.get());
  client =
      std::make_unique<TextInputClientForTesting>(TEXT_INPUT_TYPE_PASSWORD);
  context_->SetEatKey(false);

  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());
  key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:65");
  test_result_->ExpectAction("keypress:97");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, IBusUSTest) {
  context_->SetSyncMode(false);
  context_->SetEatKey(true);

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());
  KeyEvent key_new(ET_KEY_PRESSED, VKEY_A, 0);
  key_new.set_character(L'a');
  KeyEvent key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  // IBus mutes the key down.
  test_result_->Verify();

  // IBus simulates a faked key down and handle it in sync mode.
  context_->SetSyncMode(true);
  context_->AddCommitAction("a");
  key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:65");
  test_result_->ExpectAction("keypress:97");
  test_result_->Verify();

  // IBus does NOT handle the key up.
  context_->SetEatKey(false);
  KeyEvent key_up(ET_KEY_RELEASED, VKEY_A, 0);
  input_method_auralinux_->DispatchKeyEvent(&key_up);

  test_result_->ExpectAction("keyup:65");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, IBusPinyinTest) {
  context_->SetSyncMode(false);
  context_->SetEatKey(true);

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());
  KeyEvent key(ET_KEY_PRESSED, VKEY_A, 0);
  key.set_character(L'a');
  input_method_auralinux_->DispatchKeyEvent(&key);

  // IBus issues a standalone set_composition action.
  input_method_auralinux_->OnPreeditStart();
  CompositionText comp;
  comp.text = u"a";
  input_method_auralinux_->OnPreeditChanged(comp);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionstart");
  test_result_->ExpectAction("compositionupdate:a");
  test_result_->Verify();

  // IBus issues a commit text with composition after muting the space key down.
  KeyEvent key_up(ET_KEY_RELEASED, VKEY_SPACE, 0);
  input_method_auralinux_->DispatchKeyEvent(&key_up);

  input_method_auralinux_->OnPreeditEnd();
  input_method_auralinux_->OnCommit(u"A");

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionend");
  test_result_->ExpectAction("textinput:A");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, FcitxPinyinTest) {
  context_->SetSyncMode(false);
  context_->SetEatKey(true);

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());
  KeyEvent key(ET_KEY_PRESSED, VKEY_A, 0);
  key.set_character(L'a');
  input_method_auralinux_->DispatchKeyEvent(&key);
  input_method_auralinux_->OnPreeditStart();

  // Typing return issues a commit, then preedit end.
  // When input characters with fcitx+chinese, there has no
  // composing text and no composition updated.
  // So do nothing here is to emulate the fcitx+chinese input.
  KeyEvent key_up(ET_KEY_RELEASED, VKEY_RETURN, 0);
  input_method_auralinux_->DispatchKeyEvent(&key_up);

  input_method_auralinux_->OnCommit(u"a");
  input_method_auralinux_->OnPreeditEnd();

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("keypress:97");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, Fcitx5PinyinTest) {
  // Fcitx5 performance is consistent with ibus.
  // The composition is updated when input characters.
  context_->SetSyncMode(false);
  context_->SetEatKey(true);

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());
  KeyEvent key(ET_KEY_PRESSED, VKEY_A, 0);
  key.set_character(L'a');
  input_method_auralinux_->DispatchKeyEvent(&key);
  input_method_auralinux_->OnPreeditStart();

  CompositionText comp;
  comp.text = u"a";
  input_method_auralinux_->OnPreeditChanged(comp);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionstart");
  test_result_->ExpectAction("compositionupdate:a");
  test_result_->Verify();

  // Typing return issues a commit, followed by preedit change (to make
  // composition empty), then preedit end.
  KeyEvent key_up(ET_KEY_RELEASED, VKEY_RETURN, 0);
  input_method_auralinux_->DispatchKeyEvent(&key_up);

  input_method_auralinux_->OnCommit(u"a");
  comp.text = u"";
  input_method_auralinux_->OnPreeditChanged(comp);
  input_method_auralinux_->OnPreeditEnd();


  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionend");
  test_result_->ExpectAction("textinput:a");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, JapaneseCommit) {
  context_->SetSyncMode(false);
  context_->SetEatKey(true);

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());
  KeyEvent key(ET_KEY_PRESSED, VKEY_A, 0);
  key.set_character(L'a');
  input_method_auralinux_->DispatchKeyEvent(&key);

  // IBus issues a standalone set_composition action.
  input_method_auralinux_->OnPreeditStart();
  CompositionText comp;
  comp.text = u"a";
  input_method_auralinux_->OnPreeditChanged(comp);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionstart");
  test_result_->ExpectAction("compositionupdate:a");
  test_result_->Verify();

  // IBus issues a commit text with composition after muting the space key down.
  // Typing return issues a commit, followed by preedit change (to make
  // composition empty), then preedit end.
  KeyEvent key_up(ET_KEY_PRESSED, VKEY_RETURN, 0);
  input_method_auralinux_->DispatchKeyEvent(&key_up);

  input_method_auralinux_->OnCommit(u"a");
  comp.text = u"";
  input_method_auralinux_->OnPreeditChanged(comp);
  input_method_auralinux_->OnPreeditEnd();

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionend");
  test_result_->ExpectAction("textinput:a");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, EmptyCommit) {
  context_->SetSyncMode(false);
  context_->SetEatKey(true);

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());
  KeyEvent key(ET_KEY_PRESSED, VKEY_A, 0);
  key.set_character(L'a');
  input_method_auralinux_->DispatchKeyEvent(&key);

  input_method_auralinux_->OnPreeditStart();
  CompositionText comp;
  comp.text = u"a";
  input_method_auralinux_->OnPreeditChanged(comp);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionstart");
  test_result_->ExpectAction("compositionupdate:a");
  test_result_->Verify();

  input_method_auralinux_->OnCommit(u"");
  comp.text = u"";
  input_method_auralinux_->OnPreeditChanged(comp);
  input_method_auralinux_->OnPreeditEnd();

  test_result_->ExpectAction("compositionend");
  test_result_->ExpectAction("textinput:");
  test_result_->Verify();
}

// crbug.com/463491
void DeadKeyTest(TextInputType text_input_type,
                 InputMethodAuraLinux* input_method_auralinux,
                 LinuxInputMethodContextForTesting* context,
                 TestResult* test_result) {
  context->SetSyncMode(true);
  context->SetEatKey(true);

  auto client = std::make_unique<TextInputClientForTesting>(text_input_type);
  input_method_auralinux->SetFocusedTextInputClient(client.get());
  input_method_auralinux->OnTextInputTypeChanged(client.get());

  constexpr int32_t kCombiningGraveAccent = 0x0300;
  {
    KeyEvent dead_key(
        ET_KEY_PRESSED, VKEY_OEM_4, ui::DomCode::BRACKET_LEFT,
        /* flags= */ 0,
        DomKey::DeadKeyFromCombiningCharacter(kCombiningGraveAccent),
        base::TimeTicks());
    input_method_auralinux->DispatchKeyEvent(&dead_key);
  }

  // Do not filter release key event.
  context->SetEatKey(false);
  {
    KeyEvent dead_key(
        ET_KEY_RELEASED, VKEY_OEM_4, ui::DomCode::BRACKET_LEFT,
        /* flags= */ 0,
        DomKey::DeadKeyFromCombiningCharacter(kCombiningGraveAccent),
        base::TimeTicks());
    input_method_auralinux->DispatchKeyEvent(&dead_key);
  }

  // The single quote key is muted.
  test_result->ExpectAction("keydown:219");
  test_result->ExpectAction("keyup:219");
  test_result->Verify();

  // Reset to filter press key again.
  context->SetEatKey(true);

  context->AddCommitAction("X");
  KeyEvent key(ET_KEY_PRESSED, VKEY_A, 0);
  key.set_character(L'a');
  input_method_auralinux->DispatchKeyEvent(&key);

  // The following A key generates the accent key: á.
  test_result->ExpectAction("keydown:65");
  test_result->ExpectAction("keypress:88");
  test_result->Verify();
}

TEST_F(InputMethodAuraLinuxTest, DeadKeyTest) {
  DeadKeyTest(TEXT_INPUT_TYPE_TEXT, input_method_auralinux_, context_,
              test_result_);
}

TEST_F(InputMethodAuraLinuxTest, DeadKeyTestTypeNone) {
  DeadKeyTest(TEXT_INPUT_TYPE_NONE, input_method_auralinux_, context_,
              test_result_);
}

// Wayland may send both a peek key event and a key event for key events not
// consumed by IME. In that case, the peek key should not be dispatched.
TEST_F(InputMethodAuraLinuxTest, MockWaylandEventsTest) {
  KeyEvent peek_key(ET_KEY_PRESSED, VKEY_TAB, 0);
  input_method_auralinux_->DispatchKeyEvent(&peek_key);
  // No expected action for peek key events.
  test_result_->Verify();

  KeyEvent key(ET_KEY_PRESSED, VKEY_TAB, 0);
  ui::Event::Properties properties;
  properties[ui::kPropertyKeyboardImeFlag] =
      std::vector<uint8_t>({ui::kPropertyKeyboardImeIgnoredFlag});
  key.SetProperties(properties);
  input_method_auralinux_->DispatchKeyEvent(&key);
  test_result_->ExpectAction("keydown:9");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, MultiCommitsTest) {
  context_->SetSyncMode(true);
  context_->SetEatKey(true);
  context_->AddCommitAction("a");
  context_->AddCommitAction("b");
  context_->AddCommitAction("c");

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  KeyEvent key(ET_KEY_PRESSED, VKEY_A, 0);
  key.set_character(L'a');
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("textinput:abc");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, MixedCompositionAndCommitTest) {
  context_->SetSyncMode(true);
  context_->SetEatKey(true);
  context_->AddCommitAction("a");
  context_->AddCompositionStartAction();
  context_->AddCompositionUpdateAction("b");
  context_->AddCommitAction("c");
  context_->AddCompositionUpdateAction("d");

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  KeyEvent key_new(ET_KEY_PRESSED, VKEY_A, 0);
  key_new.set_character(L'a');
  KeyEvent key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("textinput:ac");
  test_result_->ExpectAction("compositionstart");
  test_result_->ExpectAction("compositionupdate:d");
  test_result_->Verify();

  context_->AddCommitAction("e");
  key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionend");
  test_result_->ExpectAction("textinput:e");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, CompositionEndWithoutCommitTest) {
  context_->SetSyncMode(true);
  context_->SetEatKey(true);
  context_->AddCompositionStartAction();
  context_->AddCompositionUpdateAction("a");

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  KeyEvent key_new(ET_KEY_PRESSED, VKEY_A, 0);
  key_new.set_character(L'a');
  KeyEvent key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionstart");
  test_result_->ExpectAction("compositionupdate:a");
  test_result_->Verify();

  context_->AddCompositionEndAction();
  key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionend");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, CompositionEndWithEmptyCommitTest) {
  context_->SetSyncMode(true);
  context_->SetEatKey(true);
  context_->AddCompositionStartAction();
  context_->AddCompositionUpdateAction("a");

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  KeyEvent key_new(ET_KEY_PRESSED, VKEY_A, 0);
  key_new.set_character(L'a');
  KeyEvent key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionstart");
  test_result_->ExpectAction("compositionupdate:a");
  test_result_->Verify();

  context_->AddCompositionEndAction();
  context_->AddCommitAction("");
  key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionend");
  test_result_->ExpectAction("textinput:");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, CompositionEndWithCommitTest) {
  context_->SetSyncMode(true);
  context_->SetEatKey(true);
  context_->AddCompositionStartAction();
  context_->AddCompositionUpdateAction("a");

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  KeyEvent key_new(ET_KEY_PRESSED, VKEY_A, 0);
  key_new.set_character(L'a');
  KeyEvent key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionstart");
  test_result_->ExpectAction("compositionupdate:a");
  test_result_->Verify();

  context_->AddCompositionEndAction();
  context_->AddCommitAction("b");
  key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  // Verifies single char commit under composition mode will call InsertText
  // intead of InsertChar.
  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionend");
  test_result_->ExpectAction("textinput:b");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, CompositionUpdateWithCommitTest) {
  context_->SetSyncMode(true);
  context_->SetEatKey(true);
  context_->AddCompositionStartAction();
  context_->AddCompositionUpdateAction("a");
  context_->AddCommitAction("b");

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  KeyEvent key_new(ET_KEY_PRESSED, VKEY_A, 0);
  key_new.set_character(L'a');
  KeyEvent key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("textinput:b");
  test_result_->ExpectAction("compositionstart");
  test_result_->ExpectAction("compositionupdate:a");
  test_result_->Verify();

  // crbug.com/513124.
  context_->SetSyncMode(true);
  context_->SetEatKey(true);
  context_->AddCommitAction("c");
  context_->AddCompositionUpdateAction("");
  key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionend");
  test_result_->ExpectAction("textinput:c");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, MixedAsyncAndSyncTest) {
  context_->SetSyncMode(false);
  context_->SetEatKey(true);

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  KeyEvent key_new(ET_KEY_PRESSED, VKEY_A, 0);
  key_new.set_character(L'a');
  KeyEvent key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);
  CompositionText comp;
  comp.text = u"a";
  input_method_auralinux_->OnPreeditChanged(comp);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionstart");
  test_result_->ExpectAction("compositionupdate:a");
  test_result_->Verify();

  context_->SetSyncMode(true);
  context_->AddCompositionEndAction();
  context_->AddCommitAction("b");

  key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionend");
  test_result_->ExpectAction("textinput:b");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, MixedSyncAndAsyncTest) {
  context_->SetSyncMode(true);
  context_->SetEatKey(true);
  context_->AddCompositionStartAction();
  context_->AddCompositionUpdateAction("a");

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  KeyEvent key_new(ET_KEY_PRESSED, VKEY_A, 0);
  key_new.set_character(L'a');
  KeyEvent key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionstart");
  test_result_->ExpectAction("compositionupdate:a");
  test_result_->Verify();

  context_->SetSyncMode(false);

  key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);
  input_method_auralinux_->OnCommit(u"b");

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionend");
  test_result_->ExpectAction("textinput:b");
  test_result_->Verify();

  context_->SetSyncMode(true);
  context_->AddCommitAction("c");
  key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:65");
  test_result_->ExpectAction("keypress:99");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, ReleaseKeyTest) {
  context_->SetSyncMode(true);
  context_->SetEatKey(true);
  context_->AddCompositionUpdateAction("a");

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  KeyEvent key_new(ET_KEY_PRESSED, VKEY_A, 0);
  key_new.set_character(L'A');
  KeyEvent key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionstart");
  test_result_->ExpectAction("compositionupdate:a");
  test_result_->Verify();

  context_->SetEatKey(false);
  context_->AddCommitAction("b");
  key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("compositionend");
  test_result_->ExpectAction("textinput:b");
  test_result_->ExpectAction("keydown:65");
  test_result_->ExpectAction("keypress:65");
  test_result_->Verify();

  context_->AddCommitAction("c");
  key = key_new;
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("textinput:c");
  test_result_->ExpectAction("keydown:65");
  test_result_->ExpectAction("keypress:65");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, ReleaseKeyTest_PeekKey) {
  context_->SetSyncMode(true);
  context_->SetEatKey(true);

  KeyEvent key(ET_KEY_RELEASED, VKEY_A, 0);
  key.set_character(L'A');
  input_method_auralinux_->DispatchKeyEvent(&key);

  test_result_->ExpectAction("keyup:65");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, SurroundingText_NoSelectionTest) {
  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  client->surrounding_text = u"abcdef";
  client->text_range = gfx::Range(0, 6);
  client->selection_range = gfx::Range(3, 3);

  input_method_auralinux_->OnCaretBoundsChanged(client.get());

  test_result_->ExpectAction("surroundingtext:abcdef");
  test_result_->ExpectAction("selectionrangestart:3");
  test_result_->ExpectAction("selectionrangeend:3");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, SurroundingText_SelectionTest) {
  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  client->surrounding_text = u"abcdef";
  client->text_range = gfx::Range(0, 6);
  client->selection_range = gfx::Range(2, 5);

  input_method_auralinux_->OnCaretBoundsChanged(client.get());

  test_result_->ExpectAction("surroundingtext:abcdef");
  test_result_->ExpectAction("selectionrangestart:2");
  test_result_->ExpectAction("selectionrangeend:5");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, SurroundingText_PartialText) {
  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  client->surrounding_text = u"abcdefghij";
  client->text_range = gfx::Range(5, 10);
  client->selection_range = gfx::Range(7, 9);

  input_method_auralinux_->OnCaretBoundsChanged(client.get());

  test_result_->ExpectAction("surroundingtext:fghij");
  test_result_->ExpectAction("selectionrangestart:7");
  test_result_->ExpectAction("selectionrangeend:9");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, SetPreeditRegionSingleCharTest) {
  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  client->surrounding_text = u"a";
  client->text_range = gfx::Range(0, 1);
  client->selection_range = gfx::Range(1, 1);

  input_method_auralinux_->OnCaretBoundsChanged(client.get());
  input_method_auralinux_->OnSetPreeditRegion(client->text_range,
                                              std::vector<ImeTextSpan>());

  test_result_->ExpectAction("surroundingtext:a");
  test_result_->ExpectAction("selectionrangestart:1");
  test_result_->ExpectAction("selectionrangeend:1");

  input_method_auralinux_->OnCommit(u"a");

  // Verifies single char commit under composition mode will call InsertText
  // instead of InsertChar.
  test_result_->ExpectAction("textinput:a");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, SetPreeditRegionCompositionEndTest) {
  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  input_method_auralinux_->OnCommit(u"a");

  test_result_->ExpectAction("keypress:97");

  client->surrounding_text = u"a";
  client->text_range = gfx::Range(0, 1);
  client->selection_range = gfx::Range(1, 1);

  input_method_auralinux_->OnCaretBoundsChanged(client.get());
  input_method_auralinux_->OnSetPreeditRegion(client->text_range,
                                              std::vector<ImeTextSpan>());

  test_result_->ExpectAction("surroundingtext:a");
  test_result_->ExpectAction("selectionrangestart:1");
  test_result_->ExpectAction("selectionrangeend:1");

  CompositionText comp;
  comp.text = u"";
  input_method_auralinux_->OnPreeditChanged(comp);

  test_result_->ExpectAction("compositionend");
  test_result_->Verify();
}

TEST_F(InputMethodAuraLinuxTest, OnSetVirtualKeyboardOccludedBounds) {
  auto client =
      std::make_unique<TextInputClientForTesting>(TEXT_INPUT_TYPE_TEXT);
  input_method_auralinux_->SetFocusedTextInputClient(client.get());

  constexpr gfx::Rect kBounds(10, 20, 300, 400);
  input_method_auralinux_->OnSetVirtualKeyboardOccludedBounds(kBounds);

  EXPECT_EQ(client->caret_not_in_rect, kBounds);
}

TEST_F(InputMethodAuraLinuxTest, GetVirtualKeyboardController) {
  EXPECT_EQ(input_method_auralinux_->GetVirtualKeyboardController(),
            context_->GetVirtualKeyboardController());
}

TEST_F(InputMethodAuraLinuxTest, SetContentTypeWithUpdateFocus) {
  auto client1 =
      std::make_unique<TextInputClientForTesting>(TEXT_INPUT_TYPE_TEXT);
  auto client2 =
      std::make_unique<TextInputClientForTesting>(TEXT_INPUT_TYPE_URL);

  EXPECT_EQ(context_->old_client(), nullptr);
  EXPECT_EQ(context_->new_client(), nullptr);

  input_method_auralinux_->SetFocusedTextInputClient(client1.get());

  EXPECT_EQ(context_->input_type(), TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(context_->old_client(), nullptr);
  EXPECT_EQ(context_->new_client(), client1.get());

  input_method_auralinux_->SetFocusedTextInputClient(client2.get());

  EXPECT_EQ(context_->input_type(), TEXT_INPUT_TYPE_URL);
  EXPECT_EQ(context_->old_client(), client1.get());
  EXPECT_EQ(context_->new_client(), client2.get());

  input_method_auralinux_->SetFocusedTextInputClient(client1.get());

  EXPECT_EQ(context_->input_type(), TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(context_->old_client(), client2.get());
  EXPECT_EQ(context_->new_client(), client1.get());

  input_method_auralinux_->SetFocusedTextInputClient(nullptr);

  EXPECT_EQ(context_->input_type(), TEXT_INPUT_TYPE_NONE);
  EXPECT_EQ(context_->old_client(), client1.get());
  EXPECT_EQ(context_->new_client(), nullptr);
}

}  // namespace
}  // namespace ui
