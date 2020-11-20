// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/textfield_model.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"

#define EXPECT_STR_EQ(ascii, utf16) EXPECT_EQ(base::ASCIIToUTF16(ascii), utf16)

namespace {

struct WordAndCursor {
  WordAndCursor(const wchar_t* w, size_t c) : word(w), cursor(c) {}

  const wchar_t* word;
  size_t cursor;
};

}  // namespace

namespace views {

class TextfieldModelTest : public ViewsTestBase,
                           public TextfieldModel::Delegate {
 public:
  TextfieldModelTest() = default;

  // ::testing::Test:
  void TearDown() override {
    // Clear kill buffer used for "Yank" text editing command so that no state
    // persists between tests.
    TextfieldModel::ClearKillBuffer();
    ViewsTestBase::TearDown();
  }

  void OnCompositionTextConfirmedOrCleared() override {
    composition_text_confirmed_or_cleared_ = true;
  }

 protected:
  void ResetModel(TextfieldModel* model) const {
    model->SetText(base::string16(), 0);
    model->ClearEditHistory();
  }

  const std::vector<base::string16> GetAllSelectionTexts(
      TextfieldModel* model) const {
    std::vector<base::string16> selected_texts;
    for (auto range : model->render_text()->GetAllSelections())
      selected_texts.push_back(model->GetTextFromRange(range));
    return selected_texts;
  }

  void VerifyAllSelectionTexts(
      TextfieldModel* model,
      std::vector<std::string> expected_selected_texts) const {
    std::vector<base::string16> selected_texts = GetAllSelectionTexts(model);
    EXPECT_EQ(expected_selected_texts.size(), selected_texts.size());
    for (size_t i = 0; i < selected_texts.size(); ++i)
      EXPECT_STR_EQ(expected_selected_texts[i], selected_texts[i]);
  }

  bool composition_text_confirmed_or_cleared_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextfieldModelTest);
};

TEST_F(TextfieldModelTest, EditString) {
  TextfieldModel model(nullptr);
  // Append two strings.
  model.Append(base::ASCIIToUTF16("HILL"));
  EXPECT_STR_EQ("HILL", model.text());
  model.Append(base::ASCIIToUTF16("WORLD"));
  EXPECT_STR_EQ("HILLWORLD", model.text());

  // Insert "E" and replace "I" with "L" to make "HELLO".
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_NONE);
  model.InsertChar('E');
  EXPECT_STR_EQ("HEILLWORLD", model.text());
  model.ReplaceChar('L');
  EXPECT_STR_EQ("HELLLWORLD", model.text());
  model.ReplaceChar('L');
  model.ReplaceChar('O');
  EXPECT_STR_EQ("HELLOWORLD", model.text());

  // Delete 6th char "W", then delete 5th char "O".
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("HELLOORLD", model.text());
  EXPECT_TRUE(model.Backspace());
  EXPECT_EQ(4U, model.GetCursorPosition());
  EXPECT_STR_EQ("HELLORLD", model.text());

  // Move the cursor to start; backspace should fail.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  EXPECT_FALSE(model.Backspace());
  EXPECT_STR_EQ("HELLORLD", model.text());
  // Move the cursor to the end; delete should fail, but backspace should work.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  EXPECT_FALSE(model.Delete());
  EXPECT_STR_EQ("HELLORLD", model.text());
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("HELLORL", model.text());

  model.MoveCursorTo(5);
  model.ReplaceText(base::ASCIIToUTF16(" WOR"));
  EXPECT_STR_EQ("HELLO WORL", model.text());
}

TEST_F(TextfieldModelTest, EditString_SimpleRTL) {
  TextfieldModel model(nullptr);
  // Append two strings.
  model.Append(base::WideToUTF16(L"\x05d0\x05d1\x05d2"));
  EXPECT_EQ(base::WideToUTF16(L"\x05d0\x05d1\x05d2"), model.text());
  model.Append(base::WideToUTF16(L"\x05e0\x05e1\x05e2"));
  EXPECT_EQ(base::WideToUTF16(L"\x05d0\x05d1\x05d2\x05e0\x05e1\x05e2"),
            model.text());

  // Insert "\x05f0".
  model.MoveCursorTo(1);
  model.InsertChar(0x05f0);
  EXPECT_EQ(base::WideToUTF16(L"\x05d0\x05f0\x05d1\x05d2\x05e0\x05e1\x05e2"),
            model.text());

  // Replace "\x05d1" with "\x05f1".
  model.ReplaceChar(0x05f1);
  EXPECT_EQ(base::WideToUTF16(L"\x05d0\x05f0\x5f1\x05d2\x05e0\x05e1\x05e2"),
            model.text());

  // Test Delete and backspace.
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Delete());
  EXPECT_EQ(base::WideToUTF16(L"\x05d0\x05f0\x5f1\x05e0\x05e1\x05e2"),
            model.text());
  EXPECT_TRUE(model.Backspace());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_EQ(base::WideToUTF16(L"\x05d0\x05f0\x05e0\x05e1\x05e2"), model.text());
}

TEST_F(TextfieldModelTest, EditString_ComplexScript) {
  TextfieldModel model(nullptr);

  // Append two Hindi strings.
  model.Append(base::WideToUTF16(L"\x0915\x093f\x0915\x094d\x0915"));
  EXPECT_EQ(base::WideToUTF16(L"\x0915\x093f\x0915\x094d\x0915"), model.text());
  model.Append(base::WideToUTF16(L"\x0915\x094d\x092e\x094d"));
  EXPECT_EQ(base::WideToUTF16(
                L"\x0915\x093f\x0915\x094d\x0915\x0915\x094d\x092e\x094d"),
            model.text());

  // Ensure the cursor cannot be placed in the middle of a grapheme.
  model.MoveCursorTo(1);
  EXPECT_EQ(0U, model.GetCursorPosition());

  model.MoveCursorTo(2);
  EXPECT_EQ(2U, model.GetCursorPosition());
  model.InsertChar('a');
  EXPECT_EQ(
      base::WideToUTF16(
          L"\x0915\x093f\x0061\x0915\x094d\x0915\x0915\x094d\x092e\x094d"),
      model.text());

  // ReplaceChar will replace the whole grapheme.
  model.ReplaceChar('b');
// TODO(xji): temporarily disable in platform Win since the complex script
// characters turned into empty square due to font regression. So, not able
// to test 2 characters belong to the same grapheme.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  EXPECT_EQ(
      base::WideToUTF16(L"\x0915\x093f\x0061\x0062\x0915\x094d\x092e\x094d"),
      model.text());
#endif
  EXPECT_EQ(4U, model.GetCursorPosition());

  // Delete should delete the whole grapheme.
  model.MoveCursorTo(0);
  // TODO(xji): temporarily disable in platform Win since the complex script
  // characters turned into empty square due to font regression. So, not able
  // to test 2 characters belong to the same grapheme.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  EXPECT_TRUE(model.Delete());
  EXPECT_EQ(base::WideToUTF16(L"\x0061\x0062\x0915\x094d\x092e\x094d"),
            model.text());
  model.MoveCursorTo(model.text().length());
  EXPECT_EQ(model.text().length(), model.GetCursorPosition());
  EXPECT_TRUE(model.Backspace());
  EXPECT_EQ(base::WideToUTF16(L"\x0061\x0062\x0915\x094d\x092e"), model.text());
#endif

  // Test cursor position and deletion for Hindi Virama.
  model.SetText(base::WideToUTF16(L"\x0D38\x0D4D\x0D15\x0D16\x0D2E"), 0);
  model.MoveCursorTo(0);
  EXPECT_EQ(0U, model.GetCursorPosition());

  model.MoveCursorTo(1);
  EXPECT_EQ(0U, model.GetCursorPosition());
  model.MoveCursorTo(3);
  EXPECT_EQ(3U, model.GetCursorPosition());

  // TODO(asvitkine): Temporarily disable the following check on Windows. It
  // seems Windows treats "\x0D38\x0D4D\x0D15" as a single grapheme.
#if !defined(OS_WIN)
  model.MoveCursorTo(2);
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Backspace());
  EXPECT_EQ(base::WideToUTF16(L"\x0D38\x0D4D\x0D16\x0D2E"), model.text());
#endif

  model.SetText(
      base::WideToUTF16(L"\x05d5\x05b7\x05D9\x05B0\x05D4\x05B4\x05D9"), 0);
  model.MoveCursorTo(0);
  EXPECT_TRUE(model.Delete());
  EXPECT_TRUE(model.Delete());
  EXPECT_TRUE(model.Delete());
  EXPECT_TRUE(model.Delete());
  EXPECT_EQ(base::WideToUTF16(L""), model.text());

  // The first 2 characters are not strong directionality characters.
  model.SetText(
      base::WideToUTF16(L"\x002C\x0020\x05D1\x05BC\x05B7\x05E9\x05BC"), 0);
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  EXPECT_TRUE(model.Backspace());
  EXPECT_EQ(base::WideToUTF16(L"\x002C\x0020\x05D1\x05BC\x05B7\x05E9"),
            model.text());

  // Halfwidth katakana ﾀﾞ:
  // "HALFWIDTH KATAKANA LETTER TA" + "HALFWIDTH KATAKANA VOICED SOUND MARK"
  // ("ABC" prefix as sanity check that the entire string isn't deleted).
  model.SetText(base::WideToUTF16(L"ABC\xFF80\xFF9E"), 0);
  model.MoveCursorTo(model.text().length());
  model.Backspace();
#if defined(OS_APPLE)
  // On Mac, the entire cluster should be deleted to match
  // NSTextField behavior.
  EXPECT_EQ(base::WideToUTF16(L"ABC"), model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
#else
  EXPECT_EQ(base::WideToUTF16(L"ABC\xFF80"), model.text());
  EXPECT_EQ(4U, model.GetCursorPosition());
#endif

  // Emoji with Fitzpatrick modifier:
  // 'BOY' + 'EMOJI MODIFIER FITZPATRICK TYPE-5'
  model.SetText(base::WideToUTF16(L"\U0001F466\U0001F3FE"), 0);
  model.MoveCursorTo(model.text().length());
  model.Backspace();
#if defined(OS_APPLE)
  // On Mac, the entire emoji should be deleted to match NSTextField
  // behavior.
  EXPECT_EQ(base::WideToUTF16(L""), model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());
#else
  // https://crbug.com/829040
  EXPECT_EQ(base::WideToUTF16(L"\U0001F466"), model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
#endif
}

TEST_F(TextfieldModelTest, EmptyString) {
  TextfieldModel model(nullptr);
  EXPECT_EQ(base::string16(), model.text());
  EXPECT_EQ(base::string16(), model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);
  EXPECT_EQ(0U, model.GetCursorPosition());
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  EXPECT_EQ(0U, model.GetCursorPosition());

  EXPECT_EQ(base::string16(), model.GetSelectedText());

  EXPECT_FALSE(model.Delete());
  EXPECT_FALSE(model.Backspace());
}

TEST_F(TextfieldModelTest, Selection) {
  TextfieldModel model(nullptr);
  model.Append(base::ASCIIToUTF16("HELLO"));
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_NONE);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  EXPECT_STR_EQ("E", model.GetSelectedText());
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  EXPECT_STR_EQ("EL", model.GetSelectedText());

  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  EXPECT_STR_EQ("H", model.GetSelectedText());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  EXPECT_STR_EQ("ELLO", model.GetSelectedText());
  model.ClearSelection();
  EXPECT_EQ(base::string16(), model.GetSelectedText());

  // SelectAll(false) selects towards the end.
  model.SelectAll(false);
  EXPECT_STR_EQ("HELLO", model.GetSelectedText());
  EXPECT_EQ(gfx::Range(0, 5), model.render_text()->selection());

  // SelectAll(true) selects towards the beginning.
  model.SelectAll(true);
  EXPECT_STR_EQ("HELLO", model.GetSelectedText());
  EXPECT_EQ(gfx::Range(5, 0), model.render_text()->selection());

  // Select and move cursor.
  model.SelectRange(gfx::Range(1U, 3U));
  EXPECT_STR_EQ("EL", model.GetSelectedText());
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  EXPECT_EQ(1U, model.GetCursorPosition());
  model.SelectRange(gfx::Range(1U, 3U));
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_NONE);
  EXPECT_EQ(3U, model.GetCursorPosition());

  // Select multiple ranges and move cursor.
  model.SelectRange(gfx::Range(1U, 3U));
  model.SelectRange(gfx::Range(5U, 4U), false);
  EXPECT_STR_EQ("EL", model.GetSelectedText());
  EXPECT_EQ(3U, model.GetCursorPosition());
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  EXPECT_TRUE(model.GetSelectedText().empty());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.render_text()->secondary_selections().empty());
  model.SelectRange(gfx::Range(1U, 3U));
  model.SelectRange(gfx::Range(4U, 5U), false);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_NONE);
  EXPECT_TRUE(model.GetSelectedText().empty());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.render_text()->secondary_selections().empty());

  // Select all and move cursor.
  model.SelectAll(false);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  EXPECT_EQ(0U, model.GetCursorPosition());
  model.SelectAll(false);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_NONE);
  EXPECT_EQ(5U, model.GetCursorPosition());
}

TEST_F(TextfieldModelTest, Selection_BidiWithNonSpacingMarks) {
  // Selection is a logical operation. And it should work with the arrow
  // keys doing visual movements, while the selection is logical between
  // the (logical) start and end points. Selection is simply defined as
  // the portion of text between the logical positions of the start and end
  // caret positions.
  TextfieldModel model(nullptr);
  // TODO(xji): temporarily disable in platform Win since the complex script
  // characters turned into empty square due to font regression. So, not able
  // to test 2 characters belong to the same grapheme.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  model.Append(
      base::WideToUTF16(L"abc\x05E9\x05BC\x05C1\x05B8\x05E0\x05B8"
                        L"def"));
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_NONE);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_NONE);

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  EXPECT_EQ(gfx::Range(2, 3), model.render_text()->selection());
  EXPECT_EQ(base::WideToUTF16(L"c"), model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  EXPECT_EQ(gfx::Range(2, 7), model.render_text()->selection());
  EXPECT_EQ(base::WideToUTF16(L"c\x05E9\x05BC\x05C1\x05B8"),
            model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  EXPECT_EQ(gfx::Range(2, 3), model.render_text()->selection());
  EXPECT_EQ(base::WideToUTF16(L"c"), model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  EXPECT_EQ(gfx::Range(2, 10), model.render_text()->selection());
  EXPECT_EQ(base::WideToUTF16(L"c\x05E9\x05BC\x05C1\x05B8\x05E0\x05B8"
                              L"d"),
            model.GetSelectedText());

  model.ClearSelection();
  EXPECT_EQ(base::string16(), model.GetSelectedText());
  model.SelectAll(false);
  EXPECT_EQ(base::WideToUTF16(L"abc\x05E9\x05BC\x05C1\x05B8\x05E0\x05B8"
                              L"def"),
            model.GetSelectedText());
#endif

  // In case of "aBc", this test shows how to select "aB" or "Bc", assume 'B' is
  // an RTL character.
  model.SetText(base::WideToUTF16(L"a\x05E9"
                                  L"b"),
                0);
  model.MoveCursorTo(0);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  EXPECT_EQ(base::WideToUTF16(L"a"), model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  EXPECT_EQ(base::WideToUTF16(L"a"), model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  EXPECT_EQ(base::WideToUTF16(L"a\x05E9"
                              L"b"),
            model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_NONE);
  EXPECT_EQ(3U, model.GetCursorPosition());
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);
  EXPECT_EQ(base::WideToUTF16(L"b"), model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);
  EXPECT_EQ(base::WideToUTF16(L"b"), model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);
  EXPECT_EQ(base::WideToUTF16(L"a\x05E9"
                              L"b"),
            model.GetSelectedText());

  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);
  EXPECT_EQ(base::WideToUTF16(L"a\x05E9"), model.GetSelectedText());

  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  EXPECT_EQ(base::WideToUTF16(L"\x05E9"
                              L"b"),
            model.GetSelectedText());

  model.ClearSelection();
  EXPECT_EQ(base::string16(), model.GetSelectedText());
  model.SelectAll(false);
  EXPECT_EQ(base::WideToUTF16(L"a\x05E9"
                              L"b"),
            model.GetSelectedText());
}

TEST_F(TextfieldModelTest, SelectionAndEdit) {
  TextfieldModel model(nullptr);
  model.Append(base::ASCIIToUTF16("HELLO"));
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_NONE);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);  // "EL"
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("HLO", model.text());

  model.Append(base::ASCIIToUTF16("ILL"));
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);  // "LO"
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("HILL", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);  // "I"
  model.InsertChar('E');
  EXPECT_STR_EQ("HELL", model.text());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);  // "H"
  model.ReplaceChar('B');
  EXPECT_STR_EQ("BELL", model.text());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);  // "ELL"
  model.ReplaceChar('E');
  EXPECT_STR_EQ("BEE", model.text());
}

TEST_F(TextfieldModelTest, SelectionAndEdit_WithSecondarySelection) {
  // Backspace
  TextfieldModel model(nullptr);
  model.Append(base::ASCIIToUTF16("asynchronous promises make the moon spin?"));
  model.SelectRange(gfx::Range(0U, 4U));
  model.SelectRange(gfx::Range(17U, 19U), false);
  model.SelectRange(gfx::Range(15U, 7U), false);
  model.SelectRange(gfx::Range(41U, 20U), false);
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("chrome", model.text());
  EXPECT_TRUE(model.GetSelectedText().empty());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_TRUE(model.render_text()->secondary_selections().empty());

  // Delete with an empty primary selection
  model.Append(base::ASCIIToUTF16(" is constructor overloading bad?"));
  model.SelectRange(gfx::Range(1U, 1U));
  model.SelectRange(gfx::Range(22U, 12U), false);
  model.SelectRange(gfx::Range(26U, 23U), false);
  model.SelectRange(gfx::Range(27U, 38U), false);
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("chrome is cool", model.text());
  EXPECT_TRUE(model.GetSelectedText().empty());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.render_text()->secondary_selections().empty());

  // Insert
  model.Append(base::ASCIIToUTF16(" are inherited classes heavy?"));
  model.SelectRange(gfx::Range(27U, 16U));
  model.SelectRange(gfx::Range(41U, 34U), false);
  model.SelectRange(gfx::Range(42U, 43U), false);
  model.InsertChar('n');
  EXPECT_STR_EQ("chrome is cool and classy", model.text());
  EXPECT_TRUE(model.GetSelectedText().empty());
  EXPECT_EQ(17U, model.GetCursorPosition());
  EXPECT_TRUE(model.render_text()->secondary_selections().empty());

  // Replace
  model.Append(
      base::ASCIIToUTF16("help! why can't i instantiate an abstract sun!?"));
  model.SelectRange(gfx::Range(71U, 72U));
  model.SelectRange(gfx::Range(30U, 70U), false);
  model.SelectRange(gfx::Range(29U, 25U), false);
  model.ReplaceChar('!');
  EXPECT_STR_EQ("chrome is cool and classy!!!", model.text());
  EXPECT_TRUE(model.GetSelectedText().empty());
  EXPECT_EQ(28U, model.GetCursorPosition());
  EXPECT_TRUE(model.render_text()->secondary_selections().empty());
}

TEST_F(TextfieldModelTest, Word) {
  TextfieldModel model(nullptr);
  model.Append(
      base::ASCIIToUTF16("The answer to Life, the Universe, and Everything"));
#if defined(OS_WIN)  // Move right by word includes space/punctuation.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  EXPECT_EQ(4U, model.GetCursorPosition());
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  EXPECT_EQ(11U, model.GetCursorPosition());
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  // Should pass the non word chars ', ' and be at the start of "the".
  EXPECT_EQ(20U, model.GetCursorPosition());

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  EXPECT_EQ(24U, model.GetCursorPosition());
  EXPECT_STR_EQ("the ", model.GetSelectedText());

  // Move to the end.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  EXPECT_STR_EQ("the Universe, and Everything", model.GetSelectedText());
  // Should be safe to go next word at the end.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  EXPECT_STR_EQ("the Universe, and Everything", model.GetSelectedText());
  model.InsertChar('2');
  EXPECT_EQ(21U, model.GetCursorPosition());

  // Now backwards.
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_NONE);  // leave 2.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  EXPECT_EQ(14U, model.GetCursorPosition());
  EXPECT_STR_EQ("Life, ", model.GetSelectedText());
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  EXPECT_STR_EQ("to Life, ", model.GetSelectedText());
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);  // Now at start.
  EXPECT_STR_EQ("The answer to Life, ", model.GetSelectedText());
  // Should be safe to go to the previous word at the beginning.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  EXPECT_STR_EQ("The answer to Life, ", model.GetSelectedText());
  model.ReplaceChar('4');
  EXPECT_EQ(base::string16(), model.GetSelectedText());
  EXPECT_STR_EQ("42", model.text());
#else  // Non-Windows: move right by word does NOT include space/punctuation.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  EXPECT_EQ(3U, model.GetCursorPosition());
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  EXPECT_EQ(10U, model.GetCursorPosition());
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  EXPECT_EQ(18U, model.GetCursorPosition());

  // Should passes the non word char ','
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  EXPECT_EQ(23U, model.GetCursorPosition());
  EXPECT_STR_EQ(", the", model.GetSelectedText());

  // Move to the end.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  EXPECT_STR_EQ(", the Universe, and Everything", model.GetSelectedText());
  // Should be safe to go next word at the end.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  EXPECT_STR_EQ(", the Universe, and Everything", model.GetSelectedText());
  model.InsertChar('2');
  EXPECT_EQ(19U, model.GetCursorPosition());

  // Now backwards.
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_NONE);  // leave 2.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  EXPECT_EQ(14U, model.GetCursorPosition());
  EXPECT_STR_EQ("Life", model.GetSelectedText());
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  EXPECT_STR_EQ("to Life", model.GetSelectedText());
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);  // Now at start.
  EXPECT_STR_EQ("The answer to Life", model.GetSelectedText());
  // Should be safe to go to the previous word at the beginning.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  EXPECT_STR_EQ("The answer to Life", model.GetSelectedText());
  model.ReplaceChar('4');
  EXPECT_EQ(base::string16(), model.GetSelectedText());
  EXPECT_STR_EQ("42", model.text());
#endif
}

TEST_F(TextfieldModelTest, SetText) {
  TextfieldModel model(nullptr);
  model.Append(base::ASCIIToUTF16("HELLO"));

  // SetText moves cursor to the indicated position.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  model.SetText(base::ASCIIToUTF16("GOODBYE"), 6);
  EXPECT_STR_EQ("GOODBYE", model.text());
  EXPECT_EQ(6U, model.GetCursorPosition());
  model.SetText(base::ASCIIToUTF16("SUNSET"), 6);
  EXPECT_STR_EQ("SUNSET", model.text());
  EXPECT_EQ(6U, model.GetCursorPosition());
  model.SelectAll(false);
  EXPECT_STR_EQ("SUNSET", model.GetSelectedText());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  EXPECT_EQ(6U, model.GetCursorPosition());

  // Setting text to the current text should not modify the cursor position.
  model.SetText(base::ASCIIToUTF16("SUNSET"), 3);
  EXPECT_STR_EQ("SUNSET", model.text());
  EXPECT_EQ(6U, model.GetCursorPosition());

  // Setting text that's shorter than the indicated cursor moves the cursor to
  // the text end.
  model.SetText(base::ASCIIToUTF16("BYE"), 5);
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_EQ(base::string16(), model.GetSelectedText());

  // SetText with empty string.
  model.SetText(base::string16(), 0);
  EXPECT_EQ(0U, model.GetCursorPosition());
}

TEST_F(TextfieldModelTest, Clipboard) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  const base::string16 initial_clipboard_text =
      base::ASCIIToUTF16("initial text");
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(initial_clipboard_text);

  base::string16 clipboard_text;
  TextfieldModel model(nullptr);
  model.Append(base::ASCIIToUTF16("HELLO WORLD"));

  // Cut with an empty selection should do nothing.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  EXPECT_FALSE(model.Cut());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_EQ(initial_clipboard_text, clipboard_text);
  EXPECT_STR_EQ("HELLO WORLD", model.text());
  EXPECT_EQ(11U, model.GetCursorPosition());

  // Copy with an empty selection should do nothing.
  EXPECT_FALSE(model.Copy());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_EQ(initial_clipboard_text, clipboard_text);
  EXPECT_STR_EQ("HELLO WORLD", model.text());
  EXPECT_EQ(11U, model.GetCursorPosition());

  // Cut on obscured (password) text should do nothing.
  model.render_text()->SetObscured(true);
  model.SelectAll(false);
  EXPECT_FALSE(model.Cut());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_EQ(initial_clipboard_text, clipboard_text);
  EXPECT_STR_EQ("HELLO WORLD", model.text());
  EXPECT_STR_EQ("HELLO WORLD", model.GetSelectedText());

  // Copy on obscured (password) text should do nothing.
  model.SelectAll(false);
  EXPECT_FALSE(model.Copy());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_EQ(initial_clipboard_text, clipboard_text);
  EXPECT_STR_EQ("HELLO WORLD", model.text());
  EXPECT_STR_EQ("HELLO WORLD", model.GetSelectedText());

  // Cut with non-empty selection.
  model.render_text()->SetObscured(false);
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  EXPECT_TRUE(model.Cut());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_STR_EQ("WORLD", clipboard_text);
  EXPECT_STR_EQ("HELLO ", model.text());
  EXPECT_EQ(6U, model.GetCursorPosition());

  // Copy with non-empty selection.
  model.SelectAll(false);
  EXPECT_TRUE(model.Copy());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_STR_EQ("HELLO ", clipboard_text);
  EXPECT_STR_EQ("HELLO ", model.text());
  EXPECT_EQ(6U, model.GetCursorPosition());

  // Test that paste works regardless of the obscured bit. Please note that
  // trailing spaces and tabs in clipboard strings will be stripped.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  EXPECT_TRUE(model.Paste());
  EXPECT_STR_EQ("HELLO HELLO", model.text());
  EXPECT_EQ(11U, model.GetCursorPosition());
  model.render_text()->SetObscured(true);
  EXPECT_TRUE(model.Paste());
  EXPECT_STR_EQ("HELLO HELLOHELLO", model.text());
  EXPECT_EQ(16U, model.GetCursorPosition());

  // Paste should replace the selection.
  model.render_text()->SetObscured(false);
  model.SetText(base::ASCIIToUTF16("It's time to say goodbye."), 0);
  model.SelectRange({17, 24});
  EXPECT_TRUE(model.Paste());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_STR_EQ("HELLO ", clipboard_text);
  EXPECT_STR_EQ("It's time to say HELLO.", model.text());
  EXPECT_EQ(22U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.render_text()->secondary_selections().empty());

  // Paste with an empty clipboard should not replace the selection.
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  model.SelectRange({5, 8});
  EXPECT_FALSE(model.Paste());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_TRUE(clipboard_text.empty());
  EXPECT_STR_EQ("It's time to say HELLO.", model.text());
  EXPECT_EQ(8U, model.GetCursorPosition());
  EXPECT_STR_EQ("tim", model.GetSelectedText());
}

TEST_F(TextfieldModelTest, Clipboard_WithSecondarySelections) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  const base::string16 initial_clipboard_text =
      base::ASCIIToUTF16("initial text");
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(initial_clipboard_text);

  base::string16 clipboard_text;
  TextfieldModel model(nullptr);
  model.Append(base::ASCIIToUTF16("It's time to say HELLO."));

  // Cut with multiple selections should copy only the primary selection but
  // delete all selections.
  model.SelectRange({0, 5});
  model.SelectRange({13, 17}, false);
  EXPECT_TRUE(model.Cut());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_STR_EQ("It's ", clipboard_text);
  EXPECT_STR_EQ("time to HELLO.", model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.render_text()->secondary_selections().empty());

  // Copy with multiple selections should copy only the primary selection and
  // retain multiple selections.
  model.SelectRange({13, 8});
  model.SelectRange({0, 4}, false);
  EXPECT_TRUE(model.Copy());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_STR_EQ("HELLO", clipboard_text);
  EXPECT_STR_EQ("time to HELLO.", model.text());
  EXPECT_EQ(8U, model.GetCursorPosition());
  EXPECT_TRUE(model.HasSelection());
  VerifyAllSelectionTexts(&model, {"HELLO", "time"});

  // Paste with multiple selections should paste at the primary selection and
  // delete all selections.
  model.SelectRange({0, 1});
  model.SelectRange({5, 8}, false);
  model.SelectRange({14, 14}, false);
  EXPECT_TRUE(model.Paste());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_STR_EQ("HELLO", clipboard_text);
  EXPECT_STR_EQ("HELLOime HELLO.", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.render_text()->secondary_selections().empty());

  // Paste with multiple selections and an empty clipboard should not change the
  // text or selections.
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  model.SelectRange({1, 2});
  model.SelectRange({4, 5}, false);
  EXPECT_FALSE(model.Paste());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_TRUE(clipboard_text.empty());
  EXPECT_STR_EQ("HELLOime HELLO.", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  VerifyAllSelectionTexts(&model, {"E", "O"});

  // Cut with an empty primary selection and nonempty secondary selections
  // should neither delete the secondary selection nor replace the clipboard.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(initial_clipboard_text);
  model.SelectRange({2, 2});
  model.SelectRange({4, 5}, false);
  EXPECT_FALSE(model.Cut());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_STR_EQ("initial text", clipboard_text);
  EXPECT_STR_EQ("HELLOime HELLO.", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  VerifyAllSelectionTexts(&model, {"", "O"});

  // Copy with an empty primary selection and nonempty secondary selections
  // should not replace the clipboard.
  EXPECT_FALSE(model.Copy());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_STR_EQ("initial text", clipboard_text);
  EXPECT_STR_EQ("HELLOime HELLO.", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  VerifyAllSelectionTexts(&model, {"", "O"});

  // Paste with an empty primary selection, nonempty secondary selection, and
  // empty clipboard should change neither the text nor the selections.
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  EXPECT_FALSE(model.Paste());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_TRUE(clipboard_text.empty());
  EXPECT_STR_EQ("HELLOime HELLO.", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  VerifyAllSelectionTexts(&model, {"", "O"});

  // Paste with an empty primary selection and nonempty secondary selections
  // should paste at the primary selection and delete the secondary selections.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(initial_clipboard_text);
  EXPECT_TRUE(model.Paste());
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_STR_EQ("initial text", clipboard_text);
  EXPECT_STR_EQ("HEinitial textLLime HELLO.", model.text());
  EXPECT_EQ(14U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
}

static void SelectWordTestVerifier(
    const TextfieldModel& model,
    const base::string16& expected_selected_string,
    size_t expected_cursor_pos) {
  EXPECT_EQ(expected_selected_string, model.GetSelectedText());
  EXPECT_EQ(expected_cursor_pos, model.GetCursorPosition());
}

TEST_F(TextfieldModelTest, SelectWordTest) {
  TextfieldModel model(nullptr);
  model.Append(base::ASCIIToUTF16("  HELLO  !!  WO     RLD "));

  // Test when cursor is at the beginning.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  model.SelectWord();
  SelectWordTestVerifier(model, base::ASCIIToUTF16("  "), 2U);

  // Test when cursor is at the beginning of a word.
  model.MoveCursorTo(2);
  model.SelectWord();
  SelectWordTestVerifier(model, base::ASCIIToUTF16("HELLO"), 7U);

  // Test when cursor is at the end of a word.
  model.MoveCursorTo(15);
  model.SelectWord();
  SelectWordTestVerifier(model, base::ASCIIToUTF16("     "), 20U);

  // Test when cursor is somewhere in a non-alpha-numeric fragment.
  for (size_t cursor_pos = 8; cursor_pos < 13U; cursor_pos++) {
    model.MoveCursorTo(cursor_pos);
    model.SelectWord();
    SelectWordTestVerifier(model, base::ASCIIToUTF16("  !!  "), 13U);
  }

  // Test when cursor is somewhere in a whitespace fragment.
  model.MoveCursorTo(17);
  model.SelectWord();
  SelectWordTestVerifier(model, base::ASCIIToUTF16("     "), 20U);

  // Test when cursor is at the end.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  model.SelectWord();
  SelectWordTestVerifier(model, base::ASCIIToUTF16(" "), 24U);
}

// TODO(xji): temporarily disable in platform Win since the complex script
// characters and Chinese characters are turned into empty square due to font
// regression.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
TEST_F(TextfieldModelTest, SelectWordTest_MixScripts) {
  TextfieldModel model(nullptr);
  std::vector<WordAndCursor> word_and_cursor;
  word_and_cursor.emplace_back(L"a\x05d0", 2);
  word_and_cursor.emplace_back(L"a\x05d0", 2);
  word_and_cursor.emplace_back(L"\x05d1\x05d2", 5);
  word_and_cursor.emplace_back(L"\x05d1\x05d2", 5);
  word_and_cursor.emplace_back(L" ", 3);
  word_and_cursor.emplace_back(L"a\x05d0", 2);
  word_and_cursor.emplace_back(L"\x0915\x094d\x0915", 9);
  word_and_cursor.emplace_back(L" ", 10);
  word_and_cursor.emplace_back(L"\x4E2D\x56FD", 12);
  word_and_cursor.emplace_back(L"\x4E2D\x56FD", 12);
  word_and_cursor.emplace_back(L"\x82B1", 13);
  word_and_cursor.emplace_back(L"\x5929", 14);

  // The text consists of Ascii, Hebrew, Hindi with Virama sign, and Chinese.
  model.SetText(base::WideToUTF16(L"a\x05d0 \x05d1\x05d2 \x0915\x094d\x0915 "
                                  L"\x4E2D\x56FD\x82B1\x5929"),
                0);
  for (size_t i = 0; i < word_and_cursor.size(); ++i) {
    model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
    for (size_t j = 0; j < i; ++j)
      model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                       gfx::SELECTION_NONE);
    model.SelectWord();
    SelectWordTestVerifier(model, base::WideToUTF16(word_and_cursor[i].word),
                           word_and_cursor[i].cursor);
  }
}
#endif

TEST_F(TextfieldModelTest, RangeTest) {
  TextfieldModel model(nullptr);
  model.Append(base::ASCIIToUTF16("HELLO WORLD"));
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  gfx::Range range = model.render_text()->selection();
  EXPECT_TRUE(range.is_empty());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(0U, range.end());

#if defined(OS_WIN)  // Move/select right by word includes space/punctuation.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  range = model.render_text()->selection();
  EXPECT_FALSE(range.is_empty());
  EXPECT_FALSE(range.is_reversed());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(6U, range.end());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);
  range = model.render_text()->selection();
  EXPECT_FALSE(range.is_empty());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(5U, range.end());

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  range = model.render_text()->selection();
  EXPECT_TRUE(range.is_empty());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(0U, range.end());

  // now from the end.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  range = model.render_text()->selection();
  EXPECT_TRUE(range.is_empty());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(11U, range.end());

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  range = model.render_text()->selection();
  EXPECT_FALSE(range.is_empty());
  EXPECT_TRUE(range.is_reversed());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(6U, range.end());
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  range = model.render_text()->selection();
  EXPECT_FALSE(range.is_empty());
  EXPECT_TRUE(range.is_reversed());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(7U, range.end());

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  range = model.render_text()->selection();
  EXPECT_TRUE(range.is_empty());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(11U, range.end());
#else
  // Non-Windows: move/select right by word does NOT include space/punctuation.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  range = model.render_text()->selection();
  EXPECT_FALSE(range.is_empty());
  EXPECT_FALSE(range.is_reversed());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(5U, range.end());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);
  range = model.render_text()->selection();
  EXPECT_FALSE(range.is_empty());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(4U, range.end());

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  range = model.render_text()->selection();
  EXPECT_TRUE(range.is_empty());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(0U, range.end());

  // now from the end.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  range = model.render_text()->selection();
  EXPECT_TRUE(range.is_empty());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(11U, range.end());

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  range = model.render_text()->selection();
  EXPECT_FALSE(range.is_empty());
  EXPECT_TRUE(range.is_reversed());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(6U, range.end());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  range = model.render_text()->selection();
  EXPECT_FALSE(range.is_empty());
  EXPECT_TRUE(range.is_reversed());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(7U, range.end());

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  range = model.render_text()->selection();
  EXPECT_TRUE(range.is_empty());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(11U, range.end());
#endif

  // Select All
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  range = model.render_text()->selection();
  EXPECT_FALSE(range.is_empty());
  EXPECT_TRUE(range.is_reversed());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(0U, range.end());
}

TEST_F(TextfieldModelTest, SelectRangeTest) {
  TextfieldModel model(nullptr);
  model.Append(base::ASCIIToUTF16("HELLO WORLD"));
  gfx::Range range(0, 6);
  EXPECT_FALSE(range.is_reversed());
  model.SelectRange(range);
  EXPECT_TRUE(model.HasSelection());
  EXPECT_STR_EQ("HELLO ", model.GetSelectedText());

  range = gfx::Range(6, 1);
  EXPECT_TRUE(range.is_reversed());
  model.SelectRange(range);
  EXPECT_TRUE(model.HasSelection());
  EXPECT_STR_EQ("ELLO ", model.GetSelectedText());

  range = gfx::Range(2, 1000);
  EXPECT_FALSE(range.is_reversed());
  model.SelectRange(range);
  EXPECT_TRUE(model.HasSelection());
  EXPECT_STR_EQ("LLO WORLD", model.GetSelectedText());

  range = gfx::Range(1000, 3);
  EXPECT_TRUE(range.is_reversed());
  model.SelectRange(range);
  EXPECT_TRUE(model.HasSelection());
  EXPECT_STR_EQ("LO WORLD", model.GetSelectedText());

  range = gfx::Range(0, 0);
  EXPECT_TRUE(range.is_empty());
  model.SelectRange(range);
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.GetSelectedText().empty());

  range = gfx::Range(3, 3);
  EXPECT_TRUE(range.is_empty());
  model.SelectRange(range);
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.GetSelectedText().empty());

  range = gfx::Range(1000, 100);
  EXPECT_FALSE(range.is_empty());
  model.SelectRange(range);
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.GetSelectedText().empty());

  range = gfx::Range(1000, 1000);
  EXPECT_TRUE(range.is_empty());
  model.SelectRange(range);
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.GetSelectedText().empty());

  EXPECT_TRUE(range.is_empty());
  model.SelectRange({1, 5});
  model.SelectRange({100, 7}, false);
  EXPECT_TRUE(model.HasSelection());
  EXPECT_STR_EQ("ELLO", model.GetSelectedText());
  VerifyAllSelectionTexts(&model, {"ELLO", "ORLD"});
}

TEST_F(TextfieldModelTest, SelectionTest) {
  TextfieldModel model(nullptr);
  model.Append(base::ASCIIToUTF16("HELLO WORLD"));
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  gfx::Range selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(0), selection);

#if defined(OS_WIN)  // Select word right includes trailing space/punctuation.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(0, 6), selection);

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(0, 5), selection);
#else  // Non-Windows: select word right does NOT include space/punctuation.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(0, 5), selection);

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(0, 4), selection);
#endif

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(0), selection);
  // now from the end.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(11), selection);

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(11, 6), selection);

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_RETAIN);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(11, 7), selection);

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_RETAIN);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(11), selection);

  // Select All
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(11, 0), selection);
}

TEST_F(TextfieldModelTest, SelectSelectionModelTest) {
  TextfieldModel model(nullptr);
  model.Append(base::ASCIIToUTF16("HELLO WORLD"));
  model.SelectSelectionModel(
      gfx::SelectionModel(gfx::Range(0, 6), gfx::CURSOR_BACKWARD));
  EXPECT_STR_EQ("HELLO ", model.GetSelectedText());

  model.SelectSelectionModel(
      gfx::SelectionModel(gfx::Range(6, 1), gfx::CURSOR_FORWARD));
  EXPECT_STR_EQ("ELLO ", model.GetSelectedText());

  model.SelectSelectionModel(
      gfx::SelectionModel(gfx::Range(2, 1000), gfx::CURSOR_BACKWARD));
  EXPECT_STR_EQ("LLO WORLD", model.GetSelectedText());

  model.SelectSelectionModel(
      gfx::SelectionModel(gfx::Range(1000, 3), gfx::CURSOR_FORWARD));
  EXPECT_STR_EQ("LO WORLD", model.GetSelectedText());

  model.SelectSelectionModel(gfx::SelectionModel(0, gfx::CURSOR_FORWARD));
  EXPECT_TRUE(model.GetSelectedText().empty());

  model.SelectSelectionModel(gfx::SelectionModel(3, gfx::CURSOR_FORWARD));
  EXPECT_TRUE(model.GetSelectedText().empty());

  model.SelectSelectionModel(
      gfx::SelectionModel(gfx::Range(1000, 100), gfx::CURSOR_FORWARD));
  EXPECT_TRUE(model.GetSelectedText().empty());

  model.SelectSelectionModel(gfx::SelectionModel(1000, gfx::CURSOR_BACKWARD));
  EXPECT_TRUE(model.GetSelectedText().empty());

  gfx::SelectionModel mutliselection_selection_model{{2, 3},
                                                     gfx::CURSOR_BACKWARD};
  mutliselection_selection_model.AddSecondarySelection({5, 4});
  mutliselection_selection_model.AddSecondarySelection({1, 0});
  mutliselection_selection_model.AddSecondarySelection({20, 9});
  mutliselection_selection_model.AddSecondarySelection({6, 6});
  model.SelectSelectionModel(mutliselection_selection_model);
  EXPECT_STR_EQ("L", model.GetSelectedText());
  VerifyAllSelectionTexts(&model, {"L", "O", "H", "LD", ""});
}

TEST_F(TextfieldModelTest, CompositionTextTest) {
  TextfieldModel model(this);
  model.Append(base::ASCIIToUTF16("1234590"));
  model.SelectRange(gfx::Range(5, 5));
  EXPECT_FALSE(model.HasSelection());
  EXPECT_EQ(5U, model.GetCursorPosition());

  gfx::Range range;
  model.GetTextRange(&range);
  EXPECT_EQ(gfx::Range(0, 7), range);

  ui::CompositionText composition;
  composition.text = base::ASCIIToUTF16("678");
  composition.ime_text_spans.push_back(
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0, 3,
                      ui::ImeTextSpan::Thickness::kThin));

  // Cursor should be at the end of composition when characters are just typed.
  composition.selection = gfx::Range(3, 3);
  model.SetCompositionText(composition);
  EXPECT_TRUE(model.HasCompositionText());
  EXPECT_FALSE(model.HasSelection());

  // Cancel the composition.
  model.CancelCompositionText();
  composition_text_confirmed_or_cleared_ = false;

  // Restart composition with targeting "67" in "678".
  composition.selection = gfx::Range(1, 3);
  composition.ime_text_spans.clear();
  composition.ime_text_spans.push_back(
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0, 2,
                      ui::ImeTextSpan::Thickness::kThick));
  composition.ime_text_spans.push_back(
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 2, 3,
                      ui::ImeTextSpan::Thickness::kThin));
  model.SetCompositionText(composition);
  EXPECT_TRUE(model.HasCompositionText());
  EXPECT_TRUE(model.HasSelection());
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // |composition.selection| is ignored because SetCompositionText checks
  // if a thick underline exists first.
  EXPECT_EQ(gfx::Range(5, 7), model.render_text()->selection());
  EXPECT_EQ(7U, model.render_text()->cursor_position());
#else
  // See SelectRangeInCompositionText().
  EXPECT_EQ(gfx::Range(7, 5), model.render_text()->selection());
  EXPECT_EQ(5U, model.render_text()->cursor_position());
#endif

  model.GetTextRange(&range);
  EXPECT_EQ(10U, range.end());
  EXPECT_STR_EQ("1234567890", model.text());

  model.GetCompositionTextRange(&range);
  EXPECT_EQ(gfx::Range(5, 8), range);
  // Check the composition text.
  EXPECT_STR_EQ("456", model.GetTextFromRange(gfx::Range(3, 6)));

  EXPECT_FALSE(composition_text_confirmed_or_cleared_);
  model.CancelCompositionText();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_FALSE(model.HasCompositionText());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_EQ(5U, model.GetCursorPosition());

  model.SetCompositionText(composition);
  EXPECT_STR_EQ("1234567890", model.text());
  EXPECT_TRUE(model.SetText(base::ASCIIToUTF16("1234567890"), 0));
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);

  // Also test the case where a selection exists but a thick underline doesn't.
  composition.selection = gfx::Range(0, 1);
  composition.ime_text_spans.clear();
  model.SetCompositionText(composition);
  EXPECT_STR_EQ("1234567890678", model.text());
  EXPECT_TRUE(model.HasSelection());
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(gfx::Range(10, 11), model.render_text()->selection());
  EXPECT_EQ(11U, model.render_text()->cursor_position());
#else
  // See SelectRangeInCompositionText().
  EXPECT_EQ(gfx::Range(11, 10), model.render_text()->selection());
  EXPECT_EQ(10U, model.render_text()->cursor_position());
#endif

  model.InsertText(base::UTF8ToUTF16("-"));
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("1234567890-", model.text());
  EXPECT_FALSE(model.HasCompositionText());
  EXPECT_FALSE(model.HasSelection());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                   gfx::SELECTION_RETAIN);
  EXPECT_STR_EQ("-", model.GetSelectedText());
  model.SetCompositionText(composition);
  EXPECT_STR_EQ("1234567890678", model.text());

  model.ReplaceText(base::UTF8ToUTF16("-"));
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("1234567890-", model.text());
  EXPECT_FALSE(model.HasCompositionText());
  EXPECT_FALSE(model.HasSelection());

  model.SetCompositionText(composition);
  model.Append(base::UTF8ToUTF16("-"));
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("1234567890-678-", model.text());

  model.SetCompositionText(composition);
  model.Delete();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("1234567890-678-", model.text());

  model.SetCompositionText(composition);
  model.Backspace();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("1234567890-678-", model.text());

  model.SetText(base::string16(), 0);
  model.SetCompositionText(composition);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());

  model.SetCompositionText(composition);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_NONE);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("676788", model.text());
  EXPECT_EQ(6U, model.GetCursorPosition());

  model.SetCompositionText(composition);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("676788678", model.text());

  model.SetText(base::string16(), 0);
  model.SetCompositionText(composition);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;

  model.SetCompositionText(composition);
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_RETAIN);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678678", model.text());

  model.SetCompositionText(composition);
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678", model.text());

  model.SetCompositionText(composition);
  gfx::SelectionModel sel(
      gfx::Range(model.render_text()->selection().start(), 0),
      gfx::CURSOR_FORWARD);
  model.MoveCursorTo(sel);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678678", model.text());

  model.SetCompositionText(composition);
  model.SelectRange(gfx::Range(0, 3));
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678", model.text());

  model.SetCompositionText(composition);
  model.SelectAll(false);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678", model.text());

  model.SetCompositionText(composition);
  model.SelectWord();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678", model.text());

  model.SetCompositionText(composition);
  model.ClearSelection();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;

  model.SetCompositionText(composition);
  EXPECT_FALSE(model.Cut());
  EXPECT_FALSE(composition_text_confirmed_or_cleared_);
}

TEST_F(TextfieldModelTest, UndoRedo_BasicTest) {
  TextfieldModel model(nullptr);
  model.InsertChar('a');
  EXPECT_FALSE(model.Redo());  // There is nothing to redo.
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("a", model.text());

  // Continuous inserts are treated as one edit.
  model.InsertChar('b');
  model.InsertChar('c');
  EXPECT_STR_EQ("abc", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("a", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());

  // Undoing further shouldn't change the text.
  EXPECT_FALSE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_FALSE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());

  // Redoing to the latest text.
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("a", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("abc", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());

  // Backspace ===============================
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("ab", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("abc", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ab", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  // Continous backspaces are treated as one edit.
  EXPECT_TRUE(model.Backspace());
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("", model.text());
  // Extra backspace shouldn't affect the history.
  EXPECT_FALSE(model.Backspace());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ab", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("abc", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("a", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());

  // Clear history
  model.ClearEditHistory();
  EXPECT_FALSE(model.Undo());
  EXPECT_FALSE(model.Redo());
  EXPECT_STR_EQ("a", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());

  // Delete ===============================
  model.SetText(base::ASCIIToUTF16("ABCDE"), 0);
  model.ClearEditHistory();
  model.MoveCursorTo(2);
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("ABDE", model.text());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("BDE", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABDE", model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABDE", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  // Continous deletes are treated as one edit.
  EXPECT_TRUE(model.Delete());
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("AB", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABDE", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("AB", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
}

TEST_F(TextfieldModelTest, UndoRedo_SetText) {
  // This is to test the undo/redo behavior of omnibox.
  TextfieldModel model(nullptr);
  // Simulate typing www.y while www.google.com and www.youtube.com are
  // autocompleted.
  model.InsertChar('w');  //                                    w|
  EXPECT_STR_EQ("w", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  model.SetText(base::ASCIIToUTF16("www.google.com"), 1);  //   w|ww.google.com
  model.SelectRange(gfx::Range(14, 1));  //                     w[ww.google.com]
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_STR_EQ("www.google.com", model.text());
  model.InsertChar('w');  //                                    ww|
  EXPECT_STR_EQ("ww", model.text());
  model.SetText(base::ASCIIToUTF16("www.google.com"), 2);  //   ww|w.google.com
  model.SelectRange(gfx::Range(14, 2));  //                     ww[w.google.com]
  model.InsertChar('w');  //                                    www|
  EXPECT_STR_EQ("www", model.text());
  model.SetText(base::ASCIIToUTF16("www.google.com"), 3);  //   www|.google.com
  model.SelectRange(gfx::Range(14, 3));  //                     www[.google.com]
  model.InsertChar('.');  //                                    www.|
  EXPECT_STR_EQ("www.", model.text());
  model.SetText(base::ASCIIToUTF16("www.google.com"), 4);  //   www.|google.com
  model.SelectRange(gfx::Range(14, 4));  //                     www.[google.com]
  model.InsertChar('y');  //                                    www.y|
  EXPECT_STR_EQ("www.y", model.text());
  model.SetText(base::ASCIIToUTF16("www.youtube.com"), 5);  //  www.y|outube.com
  EXPECT_STR_EQ("www.youtube.com", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());

  // Undo until the initial edit.
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(4U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_FALSE(model.Undo());

  // Redo until the last edit.
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(4U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("www.youtube.com", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());
}

TEST_F(TextfieldModelTest, UndoRedo_BackspaceThenSetText) {
  // This is to test the undo/redo behavior of omnibox.
  TextfieldModel model(nullptr);
  model.InsertChar('w');
  EXPECT_STR_EQ("w", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  model.SetText(base::ASCIIToUTF16("www.google.com"), 1);
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  EXPECT_EQ(14U, model.GetCursorPosition());
  EXPECT_TRUE(model.Backspace());
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("www.google.c", model.text());
  // Autocomplete sets the text.
  model.SetText(base::ASCIIToUTF16("www.google.com/search=www.google.c"), 12);
  EXPECT_STR_EQ("www.google.com/search=www.google.c", model.text());
  EXPECT_EQ(12U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.c", model.text());
  EXPECT_EQ(12U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(14U, model.GetCursorPosition());
}

TEST_F(TextfieldModelTest, UndoRedo_CutCopyPasteTest) {
  TextfieldModel model(nullptr);
  model.SetText(base::ASCIIToUTF16("ABCDE"), 5);
  EXPECT_FALSE(model.Redo());  // There is nothing to redo.
  // Test Cut.
  model.SelectRange(gfx::Range(1, 3));  //                         A[BC]DE
  EXPECT_EQ(3U, model.GetCursorPosition());
  model.Cut();  //                                                 A|DE
  EXPECT_STR_EQ("ADE", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());  //                                   A[BC]DE
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.render_text()->selection().EqualsIgnoringDirection(
      gfx::Range(1, 3)));
  EXPECT_TRUE(model.Undo());  //                                   |
  EXPECT_STR_EQ("", model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_FALSE(model.Undo());  // There is no more to undo.        |
  EXPECT_STR_EQ("", model.text());
  EXPECT_TRUE(model.Redo());  //                                   ABCDE|
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());  //                                   A|DE
  EXPECT_STR_EQ("ADE", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());  // There is no more to redo.        A|DE
  EXPECT_STR_EQ("ADE", model.text());

  model.Paste();  //                                               ABC|DE
  model.Paste();  //                                               ABCBC|DE
  model.Paste();  //                                               ABCBCBC|DE
  EXPECT_STR_EQ("ABCBCBCDE", model.text());
  EXPECT_EQ(7U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());  //                                   ABCBC|DE
  EXPECT_STR_EQ("ABCBCDE", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());  //                                   ABC|DE
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());  //                                   A|DE
  EXPECT_STR_EQ("ADE", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());  //                                   A[BC]DE
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.render_text()->selection().EqualsIgnoringDirection(
      gfx::Range(1, 3)));
  EXPECT_TRUE(model.Undo());  //                                   |
  EXPECT_STR_EQ("", model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_FALSE(model.Undo());  //                                  |
  EXPECT_STR_EQ("", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDE", model.text());  //                        ABCDE|
  EXPECT_EQ(5U, model.GetCursorPosition());

  // Test Redo.
  EXPECT_TRUE(model.Redo());  //                                   A|DE
  EXPECT_STR_EQ("ADE", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());  //                                   ABC|DE
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());  //                                   ABCBC|DE
  EXPECT_STR_EQ("ABCBCDE", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());  //                                   ABCBCBC|DE
  EXPECT_STR_EQ("ABCBCBCDE", model.text());
  EXPECT_EQ(7U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());  //                                  ABCBCBC|DE

  // Test using SelectRange.
  model.SelectRange(gfx::Range(1, 3));  //                         A[BC]BCBCDE
  EXPECT_TRUE(model.Cut());  //                                    A|BCBCDE
  EXPECT_STR_EQ("ABCBCDE", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  model.SelectRange(gfx::Range(1, 1));  //                         A|BCBCDE
  EXPECT_FALSE(model.Cut());  //                                   A|BCBCDE
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  //                                                               ABCBCDE|
  EXPECT_TRUE(model.Paste());  //                                  ABCBCDEBC|
  EXPECT_STR_EQ("ABCBCDEBC", model.text());
  EXPECT_EQ(9U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());  //                                   ABCBCDE|
  EXPECT_STR_EQ("ABCBCDE", model.text());
  EXPECT_EQ(7U, model.GetCursorPosition());
  // An empty cut shouldn't create an edit.
  EXPECT_TRUE(model.Undo());  //                                   ABC|BCBCDE
  EXPECT_STR_EQ("ABCBCBCDE", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.render_text()->selection().EqualsIgnoringDirection(
      gfx::Range(1, 3)));
  // Test Copy.
  ResetModel(&model);
  model.SetText(base::ASCIIToUTF16("12345"), 5);  //                  12345|
  EXPECT_STR_EQ("12345", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  model.SelectRange(gfx::Range(1, 3));  //                         1[23]45
  model.Copy();  // Copy "23".  //                                 1[23]45
  EXPECT_STR_EQ("12345", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  model.Paste();  // Paste "23" into "23".  //                     123|45
  EXPECT_STR_EQ("12345", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  model.Paste();  //                                               12323|45
  EXPECT_STR_EQ("1232345", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());  //                                   123|45
  EXPECT_STR_EQ("12345", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  // TODO(oshima): Change the return type from bool to enum.
  EXPECT_FALSE(model.Undo());  // No text change.                  1[23]45
  EXPECT_STR_EQ("12345", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.render_text()->selection().EqualsIgnoringDirection(
      gfx::Range(1, 3)));
  EXPECT_TRUE(model.Undo());  //                                   |
  EXPECT_STR_EQ("", model.text());
  EXPECT_FALSE(model.Undo());  //                                  |
  // Test Redo.
  EXPECT_TRUE(model.Redo());  //                                   12345|
  EXPECT_STR_EQ("12345", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());  //                                   12|345
  EXPECT_STR_EQ("12345", model.text());  // For 1st paste
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());  //                                   12323|45
  EXPECT_STR_EQ("1232345", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());  //                                  12323|45
  EXPECT_STR_EQ("1232345", model.text());

  // Test using SelectRange.
  model.SelectRange(gfx::Range(1, 3));  //                         1[23]2345
  model.Copy();  //                                                1[23]2345
  EXPECT_STR_EQ("1232345", model.text());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  //                                                               1232345|
  EXPECT_TRUE(model.Paste());  //                                  123234523|
  EXPECT_STR_EQ("123234523", model.text());
  EXPECT_EQ(9U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());  //                                   1232345|
  EXPECT_STR_EQ("1232345", model.text());
  EXPECT_EQ(7U, model.GetCursorPosition());
}

TEST_F(TextfieldModelTest, UndoRedo_CursorTest) {
  TextfieldModel model(nullptr);
  model.InsertChar('a');
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                   gfx::SELECTION_NONE);
  model.InsertChar('b');
  // Moving the cursor shouldn't create a new edit.
  EXPECT_STR_EQ("ab", model.text());
  EXPECT_FALSE(model.Redo());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_FALSE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ab", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());
}

TEST_F(TextfieldModelTest, Undo_SelectionTest) {
  gfx::Range range = gfx::Range(2, 4);
  TextfieldModel model(nullptr);
  model.SetText(base::ASCIIToUTF16("abcdef"), 0);
  model.SelectRange(range);
  EXPECT_EQ(model.render_text()->selection(), range);

  // Deleting the selected text should change the text and the range.
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("abef", model.text());
  EXPECT_EQ(model.render_text()->selection(), gfx::Range(2, 2));

  // Undoing the deletion should restore the former range.
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("abcdef", model.text());
  EXPECT_EQ(model.render_text()->selection(), range);

  // When range.start = range.end, nothing is selected and
  // range.start = range.end = cursor position
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  EXPECT_EQ(model.render_text()->selection(), gfx::Range(2, 2));

  // Deleting a single character should change the text and cursor location.
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("acdef", model.text());
  EXPECT_EQ(model.render_text()->selection(), gfx::Range(1, 1));

  // Undoing the deletion should restore the former range.
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("abcdef", model.text());
  EXPECT_EQ(model.render_text()->selection(), gfx::Range(2, 2));

  model.MoveCursorTo(model.text().length());
  EXPECT_TRUE(model.Backspace());
  model.SelectRange(gfx::Range(1, 3));
  model.SetText(base::ASCIIToUTF16("[set]"), 0);
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("abcde", model.text());
  EXPECT_EQ(model.render_text()->selection(), gfx::Range(1, 3));
}

void RunInsertReplaceTest(TextfieldModel* model) {
  const bool reverse = model->render_text()->selection().is_reversed();
  model->InsertChar('1');
  model->InsertChar('2');
  model->InsertChar('3');
  EXPECT_STR_EQ("a123d", model->text());
  EXPECT_EQ(4U, model->GetCursorPosition());
  EXPECT_TRUE(model->Undo());
  EXPECT_STR_EQ("abcd", model->text());
  EXPECT_EQ(reverse ? 1U : 3U, model->GetCursorPosition());
  EXPECT_TRUE(model->Undo());
  EXPECT_STR_EQ("", model->text());
  EXPECT_EQ(0U, model->GetCursorPosition());
  EXPECT_FALSE(model->Undo());
  EXPECT_TRUE(model->Redo());
  EXPECT_STR_EQ("abcd", model->text());
  EXPECT_EQ(4U, model->GetCursorPosition());
  EXPECT_TRUE(model->Redo());
  EXPECT_STR_EQ("a123d", model->text());
  EXPECT_EQ(4U, model->GetCursorPosition());
  EXPECT_FALSE(model->Redo());
}

void RunOverwriteReplaceTest(TextfieldModel* model) {
  const bool reverse = model->render_text()->selection().is_reversed();
  model->ReplaceChar('1');
  model->ReplaceChar('2');
  model->ReplaceChar('3');
  model->ReplaceChar('4');
  EXPECT_STR_EQ("a1234", model->text());
  EXPECT_EQ(5U, model->GetCursorPosition());
  EXPECT_TRUE(model->Undo());
  EXPECT_STR_EQ("abcd", model->text());
  EXPECT_EQ(reverse ? 1U : 3U, model->GetCursorPosition());
  EXPECT_TRUE(model->Undo());
  EXPECT_STR_EQ("", model->text());
  EXPECT_EQ(0U, model->GetCursorPosition());
  EXPECT_FALSE(model->Undo());
  EXPECT_TRUE(model->Redo());
  EXPECT_STR_EQ("abcd", model->text());
  EXPECT_EQ(4U, model->GetCursorPosition());
  EXPECT_TRUE(model->Redo());
  EXPECT_STR_EQ("a1234", model->text());
  EXPECT_EQ(5U, model->GetCursorPosition());
  EXPECT_FALSE(model->Redo());
}

TEST_F(TextfieldModelTest, UndoRedo_ReplaceTest) {
  {
    SCOPED_TRACE("Select forwards and insert.");
    TextfieldModel model(nullptr);
    model.SetText(base::ASCIIToUTF16("abcd"), 4);
    model.SelectRange(gfx::Range(1, 3));
    RunInsertReplaceTest(&model);
  }
  {
    SCOPED_TRACE("Select reversed and insert.");
    TextfieldModel model(nullptr);
    model.SetText(base::ASCIIToUTF16("abcd"), 4);
    model.SelectRange(gfx::Range(3, 1));
    RunInsertReplaceTest(&model);
  }
  {
    SCOPED_TRACE("Select forwards and overwrite.");
    TextfieldModel model(nullptr);
    model.SetText(base::ASCIIToUTF16("abcd"), 4);
    model.SelectRange(gfx::Range(1, 3));
    RunOverwriteReplaceTest(&model);
  }
  {
    SCOPED_TRACE("Select reversed and overwrite.");
    TextfieldModel model(nullptr);
    model.SetText(base::ASCIIToUTF16("abcd"), 4);
    model.SelectRange(gfx::Range(3, 1));
    RunOverwriteReplaceTest(&model);
  }
}

TEST_F(TextfieldModelTest, UndoRedo_CompositionText) {
  TextfieldModel model(nullptr);

  ui::CompositionText composition;
  composition.text = base::ASCIIToUTF16("abc");
  composition.ime_text_spans.push_back(
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0, 3,
                      ui::ImeTextSpan::Thickness::kThin));
  composition.selection = gfx::Range(2, 3);

  model.SetText(base::ASCIIToUTF16("ABCDE"), 0);
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  model.InsertChar('x');
  EXPECT_STR_EQ("ABCDEx", model.text());
  EXPECT_TRUE(model.Undo());  // set composition should forget undone edit.
  model.SetCompositionText(composition);
  EXPECT_TRUE(model.HasCompositionText());
  EXPECT_TRUE(model.HasSelection());
  EXPECT_STR_EQ("ABCDEabc", model.text());

  // Confirm the composition.
  uint32_t composition_text_length = model.ConfirmCompositionText();
  EXPECT_EQ(composition_text_length, static_cast<uint32_t>(3));
  EXPECT_STR_EQ("ABCDEabc", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDEabc", model.text());
  EXPECT_FALSE(model.Redo());

  // Cancel the composition.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, gfx::SELECTION_NONE);
  model.SetCompositionText(composition);
  EXPECT_STR_EQ("abcABCDEabc", model.text());
  model.CancelCompositionText();
  EXPECT_STR_EQ("ABCDEabc", model.text());
  EXPECT_FALSE(model.Redo());
  EXPECT_STR_EQ("ABCDEabc", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDEabc", model.text());
  EXPECT_FALSE(model.Redo());

  // Call SetText with the same text as the result.
  ResetModel(&model);
  model.SetText(base::ASCIIToUTF16("ABCDE"), 0);
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  model.SetCompositionText(composition);
  EXPECT_STR_EQ("ABCDEabc", model.text());
  model.SetText(base::ASCIIToUTF16("ABCDEabc"), 0);
  EXPECT_STR_EQ("ABCDEabc", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDEabc", model.text());
  EXPECT_FALSE(model.Redo());

  // Call SetText with a different result; the composition should be forgotten.
  ResetModel(&model);
  model.SetText(base::ASCIIToUTF16("ABCDE"), 0);
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  model.SetCompositionText(composition);
  EXPECT_STR_EQ("ABCDEabc", model.text());
  model.SetText(base::ASCIIToUTF16("1234"), 0);
  EXPECT_STR_EQ("1234", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("1234", model.text());
  EXPECT_FALSE(model.Redo());

  // TODO(oshima): Test the behavior with an IME.
}

TEST_F(TextfieldModelTest, UndoRedo_TypingWithSecondarySelections) {
  TextfieldModel model(nullptr);

  // Type 'ab cd' as 'prefix ab xy suffix' and 'prefix ab cd suffix' are
  // autocompleted.
  // Type 'a', autocomplete [prefix ]a[b xy suffix]
  model.InsertChar('a');
  model.SetText(base::ASCIIToUTF16("prefix ab xy suffix"), 8);
  model.SelectRange({19, 8});
  model.SelectRange({0, 7}, false);

  // Type 'ab', autocomplete [prefix ]ab[ xy suffix]
  model.InsertChar('b');
  model.SetText(base::ASCIIToUTF16("prefix ab xy suffix"), 9);
  model.SelectRange({19, 9});
  model.SelectRange({0, 7}, false);

  // Type 'ab ', autocomplete [prefix ]ab [xy suffix]
  model.InsertChar(' ');
  model.SetText(base::ASCIIToUTF16("prefix ab xy suffix"), 10);
  model.SelectRange({19, 10});
  model.SelectRange({0, 7}, false);

  // Type 'ab c', autocomplete changed to [prefix ]ab c[d suffix]
  model.InsertChar('c');
  model.SetText(base::ASCIIToUTF16("prefix ab cd suffix"), 11);
  model.SelectRange({19, 11});
  model.SelectRange({0, 7}, false);

  // Type 'ab cd', autocomplete [prefix ]ab cd[ suffix]
  model.InsertChar('d');
  model.SetText(base::ASCIIToUTF16("prefix ab cd suffix"), 12);
  model.SelectRange({19, 12});
  model.SelectRange({0, 7}, false);

  // Undo 3 times
  EXPECT_TRUE(model.Undo());  // [prefix ]ab c[d suffix]
  EXPECT_STR_EQ("prefix ab cd suffix", model.text());
  EXPECT_EQ(11U, model.GetCursorPosition());
  VerifyAllSelectionTexts(&model, {"d suffix", "prefix "});

  EXPECT_TRUE(model.Undo());  // [prefix ]ab [xy suffix]
  EXPECT_STR_EQ("prefix ab xy suffix", model.text());
  EXPECT_EQ(10U, model.GetCursorPosition());
  VerifyAllSelectionTexts(&model, {"xy suffix", "prefix "});

  EXPECT_TRUE(model.Undo());  // [prefix ]ab[ xy suffix]
  EXPECT_STR_EQ("prefix ab xy suffix", model.text());
  EXPECT_EQ(9U, model.GetCursorPosition());
  VerifyAllSelectionTexts(&model, {" xy suffix", "prefix "});

  // Redo 3 times
  EXPECT_TRUE(model.Redo());  // [prefix ]ab [xy suffix]
  EXPECT_STR_EQ("prefix ab xy suffix", model.text());
  EXPECT_EQ(10U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());

  EXPECT_TRUE(model.Redo());  // [prefix ]ab c[d suffix]
  EXPECT_STR_EQ("prefix ab cd suffix", model.text());
  EXPECT_EQ(11U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());

  EXPECT_TRUE(model.Redo());  // [prefix ]ab cd[ suffix]
  EXPECT_STR_EQ("prefix ab cd suffix", model.text());
  EXPECT_EQ(12U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
}

TEST_F(TextfieldModelTest, UndoRedo_MergingEditsWithSecondarySelections) {
  TextfieldModel model(nullptr);

  // Test all possible merge combinations involving secondary selections.
  // I.e. an initial [replace or delete] edit with secondary selections,
  // followed by a second and third [insert, replace, or delete] edits, which
  // are [continuous and discontinuous] respectively. Some cases of the third,
  // discontinuous edit have been omitted when the the second edit would not
  // been merged anyways.

  // Note, the cursor and selections depend on whether we're traversing forward
  // or backwards through edit history. I.e., `undo(); redo();` can result in a
  // different outcome than `redo(); undo();`. In general, if our edit history
  // consists of 3 edits: A->B, C->D, & E->F, then undo will traverse
  // F->E->C->A, while redo will traverse A->B->D->F. Though, B & C and D & E
  // will have the same text, they can have different cursors and selections.

  // Replacement with secondary selections followed by insertions
  model.SetText(base::ASCIIToUTF16("prefix infix suffix"), 13);
  model.SelectRange({18, 13});
  model.SelectRange({1, 6}, false);  //                 p[refix] infix [suffi]x
  // Replace
  model.InsertChar('1');  //                            p infix 1|x
  // Continuous insert (should merge)
  model.InsertChar('3');  //                            p infix 13|x
  // Discontinuous insert (should not merge)
  model.SelectRange({9, 9});  //                        p infix 1|3x
  model.InsertChar('2');      //                        p infix 12|3x
  EXPECT_STR_EQ("p infix 123x", model.text());
  EXPECT_FALSE(model.HasSelection());
  // Edit history should be
  // p[refix] infix [suffi]x -> p infix 13|x
  // p infix 1|3x -> p infix 12|3x
  // Undo 2 times
  EXPECT_TRUE(model.Undo());  //                        p infix 1|3x
  EXPECT_STR_EQ("p infix 13x", model.text());
  EXPECT_EQ(9U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.Undo());  //                        p[refix] infix [suffi]x
  EXPECT_STR_EQ("prefix infix suffix", model.text());
  EXPECT_EQ(13U, model.GetCursorPosition());
  VerifyAllSelectionTexts(&model, {"suffi", "refix"});
  // Redo 2 times
  EXPECT_TRUE(model.Redo());  //                        p infix 13|x
  EXPECT_STR_EQ("p infix 13x", model.text());
  EXPECT_EQ(10U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.Redo());  //                        p infix 12|3x
  EXPECT_STR_EQ("p infix 123x", model.text());
  EXPECT_EQ(10U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_FALSE(model.Redo());

  // Replacement with secondary selections followed by replacements
  model.SetText(base::ASCIIToUTF16("prefix infix suffix"), 13);
  model.SelectRange({15, 13});
  model.SelectRange({1, 6}, false);  //                 p[refix] infix [su]ffix
  // Replace
  model.InsertChar('1');  //                            p infix 1|ffix
  // Continuous multiple characters, and backwards replace (should merge)
  model.SelectRange({11, 9});  //                       p infix 1[ff]ix
  model.InsertChar('3');       //                       p infix 13|ix
  // Discontinuous but adjacent replace (should not merge)
  model.SelectRange({10, 9});  //                       p infix 1[3]ix
  model.InsertChar('2');       //                       p infix 12|ix
  EXPECT_STR_EQ("p infix 12ix", model.text());
  EXPECT_FALSE(model.HasSelection());
  // Edit history should be
  // p[refix] infix [su]ffix -> p infix 13|ix
  // p infix 1[3]ix -> p infix 12|ix
  // Undo 2 times
  EXPECT_TRUE(model.Undo());  //                        p infix 1[3]ix
  EXPECT_STR_EQ("p infix 13ix", model.text());
  EXPECT_EQ(9U, model.GetCursorPosition());
  VerifyAllSelectionTexts(&model, {"3"});
  EXPECT_TRUE(model.Undo());  //                        p[refix] infix [su]ffix
  EXPECT_STR_EQ("prefix infix suffix", model.text());
  EXPECT_EQ(13U, model.GetCursorPosition());
  VerifyAllSelectionTexts(&model, {"su", "refix"});
  // Redo 2 times
  EXPECT_TRUE(model.Redo());  //                        p infix 13|ix
  EXPECT_STR_EQ("p infix 13ix", model.text());
  EXPECT_EQ(10U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.Redo());  //                        p infix 12|ix
  EXPECT_STR_EQ("p infix 12ix", model.text());
  EXPECT_EQ(10U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_FALSE(model.Redo());

  // Replacement with secondary selections followed by deletion
  model.SetText(base::ASCIIToUTF16("prefix infix suffix"), 13);
  model.SelectRange({15, 13});
  model.SelectRange({1, 6}, false);  //                 p[refix] infix [su]ffix
  // Replace
  model.InsertChar('1');  //                            p infix 1|ffix
  // Continuous delete (should not merge)
  model.Delete(false);  //                              p infix 1|fix
  EXPECT_STR_EQ("p infix 1fix", model.text());
  EXPECT_FALSE(model.HasSelection());
  // Edit history should be
  // p[refix] infix [su]ffix -> p infix 1|ffix
  // p infix 1|ffix -> p infix 1|fix
  // Undo 2 times
  EXPECT_TRUE(model.Undo());  //                        p infix 1|ffix
  EXPECT_STR_EQ("p infix 1ffix", model.text());
  EXPECT_EQ(9U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.Undo());  //                        p[refix] infix [su]ffix
  EXPECT_STR_EQ("prefix infix suffix", model.text());
  EXPECT_EQ(13U, model.GetCursorPosition());
  VerifyAllSelectionTexts(&model, {"su", "refix"});
  // Redo 2 times
  EXPECT_TRUE(model.Redo());  //                        p infix 1|ffix
  EXPECT_STR_EQ("p infix 1ffix", model.text());
  EXPECT_EQ(9U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.Redo());  //                        p infix 1|fix
  EXPECT_STR_EQ("p infix 1fix", model.text());
  EXPECT_EQ(9U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_FALSE(model.Redo());

  // Deletion with secondary selections followed by insertion
  model.SetText(base::ASCIIToUTF16("prefix infix suffix"), 13);
  model.SelectRange({15, 13});
  model.SelectRange({1, 6}, false);  //                 p[refix] infix [su]ffix
  // Delete
  model.Delete(false);  //                              p infix |ffix
  // Continuous insert (should not merge)
  model.InsertChar('1');  //                            p infix 1|ffix
  EXPECT_STR_EQ("p infix 1ffix", model.text());
  EXPECT_FALSE(model.HasSelection());
  // Edit history should be
  // p[refix] infix [su]ffix -> p infix |ffix
  // p infix |ffix -> p infix 1|ffix
  // Undo 2 times
  EXPECT_TRUE(model.Undo());  //                        p infix |ffix
  EXPECT_STR_EQ("p infix ffix", model.text());
  EXPECT_EQ(8U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.Undo());  //                        p[refix] infix [su]ffix
  EXPECT_STR_EQ("prefix infix suffix", model.text());
  EXPECT_EQ(13U, model.GetCursorPosition());
  VerifyAllSelectionTexts(&model, {"su", "refix"});
  // Redo 2 times
  EXPECT_TRUE(model.Redo());  //                        p infix |ffix
  EXPECT_STR_EQ("p infix ffix", model.text());
  EXPECT_EQ(8U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.Redo());  //                        p infix 1|ffix
  EXPECT_STR_EQ("p infix 1ffix", model.text());
  EXPECT_EQ(9U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_FALSE(model.Redo());

  // Deletion with secondary selections followed by replacement
  model.SetText(base::ASCIIToUTF16("prefix infix suffix"), 13);
  model.SelectRange({15, 13});
  model.SelectRange({1, 6}, false);  //                 p[refix] infix [su]ffix
  // Delete
  model.Delete(false);  //                              p infix |ffix
  // Continuous replacement (should not merge)
  model.SelectRange({8, 9});  //                        p infix [f]fix
  model.InsertChar('1');      //                        p infix 1|fix
  EXPECT_STR_EQ("p infix 1fix", model.text());
  EXPECT_FALSE(model.HasSelection());
  // Edit history should be
  // p[refix] infix [su]ffix -> p infix |ffix
  // p infix [f]fix -> p infix 1|fix
  // Undo 2 times
  EXPECT_TRUE(model.Undo());  //                        p infix [f]fix
  EXPECT_STR_EQ("p infix ffix", model.text());
  EXPECT_EQ(9U, model.GetCursorPosition());
  VerifyAllSelectionTexts(&model, {"f"});
  EXPECT_TRUE(model.Undo());  //                        p[refix] infix [su]ffix
  EXPECT_STR_EQ("prefix infix suffix", model.text());
  EXPECT_EQ(13U, model.GetCursorPosition());
  VerifyAllSelectionTexts(&model, {"su", "refix"});
  // Redo 2 times
  EXPECT_TRUE(model.Redo());  //                        p infix |ffix
  EXPECT_STR_EQ("p infix ffix", model.text());
  EXPECT_EQ(8U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.Redo());  //                        p infix 1|fix
  EXPECT_STR_EQ("p infix 1fix", model.text());
  EXPECT_EQ(9U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_FALSE(model.Redo());

  // Deletion with secondary selections followed by deletion
  model.SetText(base::ASCIIToUTF16("prefix infix suffix"), 13);
  model.SelectRange({15, 13});
  model.SelectRange({1, 6}, false);  //                 p[refix] infix [su]ffix
  // Delete
  model.Delete(false);  //                              p infix |ffix
  // Continuous delete (should not merge)
  model.Delete(false);  //                              p infix |fix
  EXPECT_STR_EQ("p infix fix", model.text());
  EXPECT_FALSE(model.HasSelection());
  // Edit history should be
  // p[refix] infix [su]ffix -> p infix |ffix
  // p infix |ffix -> p infix |fix
  // Undo 2 times
  EXPECT_TRUE(model.Undo());  //                        p infix |ffix
  EXPECT_STR_EQ("p infix ffix", model.text());
  EXPECT_EQ(8U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.Undo());  //                        p[refix] infix [su]ffix
  EXPECT_STR_EQ("prefix infix suffix", model.text());
  EXPECT_EQ(13U, model.GetCursorPosition());
  VerifyAllSelectionTexts(&model, {"su", "refix"});
  // Redo 2 times
  EXPECT_TRUE(model.Redo());  //                        p infix |ffix
  EXPECT_STR_EQ("p infix ffix", model.text());
  EXPECT_EQ(8U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_TRUE(model.Redo());  //                        p infix |fix
  EXPECT_STR_EQ("p infix fix", model.text());
  EXPECT_EQ(8U, model.GetCursorPosition());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_FALSE(model.Redo());
}

// Tests that clipboard text with leading, trailing and interspersed tabs
// spaces etc is pasted correctly. Leading and trailing tabs should be
// stripped. Text separated by multiple tabs/spaces should be left alone.
// Text with just tabs and spaces should be pasted as one space.
TEST_F(TextfieldModelTest, Clipboard_WhiteSpaceStringTest) {
  // Test 1
  // Clipboard text with a leading tab should be pasted with the tab stripped.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::ASCIIToUTF16("\tB"));

  TextfieldModel model(nullptr);
  model.Append(base::ASCIIToUTF16("HELLO WORLD"));
  EXPECT_STR_EQ("HELLO WORLD", model.text());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  EXPECT_EQ(11U, model.GetCursorPosition());

  EXPECT_TRUE(model.Paste());
  EXPECT_STR_EQ("HELLO WORLDB", model.text());

  model.SelectAll(false);
  model.DeleteSelection();
  EXPECT_STR_EQ("", model.text());

  // Test 2
  // Clipboard text with multiple leading tabs and spaces should be pasted with
  // all tabs and spaces stripped.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::ASCIIToUTF16("\t\t\t B"));

  model.Append(base::ASCIIToUTF16("HELLO WORLD"));
  EXPECT_STR_EQ("HELLO WORLD", model.text());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  EXPECT_EQ(11U, model.GetCursorPosition());
  EXPECT_TRUE(model.Paste());
  EXPECT_STR_EQ("HELLO WORLDB", model.text());

  model.SelectAll(false);
  model.DeleteSelection();
  EXPECT_STR_EQ("", model.text());

  // Test 3
  // Clipboard text with multiple tabs separating the words should be pasted
  // as-is.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::ASCIIToUTF16("FOO \t\t BAR"));

  model.Append(base::ASCIIToUTF16("HELLO WORLD"));
  EXPECT_STR_EQ("HELLO WORLD", model.text());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, gfx::SELECTION_NONE);
  EXPECT_EQ(11U, model.GetCursorPosition());
  EXPECT_TRUE(model.Paste());
  EXPECT_STR_EQ("HELLO WORLDFOO \t\t BAR", model.text());

  model.SelectAll(false);
  model.DeleteSelection();
  EXPECT_STR_EQ("", model.text());

  // Test 4
  // Clipboard text with multiple leading tabs and multiple tabs separating
  // the words should be pasted with the leading tabs stripped.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::ASCIIToUTF16("\t\tFOO \t\t BAR"));

  EXPECT_TRUE(model.Paste());
  EXPECT_STR_EQ("FOO \t\t BAR", model.text());

  model.SelectAll(false);
  model.DeleteSelection();
  EXPECT_STR_EQ("", model.text());

  // Test 5
  // Clipboard text with multiple trailing tabs should be pasted with all
  // trailing tabs stripped.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::ASCIIToUTF16("FOO BAR\t\t\t"));
  EXPECT_TRUE(model.Paste());
  EXPECT_STR_EQ("FOO BAR", model.text());

  model.SelectAll(false);
  model.DeleteSelection();
  EXPECT_STR_EQ("", model.text());

  // Test 6
  // Clipboard text with only spaces and tabs should be pasted as a single
  // space.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::ASCIIToUTF16("     \t\t"));
  EXPECT_TRUE(model.Paste());
  EXPECT_STR_EQ(" ", model.text());

  model.SelectAll(false);
  model.DeleteSelection();
  EXPECT_STR_EQ("", model.text());

  // Test 7
  // Clipboard text with lots of spaces between words should be pasted as-is.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::ASCIIToUTF16("FOO      BAR"));
  EXPECT_TRUE(model.Paste());
  EXPECT_STR_EQ("FOO      BAR", model.text());
}

TEST_F(TextfieldModelTest, Transpose) {
  const base::string16 ltr = base::ASCIIToUTF16("12");
  const base::string16 rtl = base::WideToUTF16(L"\x0634\x0632");
  const base::string16 ltr_transposed = base::ASCIIToUTF16("21");
  const base::string16 rtl_transposed = base::WideToUTF16(L"\x0632\x0634");

  // This is a string with an 'a' between two emojis.
  const base::string16 surrogate_pairs({0xD83D, 0xDE07, 'a', 0xD83D, 0xDE0E});
  const base::string16 test_strings[] = {ltr, rtl, surrogate_pairs};

  struct TestCase {
    gfx::Range range;
    base::string16 expected_text;
    gfx::Range expected_selection;
  };

  std::vector<TestCase> ltr_tests = {
      {gfx::Range(0), ltr, gfx::Range(0)},
      {gfx::Range(1), ltr_transposed, gfx::Range(2)},
      {gfx::Range(2), ltr_transposed, gfx::Range(2)},
      {gfx::Range(0, 2), ltr, gfx::Range(0, 2)}};

  std::vector<TestCase> rtl_tests = {
      {gfx::Range(0), rtl, gfx::Range(0)},
      {gfx::Range(1), rtl_transposed, gfx::Range(2)},
      {gfx::Range(2), rtl_transposed, gfx::Range(2)},
      {gfx::Range(0, 1), rtl, gfx::Range(0, 1)}};

  // Only test at valid grapheme boundaries.
  std::vector<TestCase> surrogate_pairs_test = {
      {gfx::Range(0), surrogate_pairs, gfx::Range(0)},
      {gfx::Range(2), base::string16({'a', 0xD83D, 0xDE07, 0xD83D, 0xDE0E}),
       gfx::Range(3)},
      {gfx::Range(3), base::string16({0xD83D, 0xDE07, 0xD83D, 0xDE0E, 'a'}),
       gfx::Range(5)},
      {gfx::Range(5), base::string16({0xD83D, 0xDE07, 0xD83D, 0xDE0E, 'a'}),
       gfx::Range(5)},
      {gfx::Range(3, 5), surrogate_pairs, gfx::Range(3, 5)}};

  std::vector<std::vector<TestCase>> all_tests = {ltr_tests, rtl_tests,
                                                  surrogate_pairs_test};

  TextfieldModel model(nullptr);

  EXPECT_EQ(all_tests.size(), base::size(test_strings));

  for (size_t i = 0; i < base::size(test_strings); i++) {
    for (size_t j = 0; j < all_tests[i].size(); j++) {
      SCOPED_TRACE(testing::Message() << "Testing case " << i << ", " << j
                                      << " with string " << test_strings[i]);

      const TestCase& test_case = all_tests[i][j];

      model.SetText(test_strings[i], 0);
      model.SelectRange(test_case.range);
      EXPECT_EQ(test_case.range, model.render_text()->selection());
      model.Transpose();

      EXPECT_EQ(test_case.expected_text, model.text());
      EXPECT_EQ(test_case.expected_selection, model.render_text()->selection());
    }
  }
}

TEST_F(TextfieldModelTest, Yank) {
  TextfieldModel model(nullptr);
  model.SetText(base::ASCIIToUTF16("abcdefgh"), 0);
  model.SelectRange(gfx::Range(1, 3));

  // Delete selection but don't add to kill buffer.
  model.Delete(false);
  EXPECT_STR_EQ("adefgh", model.text());

  // Since the kill buffer is empty, yank should cause no change.
  EXPECT_FALSE(model.Yank());
  EXPECT_STR_EQ("adefgh", model.text());

  // With a nonempty selection and an empty kill buffer, yank should delete the
  // selection.
  model.SelectRange(gfx::Range(4, 5));
  EXPECT_TRUE(model.Yank());
  EXPECT_STR_EQ("adefh", model.text());

  // With multiple selections and an empty kill buffer, yank should delete the
  // selections.
  model.SelectRange(gfx::Range(2, 3));
  model.SelectRange(gfx::Range(4, 5), false);
  EXPECT_TRUE(model.Yank());
  EXPECT_STR_EQ("adf", model.text());

  // The kill buffer should remain empty after yanking without a kill buffer.
  EXPECT_FALSE(model.Yank());
  EXPECT_STR_EQ("adf", model.text());

  // Delete selection and add to kill buffer.
  model.SelectRange(gfx::Range(0, 1));
  model.Delete(true);
  EXPECT_STR_EQ("df", model.text());

  // Yank twice.
  EXPECT_TRUE(model.Yank());
  EXPECT_TRUE(model.Yank());
  EXPECT_STR_EQ("aadf", model.text());

  // Ensure an empty deletion does not modify the kill buffer.
  model.SelectRange(gfx::Range(4));
  model.Delete(true);
  EXPECT_TRUE(model.Yank());
  EXPECT_STR_EQ("aadfa", model.text());

  // Backspace twice but don't add to kill buffer.
  model.Backspace(false);
  model.Backspace(false);
  EXPECT_STR_EQ("aad", model.text());

  // Ensure kill buffer is not modified.
  EXPECT_TRUE(model.Yank());
  EXPECT_STR_EQ("aada", model.text());

  // Backspace twice, each time modifying the kill buffer.
  model.Backspace(true);
  model.Backspace(true);
  EXPECT_STR_EQ("aa", model.text());

  // Ensure yanking inserts the modified kill buffer text.
  EXPECT_TRUE(model.Yank());
  EXPECT_STR_EQ("aad", model.text());
}

TEST_F(TextfieldModelTest, SetCompositionFromExistingText) {
  TextfieldModel model(nullptr);
  model.SetText(base::ASCIIToUTF16("abcde"), 0);

  model.SetCompositionFromExistingText(gfx::Range(0, 1));
  EXPECT_TRUE(model.HasCompositionText());

  model.SetCompositionFromExistingText(gfx::Range(1, 3));
  EXPECT_TRUE(model.HasCompositionText());

  ui::CompositionText composition;
  composition.text = base::ASCIIToUTF16("123");
  model.SetCompositionText(composition);
  EXPECT_STR_EQ("a123de", model.text());
}

TEST_F(TextfieldModelTest, SetCompositionFromExistingText_Empty) {
  TextfieldModel model(nullptr);
  model.SetText(base::ASCIIToUTF16("abc"), 0);

  model.SetCompositionFromExistingText(gfx::Range(0, 2));
  EXPECT_TRUE(model.HasCompositionText());

  model.SetCompositionFromExistingText(gfx::Range(1, 1));
  EXPECT_FALSE(model.HasCompositionText());
  EXPECT_STR_EQ("abc", model.text());
}

TEST_F(TextfieldModelTest, SetCompositionFromExistingText_OutOfBounds) {
  TextfieldModel model(nullptr);
  model.SetText(base::string16(), 0);

  model.SetCompositionFromExistingText(gfx::Range(0, 2));
  EXPECT_FALSE(model.HasCompositionText());

  model.SetText(base::ASCIIToUTF16("abc"), 0);
  model.SetCompositionFromExistingText(gfx::Range(1, 4));
  EXPECT_FALSE(model.HasCompositionText());
}

}  // namespace views
