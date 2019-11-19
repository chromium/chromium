// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/linux/input_method_auralinux.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/linux/fake_input_method_context.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/events/event.h"

namespace ui {
namespace {

const base::char16 kActionCommit = L'C';
const base::char16 kActionCompositionStart = L'S';
const base::char16 kActionCompositionUpdate = L'U';
const base::char16 kActionCompositionEnd = L'E';

class TestResult {
 public:
  static TestResult* GetInstance() {
    return base::Singleton<TestResult>::get();
  }

  void RecordAction(const base::string16& action) {
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
  std::vector<base::string16> recorded_actions_;
  std::vector<base::string16> expected_actions_;
};

class LinuxInputMethodContextForTesting : public LinuxInputMethodContext {
 public:
  explicit LinuxInputMethodContextForTesting(
      LinuxInputMethodContextDelegate* delegate)
      : delegate_(delegate),
        is_sync_mode_(false),
        eat_key_(false),
        focused_(false) {}

  void SetSyncMode(bool is_sync_mode) { is_sync_mode_ = is_sync_mode; }
  void SetEatKey(bool eat_key) { eat_key_ = eat_key; }

  void AddCommitAction(const std::string& text) {
    actions_.push_back(base::ASCIIToUTF16("C:" + text));
  }

  void AddCompositionUpdateAction(const std::string& text) {
    actions_.push_back(base::ASCIIToUTF16("U:" + text));
  }

  void AddCompositionStartAction() {
    actions_.push_back(base::ASCIIToUTF16("S"));
  }

  void AddCompositionEndAction() {
    actions_.push_back(base::ASCIIToUTF16("E"));
  }

 protected:
  bool DispatchKeyEvent(const ui::KeyEvent& key_event) override {
    if (!is_sync_mode_) {
      actions_.clear();
      return eat_key_;
    }

    for (const auto& action : actions_) {
      std::vector<base::string16> parts = base::SplitString(
          action, base::string16(1, ':'), base::TRIM_WHITESPACE,
          base::SPLIT_WANT_ALL);
      base::char16 id = parts[0][0];
      base::string16 param;
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

  void Reset() override {}

  void Focus() override { focused_ = true; }

  void Blur() override { focused_ = false; }

  void SetCursorLocation(const gfx::Rect& rect) override {
    cursor_position_ = rect;
  }

  void SetSurroundingText(const base::string16& text,
                          const gfx::Range& selection_range) override {
    TestResult::GetInstance()->RecordAction(
        base::ASCIIToUTF16("surroundingtext:") + text);

    std::stringstream rs;
    rs << "selectionrangestart:" << selection_range.start();
    std::stringstream re;
    re << "selectionrangeend:" << selection_range.end();
    TestResult::GetInstance()->RecordAction(base::ASCIIToUTF16(rs.str()));
    TestResult::GetInstance()->RecordAction(base::ASCIIToUTF16(re.str()));
  }

 private:
  LinuxInputMethodContextDelegate* delegate_;
  std::vector<base::string16> actions_;
  bool is_sync_mode_;
  bool eat_key_;
  bool focused_;
  gfx::Rect cursor_position_;

  DISALLOW_COPY_AND_ASSIGN(LinuxInputMethodContextForTesting);
};

class LinuxInputMethodContextFactoryForTesting
    : public LinuxInputMethodContextFactory {
 public:
  LinuxInputMethodContextFactoryForTesting() {}

  std::unique_ptr<LinuxInputMethodContext> CreateInputMethodContext(
      LinuxInputMethodContextDelegate* delegate,
      bool is_simple) const override {
    return std::unique_ptr<ui::LinuxInputMethodContext>(
        new LinuxInputMethodContextForTesting(delegate));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LinuxInputMethodContextFactoryForTesting);
};

class InputMethodDelegateForTesting : public internal::InputMethodDelegate {
 public:
  InputMethodDelegateForTesting() {}
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

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodDelegateForTesting);
};

class TextInputClientForTesting : public DummyTextInputClient {
 public:
  explicit TextInputClientForTesting(TextInputType text_input_type)
      : DummyTextInputClient(text_input_type) {}

  base::string16 composition_text;
  gfx::Range text_range;
  gfx::Range selection_range;
  base::string16 surrounding_text;

 protected:
  void SetCompositionText(const CompositionText& composition) override {
    composition_text = composition.text;
    TestResult::GetInstance()->RecordAction(
        base::ASCIIToUTF16("compositionstart"));
    TestResult::GetInstance()->RecordAction(
        base::ASCIIToUTF16("compositionupdate:") + composition.text);
  }

  bool HasCompositionText() const override { return !composition_text.empty(); }

  void ConfirmCompositionText(bool keep_selection) override {
    // TODO(b/134473433) Modify this function so that when keep_selection is
    // true, the selection is not changed when text committed
    if (keep_selection) {
      NOTIMPLEMENTED_LOG_ONCE();
    }
    TestResult::GetInstance()->RecordAction(
        base::ASCIIToUTF16("compositionend"));
    TestResult::GetInstance()->RecordAction(base::ASCIIToUTF16("textinput:") +
                                            composition_text);
    composition_text.clear();
  }

  void ClearCompositionText() override {
    TestResult::GetInstance()->RecordAction(
        base::ASCIIToUTF16("compositionend"));
    composition_text.clear();
  }

  void InsertText(const base::string16& text) override {
    if (HasCompositionText()) {
      TestResult::GetInstance()->RecordAction(
          base::ASCIIToUTF16("compositionend"));
    }
    TestResult::GetInstance()->RecordAction(base::ASCIIToUTF16("textinput:") +
                                            text);
    composition_text.clear();
  }

  void InsertChar(const ui::KeyEvent& event) override {
    std::stringstream ss;
    ss << event.GetCharacter();
    TestResult::GetInstance()->RecordAction(base::ASCIIToUTF16("keypress:") +
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
                        base::string16* text) const override {
    if (surrounding_text.empty())
      return false;
    *text = surrounding_text.substr(range.GetMin(), range.length());
    return true;
  }
};

class InputMethodAuraLinuxTest : public testing::Test {
 protected:
  InputMethodAuraLinuxTest()
      : factory_(NULL),
        input_method_auralinux_(NULL),
        delegate_(NULL),
        context_(NULL),
        context_simple_(NULL) {
    factory_ = new LinuxInputMethodContextFactoryForTesting();
    LinuxInputMethodContextFactory::SetInstance(factory_);
    test_result_ = TestResult::GetInstance();
  }
  ~InputMethodAuraLinuxTest() override {
    delete factory_;
    factory_ = NULL;
    test_result_ = NULL;
  }

  void SetUp() override {
    delegate_ = new InputMethodDelegateForTesting();
    input_method_auralinux_ = new InputMethodAuraLinux(delegate_);
    input_method_auralinux_->OnFocus();
    context_ = static_cast<LinuxInputMethodContextForTesting*>(
        input_method_auralinux_->GetContextForTesting(false));
    context_simple_ = static_cast<LinuxInputMethodContextForTesting*>(
        input_method_auralinux_->GetContextForTesting(true));
  }

  void TearDown() override {
    context_->SetSyncMode(false);
    context_->SetEatKey(false);

    context_simple_->SetSyncMode(false);
    context_simple_->SetEatKey(false);

    context_ = NULL;
    context_simple_ = NULL;

    delete input_method_auralinux_;
    input_method_auralinux_ = NULL;
    delete delegate_;
    delegate_ = NULL;
  }

  LinuxInputMethodContextFactoryForTesting* factory_;
  InputMethodAuraLinux* input_method_auralinux_;
  InputMethodDelegateForTesting* delegate_;
  LinuxInputMethodContextForTesting* context_;
  LinuxInputMethodContextForTesting* context_simple_;
  TestResult* test_result_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodAuraLinuxTest);
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
  context_simple_->SetSyncMode(true);
  context_simple_->SetEatKey(false);

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
  input_method_auralinux_->OnCommit(base::ASCIIToUTF16("a"));

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("textinput:a");
  test_result_->Verify();

  input_method_auralinux_->DetachTextInputClient(client.get());
  client =
      std::make_unique<TextInputClientForTesting>(TEXT_INPUT_TYPE_PASSWORD);
  context_simple_->SetSyncMode(false);
  context_simple_->SetEatKey(false);

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
  comp.text = base::ASCIIToUTF16("a");
  input_method_auralinux_->OnPreeditChanged(comp);

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionstart");
  test_result_->ExpectAction("compositionupdate:a");
  test_result_->Verify();

  // IBus issues a commit text with composition after muting the space key down.
  KeyEvent key_up(ET_KEY_RELEASED, VKEY_SPACE, 0);
  input_method_auralinux_->DispatchKeyEvent(&key_up);

  input_method_auralinux_->OnPreeditEnd();
  input_method_auralinux_->OnCommit(base::ASCIIToUTF16("A"));

  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("compositionend");
  test_result_->ExpectAction("keydown:229");
  test_result_->ExpectAction("textinput:A");
  test_result_->Verify();
}

// crbug.com/463491
TEST_F(InputMethodAuraLinuxTest, DeadKeyTest) {
  context_simple_->SetSyncMode(true);
  context_simple_->SetEatKey(true);

  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_NONE));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  KeyEvent dead_key(ET_KEY_PRESSED, VKEY_OEM_7, 0);
  dead_key.set_character(L'\'');
  input_method_auralinux_->DispatchKeyEvent(&dead_key);

  // The single quote key is muted.
  test_result_->ExpectAction("keydown:222");
  test_result_->Verify();

  context_simple_->AddCommitAction("X");
  KeyEvent key(ET_KEY_PRESSED, VKEY_A, 0);
  key.set_character(L'a');
  input_method_auralinux_->DispatchKeyEvent(&key);

  // The following A key generates the accent key: á.
  test_result_->ExpectAction("keydown:65");
  test_result_->ExpectAction("keypress:88");
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
  comp.text = base::ASCIIToUTF16("a");
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
  input_method_auralinux_->OnCommit(base::ASCIIToUTF16("b"));

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

TEST_F(InputMethodAuraLinuxTest, SurroundingText_NoSelectionTest) {
  std::unique_ptr<TextInputClientForTesting> client(
      new TextInputClientForTesting(TEXT_INPUT_TYPE_TEXT));
  input_method_auralinux_->SetFocusedTextInputClient(client.get());
  input_method_auralinux_->OnTextInputTypeChanged(client.get());

  client->surrounding_text = base::ASCIIToUTF16("abcdef");
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

  client->surrounding_text = base::ASCIIToUTF16("abcdef");
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

  client->surrounding_text = base::ASCIIToUTF16("abcdefghij");
  client->text_range = gfx::Range(5, 10);
  client->selection_range = gfx::Range(7, 9);

  input_method_auralinux_->OnCaretBoundsChanged(client.get());

  test_result_->ExpectAction("surroundingtext:fghij");
  test_result_->ExpectAction("selectionrangestart:7");
  test_result_->ExpectAction("selectionrangeend:9");
  test_result_->Verify();
}

}  // namespace
}  // namespace ui
