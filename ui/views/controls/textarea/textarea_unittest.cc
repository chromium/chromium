// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textarea/textarea.h"

#include <memory>
#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/render_text_test_api.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/controls/textfield/textfield_unittest.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"

namespace {

const char16_t kHebrewLetterSamekh = 0x05E1;

}  // namespace

namespace views {
namespace {

class TextareaTest : public test::TextfieldTest {
 public:
  TextareaTest() = default;

  TextareaTest(const TextareaTest&) = delete;
  TextareaTest& operator=(const TextareaTest&) = delete;

  ~TextareaTest() override = default;

  // TextfieldTest:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{::features::kTouchTextEditingRedesign},
        /*disabled_features=*/{});

    TextfieldTest::SetUp();

    ASSERT_FALSE(textarea_);
    textarea_ = PrepareTextfields(0, std::make_unique<Textarea>(),
                                  gfx::Rect(100, 100, 800, 100));
  }

 protected:
  void RunMoveUpDownTest(int start_index,
                         ui::KeyboardCode key_code,
                         std::vector<int> expected) {
    DCHECK(key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN);
    textarea_->SetSelectedRange(gfx::Range(start_index));
    for (size_t i = 0; i < expected.size(); ++i) {
      SCOPED_TRACE(testing::Message()
                   << (key_code == ui::VKEY_UP ? "MOVE UP " : "MOVE DOWN ")
                   << i + 1 << " times from Range " << start_index);
      SendKeyEvent(key_code);
      EXPECT_EQ(gfx::Range(expected[i]), textarea_->GetSelectedRange());
    }
  }

  size_t GetCursorLine() {
    return GetTextfieldTestApi().GetRenderText()->GetLineContainingCaret(
        textarea_->GetSelectionModel());
  }

  // TextfieldTest:
  TextfieldTestApi GetTextfieldTestApi() override {
    return TextfieldTestApi(textarea_);
  }

  void SendHomeEvent(bool shift) override {
    SendKeyEvent(ui::VKEY_HOME, shift, TestingNativeMac());
  }

  void SendEndEvent(bool shift) override {
    SendKeyEvent(ui::VKEY_END, shift, TestingNativeMac());
  }

  raw_ptr<Textarea> textarea_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// Disabled when using XKB for crbug.com/1171828.
#if BUILDFLAG(USE_XKBCOMMON)
#define MAYBE_InsertNewlineTest DISABLED_InsertNewlineTest
#else
#define MAYBE_InsertNewlineTest InsertNewlineTest
#endif  // BUILDFLAG(USE_XKBCOMMON)
TEST_F(TextareaTest, MAYBE_InsertNewlineTest) {
  for (size_t i = 0; i < 5; i++) {
    SendKeyEvent(static_cast<ui::KeyboardCode>(ui::VKEY_A + i));
    SendKeyEvent(ui::VKEY_RETURN);
  }
  EXPECT_EQ(u"a\nb\nc\nd\ne\n", textarea_->GetText());
}

TEST_F(TextareaTest, PasteNewlineTest) {
  const std::u16string kText = u"abc\n   \n";
  textarea_->SetText(kText);
  textarea_->SelectAll(false);
  textarea_->ExecuteCommand(Textfield::kCopy, 0);
  textarea_->SetText(std::u16string());
  textarea_->ExecuteCommand(Textfield::kPaste, 0);
  EXPECT_EQ(kText, textarea_->GetText());
}

// Re-enable when crbug.com/1163587 is fixed.
TEST_F(TextareaTest, DISABLED_CursorMovement) {
  textarea_->SetText(u"one\n\ntwo three");

  // Move Up/Down at the front of the line.
  RunMoveUpDownTest(0, ui::VKEY_DOWN, {4, 5, 14});
  RunMoveUpDownTest(5, ui::VKEY_UP, {4, 0, 0});

  // Move Up/Down at the end of the line.
  RunMoveUpDownTest(3, ui::VKEY_DOWN, {4, 8, 14});
  RunMoveUpDownTest(14, ui::VKEY_UP, {4, 3, 0});

  // Move Up/Down at the middle position.
  RunMoveUpDownTest(2, ui::VKEY_DOWN, {4, 7, 14});
  RunMoveUpDownTest(7, ui::VKEY_UP, {4, 2, 0});

  // Test Home/End key on each lines.
  textarea_->SetSelectedRange(gfx::Range(2));  // First line.
  SendHomeEvent(false);
  EXPECT_EQ(gfx::Range(0), textarea_->GetSelectedRange());
  SendEndEvent(false);
  EXPECT_EQ(gfx::Range(3), textarea_->GetSelectedRange());
  textarea_->SetSelectedRange(gfx::Range(4));  // 2nd line.
  SendHomeEvent(false);
  EXPECT_EQ(gfx::Range(4), textarea_->GetSelectedRange());
  SendEndEvent(false);
  EXPECT_EQ(gfx::Range(4), textarea_->GetSelectedRange());
  textarea_->SetSelectedRange(gfx::Range(7));  // 3rd line.
  SendHomeEvent(false);
  EXPECT_EQ(gfx::Range(5), textarea_->GetSelectedRange());
  SendEndEvent(false);
  EXPECT_EQ(gfx::Range(14), textarea_->GetSelectedRange());
}

// Ensure cursor view is always inside display rect.
TEST_F(TextareaTest, CursorViewBounds) {
  textarea_->SetBounds(0, 0, 100, 31);
  for (size_t i = 0; i < 10; ++i) {
    SCOPED_TRACE(base::StringPrintf("VKEY_RETURN %" PRIuS " times", i + 1));
    SendKeyEvent(ui::VKEY_RETURN);
    ASSERT_TRUE(textarea_->GetVisibleBounds().Contains(GetCursorViewRect()));
    ASSERT_FALSE(GetCursorViewRect().size().IsEmpty());
  }

  for (size_t i = 0; i < 10; ++i) {
    SCOPED_TRACE(base::StringPrintf("VKEY_UP %" PRIuS " times", i + 1));
    SendKeyEvent(ui::VKEY_UP);
    ASSERT_TRUE(textarea_->GetVisibleBounds().Contains(GetCursorViewRect()));
    ASSERT_FALSE(GetCursorViewRect().size().IsEmpty());
  }
}

TEST_F(TextareaTest, LineSelection) {
  textarea_->SetText(u"12\n34567 89");

  // Place the cursor after "5".
  textarea_->SetEditableSelectionRange(gfx::Range(6));

  // Select line towards right.
  SendEndEvent(true);
  EXPECT_EQ(u"67 89", textarea_->GetSelectedText());

  // Select line towards left. On Mac, the existing selection should be extended
  // to cover the whole line.
  SendHomeEvent(true);

  if (Textarea::kLineSelectionBehavior == gfx::SELECTION_EXTEND)
    EXPECT_EQ(u"34567 89", textarea_->GetSelectedText());
  else
    EXPECT_EQ(u"345", textarea_->GetSelectedText());

  EXPECT_TRUE(textarea_->GetSelectedRange().is_reversed());

  // Select line towards right.
  SendEndEvent(true);

  if (Textarea::kLineSelectionBehavior == gfx::SELECTION_EXTEND)
    EXPECT_EQ(u"34567 89", textarea_->GetSelectedText());
  else
    EXPECT_EQ(u"67 89", textarea_->GetSelectedText());

  EXPECT_FALSE(textarea_->GetSelectedRange().is_reversed());
}

// Disabled on Mac for crbug.com/1171826.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MoveUpDownAndModifySelection DISABLED_MoveUpDownAndModifySelection
#else
#define MAYBE_MoveUpDownAndModifySelection MoveUpDownAndModifySelection
#endif  // BUILDFLAG(IS_MAC)
TEST_F(TextareaTest, MAYBE_MoveUpDownAndModifySelection) {
  textarea_->SetText(u"12\n34567 89");
  textarea_->SetEditableSelectionRange(gfx::Range(6));
  EXPECT_EQ(1U, GetCursorLine());

  // Up key should place the cursor after "2" not after newline to place the
  // cursor on the first line.
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(0U, GetCursorLine());
  EXPECT_EQ(gfx::Range(2), textarea_->GetSelectedRange());

  // Down key after Up key should select the same range as the previous one.
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(1U, GetCursorLine());
  EXPECT_EQ(gfx::Range(6), textarea_->GetSelectedRange());

  // Shift+Up should select the text to the upper line position including
  // the newline character.
  SendKeyEvent(ui::VKEY_UP, true /* shift */, false /* command */);
  EXPECT_EQ(gfx::Range(6, 2), textarea_->GetSelectedRange());

  // Shift+Down should collapse the selection.
  SendKeyEvent(ui::VKEY_DOWN, true /* shift */, false /* command */);
  EXPECT_EQ(gfx::Range(6), textarea_->GetSelectedRange());

  // Shift+Down again should select the text to the end of the last line.
  SendKeyEvent(ui::VKEY_DOWN, true /* shift */, false /* command */);
  EXPECT_EQ(gfx::Range(6, 11), textarea_->GetSelectedRange());
}

TEST_F(TextareaTest, MovePageUpDownAndModifySelection) {
  textarea_->SetText(u"12\n34567 89");
  textarea_->SetEditableSelectionRange(gfx::Range(6));

  EXPECT_TRUE(
      textarea_->IsTextEditCommandEnabled(ui::TextEditCommand::MOVE_PAGE_UP));
  EXPECT_TRUE(
      textarea_->IsTextEditCommandEnabled(ui::TextEditCommand::MOVE_PAGE_DOWN));
  EXPECT_TRUE(textarea_->IsTextEditCommandEnabled(
      ui::TextEditCommand::MOVE_PAGE_UP_AND_MODIFY_SELECTION));
  EXPECT_TRUE(textarea_->IsTextEditCommandEnabled(
      ui::TextEditCommand::MOVE_PAGE_DOWN_AND_MODIFY_SELECTION));

  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PAGE_UP);
  EXPECT_EQ(gfx::Range(0), textarea_->GetSelectedRange());

  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PAGE_DOWN);
  EXPECT_EQ(gfx::Range(11), textarea_->GetSelectedRange());

  textarea_->SetEditableSelectionRange(gfx::Range(6));
  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PAGE_UP_AND_MODIFY_SELECTION);
  EXPECT_EQ(gfx::Range(6, 0), textarea_->GetSelectedRange());

  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PAGE_DOWN_AND_MODIFY_SELECTION);

  if (Textarea::kLineSelectionBehavior == gfx::SELECTION_EXTEND)
    EXPECT_EQ(gfx::Range(0, 11), textarea_->GetSelectedRange());
  else
    EXPECT_EQ(gfx::Range(6, 11), textarea_->GetSelectedRange());
}

// Ensure the textarea breaks the long word and scrolls on overflow.
TEST_F(TextareaTest, OverflowTest) {
  const size_t count = 50U;
  textarea_->SetBounds(0, 0, 60, 40);

  textarea_->SetText(std::u16string(count, 'a'));
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  textarea_->SetText(std::u16string(count, kHebrewLetterSamekh));
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));
}

TEST_F(TextareaTest, OverflowInRTLTest) {
  const size_t count = 50U;
  textarea_->SetBounds(0, 0, 60, 40);
  std::string locale = base::i18n::GetConfiguredLocale();
  base::i18n::SetICUDefaultLocale("he");

  textarea_->SetText(std::u16string(count, 'a'));
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  textarea_->SetText(std::u16string(count, kHebrewLetterSamekh));
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(TextareaTest, OnBlurTest) {
  const std::string& kText = "abcdef";
  textarea_->SetText(base::ASCIIToUTF16(kText));

  SendEndEvent(false);
  EXPECT_EQ(kText.size(), textarea_->GetCursorPosition());

  // A focus loss should not change the cursor position.
  textarea_->OnBlur();
  EXPECT_EQ(kText.size(), textarea_->GetCursorPosition());
}

TEST_F(TextareaTest, MoveRangeSelectionExtentExpandByWord) {
  constexpr int kGlyphHeight = 10;
  gfx::test::RenderTextTestApi(GetTextfieldTestApi().GetRenderText())
      .SetGlyphHeight(kGlyphHeight);
  textarea_->SetText(u"a textarea\nwith multiline text");
  const int kFirstLineMiddleY = GetCursorYForTesting() + kGlyphHeight / 2;
  const int kSecondLineMiddleY = kFirstLineMiddleY + kGlyphHeight;
  textarea_->SelectBetweenCoordinates(
      gfx::Point(GetCursorPositionX(2), kFirstLineMiddleY),
      gfx::Point(GetCursorPositionX(3), kFirstLineMiddleY));

  // Expand the selection. The end of the selection should move to the nearest
  // word boundary.
  textarea_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(22), kSecondLineMiddleY));
  gfx::Range range;
  textarea_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 25));
  EXPECT_EQ(textarea_->GetSelectedText(), u"textarea\nwith multiline");

  // Shrink then expand the selection again.
  textarea_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(7), kFirstLineMiddleY));
  textarea_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(29), kSecondLineMiddleY));
  textarea_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 30));
  EXPECT_EQ(textarea_->GetSelectedText(), u"textarea\nwith multiline text");
}

TEST_F(TextareaTest, MoveRangeSelectionExtentShrinkByCharacter) {
  constexpr int kGlyphHeight = 10;
  gfx::test::RenderTextTestApi(GetTextfieldTestApi().GetRenderText())
      .SetGlyphHeight(kGlyphHeight);
  textarea_->SetText(u"a textarea\nwith multiline text");
  const int kFirstLineMiddleY = GetCursorYForTesting() + kGlyphHeight / 2;
  const int kSecondLineMiddleY = kFirstLineMiddleY + kGlyphHeight;
  textarea_->SelectBetweenCoordinates(
      gfx::Point(GetCursorPositionX(2), kFirstLineMiddleY),
      gfx::Point(GetCursorPositionX(25), kSecondLineMiddleY));

  // Shrink the selection.
  textarea_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(6), kFirstLineMiddleY));
  gfx::Range range;
  textarea_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 6));
  EXPECT_EQ(textarea_->GetSelectedText(), u"text");
}

}  // namespace views
