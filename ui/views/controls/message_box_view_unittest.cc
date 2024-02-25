// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/message_box_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/test/views_test_base.h"

namespace views {

namespace {
// The default mesage width same as defined in message_box_view.cc.
constexpr int kDefaultMessageWidth = 400;
const std::u16string kDefaultMessage =
    u"This is a test message for MessageBoxView.";
}  // namespace

class MessageBoxViewTest : public ViewsTestBase {
 public:
  MessageBoxViewTest() = default;
  MessageBoxViewTest(const MessageBoxViewTest&) = delete;
  MessageBoxViewTest& operator=(const MessageBoxViewTest&) = delete;
  ~MessageBoxViewTest() override = default;

 protected:
  void SetUp() override {
    ViewsTestBase::SetUp();
    message_box_ = std::make_unique<MessageBoxView>(kDefaultMessage);
  }

  std::unique_ptr<MessageBoxView> message_box_;
};

TEST_F(MessageBoxViewTest, CheckMessageOnlySize) {
  message_box_->SizeToPreferredSize();

  gfx::Insets box_border = LayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText);
  gfx::Size scroll_size = message_box_->scroll_view_->size();
  scroll_size.Enlarge(0, box_border.top() + box_border.bottom());
  EXPECT_EQ(scroll_size, message_box_->size());
}

TEST_F(MessageBoxViewTest, CheckWithOptionalViewsSize) {
  message_box_->SetPromptField(std::u16string());
  message_box_->SizeToPreferredSize();

  gfx::Insets box_border = LayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kControl);
  gfx::Size scroll_size = message_box_->scroll_view_->size();
  gfx::Size prompt_size = message_box_->prompt_field_->size();
  gfx::Size content_size(std::max(scroll_size.width(), prompt_size.width()),
                         scroll_size.height() + prompt_size.height());
  content_size.Enlarge(0, box_border.top() + box_border.bottom() +
                              message_box_->inter_row_vertical_spacing_);
  EXPECT_EQ(content_size, message_box_->size());

  // Add a checkbox and a link.
  message_box_->SetCheckBoxLabel(u"A checkbox");
  message_box_->SetLink(u"Link to display", base::DoNothing());
  message_box_->SizeToPreferredSize();

  box_border = LayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText);
  gfx::Size checkbox_size = message_box_->checkbox_->size();
  gfx::Size link_size = message_box_->link_->size();
  content_size =
      gfx::Size(std::max(std::max(scroll_size.width(), prompt_size.width()),
                         std::max(checkbox_size.width(), link_size.width())),
                scroll_size.height() + prompt_size.height() +
                    checkbox_size.height() + link_size.height());
  content_size.Enlarge(0, box_border.top() + box_border.bottom() +
                              3 * message_box_->inter_row_vertical_spacing_);
  EXPECT_EQ(content_size, message_box_->size());
}

TEST_F(MessageBoxViewTest, CheckMessageWidthChange) {
  message_box_->SizeToPreferredSize();
  EXPECT_EQ(kDefaultMessageWidth, message_box_->width());

  static constexpr int kNewWidth = 210;
  message_box_->SetMessageWidth(kNewWidth);
  message_box_->SizeToPreferredSize();
  EXPECT_EQ(kNewWidth, message_box_->width());
}

TEST_F(MessageBoxViewTest, CheckInterRowHeightChange) {
  message_box_->SetPromptField(std::u16string());
  message_box_->SizeToPreferredSize();

  int scroll_height = message_box_->scroll_view_->height();
  int prompt_height = message_box_->prompt_field_->height();
  gfx::Insets box_border = LayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kControl);
  int inter_row_spacing = message_box_->inter_row_vertical_spacing_;
  EXPECT_EQ(
      scroll_height + inter_row_spacing + prompt_height + box_border.height(),
      message_box_->height());

  static constexpr int kNewInterRowSpacing = 50;
  EXPECT_NE(kNewInterRowSpacing, inter_row_spacing);
  message_box_->SetInterRowVerticalSpacing(kNewInterRowSpacing);
  message_box_->SizeToPreferredSize();
  EXPECT_EQ(kNewInterRowSpacing, message_box_->inter_row_vertical_spacing_);
  EXPECT_EQ(
      scroll_height + kNewInterRowSpacing + prompt_height + box_border.height(),
      message_box_->height());
}

TEST_F(MessageBoxViewTest, CheckHasVisibleCheckBox) {
  EXPECT_FALSE(message_box_->HasVisibleCheckBox());

  // Set and show a checkbox.
  message_box_->SetCheckBoxLabel(u"test checkbox");
  EXPECT_TRUE(message_box_->HasVisibleCheckBox());
}

TEST_F(MessageBoxViewTest, CheckGetVisiblePromptField) {
  EXPECT_FALSE(message_box_->GetVisiblePromptField());

  // Set the prompt field.
  message_box_->SetPromptField(std::u16string());
  EXPECT_TRUE(message_box_->GetVisiblePromptField());
}

TEST_F(MessageBoxViewTest, CheckGetInputText) {
  EXPECT_TRUE(message_box_->GetInputText().empty());

  // Set the prompt field with an empty string. The returned text is still
  // empty.
  message_box_->SetPromptField(std::u16string());
  EXPECT_TRUE(message_box_->GetInputText().empty());

  const std::u16string prompt = u"prompt";
  message_box_->SetPromptField(prompt);
  EXPECT_FALSE(message_box_->GetInputText().empty());
  EXPECT_EQ(prompt, message_box_->GetInputText());

  // After user types some text, the returned input text should change to the
  // user input.
  views::Textfield* text_field = message_box_->GetVisiblePromptField();
  const std::u16string input = u"new input";
  text_field->SetText(input);
  EXPECT_FALSE(message_box_->GetInputText().empty());
  EXPECT_EQ(input, message_box_->GetInputText());
}

TEST_F(MessageBoxViewTest, CheckIsCheckBoxSelected) {
  EXPECT_FALSE(message_box_->IsCheckBoxSelected());

  // Set and show a checkbox.
  message_box_->SetCheckBoxLabel(u"test checkbox");
  EXPECT_FALSE(message_box_->IsCheckBoxSelected());

  // Select the checkbox.
  message_box_->SetCheckBoxSelected(true);
  EXPECT_TRUE(message_box_->IsCheckBoxSelected());
}

}  // namespace views
