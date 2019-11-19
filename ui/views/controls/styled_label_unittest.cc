// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/styled_label.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/font_list.h"
#include "ui/views/border.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/style/typography.h"
#include "ui/views/test/test_layout_provider.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;

namespace views {

class StyledLabelTest : public ViewsTestBase, public StyledLabelListener {
 public:
  StyledLabelTest() = default;
  ~StyledLabelTest() override = default;

  // StyledLabelListener implementation.
  void StyledLabelLinkClicked(StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override {}

 protected:
  StyledLabel* styled() { return styled_.get(); }

  Label* LabelAt(size_t index,
                 std::string expected_classname = Label::kViewClassName) {
    View* const child = styled_->children()[index];
    EXPECT_EQ(expected_classname, child->GetClassName());
    return static_cast<Label*>(child);
  }

  void InitStyledLabel(const std::string& ascii_text) {
    styled_ = std::make_unique<StyledLabel>(ASCIIToUTF16(ascii_text), this);
    styled_->set_owned_by_client();
  }

  int StyledLabelContentHeightForWidth(int w) {
    return styled_->GetHeightForWidth(w) - styled_->GetInsets().height();
  }

 private:
  std::unique_ptr<StyledLabel> styled_;

  DISALLOW_COPY_AND_ASSIGN(StyledLabelTest);
};

TEST_F(StyledLabelTest, NoWrapping) {
  const std::string text("This is a test block of text");
  InitStyledLabel(text);
  Label label(ASCIIToUTF16(text));
  const gfx::Size label_preferred_size = label.GetPreferredSize();
  EXPECT_EQ(label_preferred_size.height(),
            StyledLabelContentHeightForWidth(label_preferred_size.width() * 2));
}

TEST_F(StyledLabelTest, TrailingWhitespaceiIgnored) {
  const std::string text("This is a test block of text   ");
  InitStyledLabel(text);

  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();

  ASSERT_EQ(1u, styled()->children().size());
  EXPECT_EQ(ASCIIToUTF16("This is a test block of text"),
            LabelAt(0)->GetText());
}

TEST_F(StyledLabelTest, RespectLeadingWhitespace) {
  const std::string text("   This is a test block of text");
  InitStyledLabel(text);

  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();

  ASSERT_EQ(1u, styled()->children().size());
  EXPECT_EQ(ASCIIToUTF16("   This is a test block of text"),
            LabelAt(0)->GetText());
}

TEST_F(StyledLabelTest, RespectLeadingSpacesInNonFirstLine) {
  const std::string indented_line = "  indented line";
  const std::string text(std::string("First line\n") + indented_line);
  InitStyledLabel(text);
  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();
  ASSERT_EQ(2u, styled()->children().size());
  EXPECT_EQ(ASCIIToUTF16(indented_line), LabelAt(1)->GetText());
}

TEST_F(StyledLabelTest, CorrectWrapAtNewline) {
  const std::string first_line = "Line one";
  const std::string second_line = "  two";
  const std::string multiline_text(first_line + "\n" + second_line);
  InitStyledLabel(multiline_text);
  Label label(ASCIIToUTF16(first_line));
  gfx::Size label_preferred_size = label.GetPreferredSize();
  // Correct handling of \n and label width limit encountered at the same place
  styled()->SetBounds(0, 0, label_preferred_size.width(), 1000);
  styled()->Layout();
  ASSERT_EQ(2u, styled()->children().size());
  EXPECT_EQ(ASCIIToUTF16(first_line), LabelAt(0)->GetText());
  const auto* label_1 = LabelAt(1);
  EXPECT_EQ(ASCIIToUTF16(second_line), label_1->GetText());
  EXPECT_EQ(styled()->GetHeightForWidth(1000), label_1->bounds().bottom());
}

TEST_F(StyledLabelTest, FirstLineNotEmptyWhenLeadingWhitespaceTooLong) {
  const std::string text("                                     a");
  InitStyledLabel(text);

  Label label(ASCIIToUTF16(text));
  gfx::Size label_preferred_size = label.GetPreferredSize();

  styled()->SetBounds(0, 0, label_preferred_size.width() / 2, 1000);
  styled()->Layout();

  ASSERT_EQ(1u, styled()->children().size());
  EXPECT_EQ(ASCIIToUTF16("a"), LabelAt(0)->GetText());
  EXPECT_EQ(label_preferred_size.height(),
            styled()->GetHeightForWidth(label_preferred_size.width() / 2));
}

TEST_F(StyledLabelTest, BasicWrapping) {
  const std::string text("This is a test block of text");
  InitStyledLabel(text);
  Label label(ASCIIToUTF16(text.substr(0, text.size() * 2 / 3)));
  gfx::Size label_preferred_size = label.GetPreferredSize();
  EXPECT_EQ(label_preferred_size.height() * 2,
            StyledLabelContentHeightForWidth(label_preferred_size.width()));

  // Also respect the border.
  styled()->SetBorder(CreateEmptyBorder(3, 3, 3, 3));
  styled()->SetBounds(
      0,
      0,
      styled()->GetInsets().width() + label_preferred_size.width(),
      styled()->GetInsets().height() + 2 * label_preferred_size.height());
  styled()->Layout();
  ASSERT_EQ(2u, styled()->children().size());
  EXPECT_EQ(3, styled()->children()[0]->x());
  EXPECT_EQ(3, styled()->children()[0]->y());
  EXPECT_EQ(styled()->height() - 3, styled()->children()[1]->bounds().bottom());
}

TEST_F(StyledLabelTest, AllowEmptyLines) {
  const std::string text("one");
  InitStyledLabel(text);
  int default_height = styled()->GetHeightForWidth(1000);
  const std::string multiline_text("one\n\nthree");
  InitStyledLabel(multiline_text);
  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();
  EXPECT_EQ(3 * default_height, styled()->GetHeightForWidth(1000));
  ASSERT_EQ(2u, styled()->children().size());
  EXPECT_EQ(styled()->GetHeightForWidth(1000),
            styled()->children()[1]->bounds().bottom());
}

TEST_F(StyledLabelTest, WrapLongWords) {
  const std::string text("ThisIsTextAsASingleWord");
  InitStyledLabel(text);
  Label label(ASCIIToUTF16(text.substr(0, text.size() * 2 / 3)));
  gfx::Size label_preferred_size = label.GetPreferredSize();
  EXPECT_EQ(label_preferred_size.height() * 2,
            StyledLabelContentHeightForWidth(label_preferred_size.width()));

  styled()->SetBounds(
      0, 0, styled()->GetInsets().width() + label_preferred_size.width(),
      styled()->GetInsets().height() + 2 * label_preferred_size.height());
  styled()->Layout();

  ASSERT_EQ(2u, styled()->children().size());
  ASSERT_EQ(gfx::Point(), styled()->origin());
  const auto* label_0 = LabelAt(0);
  const auto* label_1 = LabelAt(1);
  EXPECT_EQ(gfx::Point(), label_0->origin());
  EXPECT_EQ(gfx::Point(0, styled()->height() / 2), label_1->origin());

  EXPECT_FALSE(label_0->GetText().empty());
  EXPECT_FALSE(label_1->GetText().empty());
  EXPECT_EQ(ASCIIToUTF16(text), label_0->GetText() + label_1->GetText());
}

TEST_F(StyledLabelTest, CreateLinks) {
  const std::string text("This is a test block of text.");
  InitStyledLabel(text);

  // Without links, there should be no focus border.
  EXPECT_TRUE(styled()->GetInsets().IsEmpty());

  // Now let's add some links.
  styled()->AddStyleRange(gfx::Range(0, 1),
                          StyledLabel::RangeStyleInfo::CreateForLink());
  styled()->AddStyleRange(gfx::Range(1, 2),
                          StyledLabel::RangeStyleInfo::CreateForLink());
  styled()->AddStyleRange(gfx::Range(10, 11),
                          StyledLabel::RangeStyleInfo::CreateForLink());
  styled()->AddStyleRange(gfx::Range(12, 13),
                          StyledLabel::RangeStyleInfo::CreateForLink());

  // Insets shouldn't change when links are added, since the links indicate
  // focus by adding an underline instead.
  EXPECT_TRUE(styled()->GetInsets().IsEmpty());

  // Verify layout creates the right number of children.
  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();
  EXPECT_EQ(7u, styled()->children().size());
}

TEST_F(StyledLabelTest, DontBreakLinks) {
  const std::string text("This is a test block of text, ");
  const std::string link_text("and this should be a link");
  InitStyledLabel(text + link_text);
  styled()->AddStyleRange(
      gfx::Range(text.size(), text.size() + link_text.size()),
      StyledLabel::RangeStyleInfo::CreateForLink());

  Label label(ASCIIToUTF16(text + link_text.substr(0, link_text.size() / 2)));
  gfx::Size label_preferred_size = label.GetPreferredSize();
  int pref_height = styled()->GetHeightForWidth(label_preferred_size.width());
  EXPECT_EQ(label_preferred_size.height() * 2,
            pref_height - styled()->GetInsets().height());

  styled()->SetBounds(0, 0, label_preferred_size.width(), pref_height);
  styled()->Layout();
  ASSERT_EQ(2u, styled()->children().size());

  // No additional insets should be added.
  EXPECT_EQ(0, styled()->children()[0]->x());
  // The Link shouldn't be offset.
  EXPECT_EQ(0, styled()->children()[1]->x());
}

TEST_F(StyledLabelTest, StyledRangeWithDisabledLineWrapping) {
  const std::string text("This is a test block of text, ");
  const std::string unbreakable_text("and this should not be broken");
  InitStyledLabel(text + unbreakable_text);
  StyledLabel::RangeStyleInfo style_info;
  style_info.disable_line_wrapping = true;
  styled()->AddStyleRange(
      gfx::Range(text.size(), text.size() + unbreakable_text.size()),
      style_info);

  Label label(ASCIIToUTF16(
      text + unbreakable_text.substr(0, unbreakable_text.size() / 2)));
  gfx::Size label_preferred_size = label.GetPreferredSize();
  int pref_height = styled()->GetHeightForWidth(label_preferred_size.width());
  EXPECT_EQ(label_preferred_size.height() * 2,
            pref_height - styled()->GetInsets().height());

  styled()->SetBounds(0, 0, label_preferred_size.width(), pref_height);
  styled()->Layout();
  ASSERT_EQ(2u, styled()->children().size());
  EXPECT_EQ(0, styled()->children()[0]->x());
  EXPECT_EQ(0, styled()->children()[1]->x());
}

TEST_F(StyledLabelTest, StyledRangeCustomFontUnderlined) {
  const std::string text("This is a test block of text, ");
  const std::string underlined_text("and this should be undelined");
  InitStyledLabel(text + underlined_text);
  StyledLabel::RangeStyleInfo style_info;
  style_info.tooltip = ASCIIToUTF16("tooltip");
  style_info.custom_font =
      styled()->GetDefaultFontList().DeriveWithStyle(gfx::Font::UNDERLINE);
  styled()->AddStyleRange(
      gfx::Range(text.size(), text.size() + underlined_text.size()),
      style_info);

  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();

  ASSERT_EQ(2u, styled()->children().size());
  EXPECT_EQ(gfx::Font::UNDERLINE, LabelAt(1)->font_list().GetFontStyle());
}

TEST_F(StyledLabelTest, StyledRangeTextStyleBold) {
  test::TestLayoutProvider bold_provider;
  const std::string bold_text(
      "This is a block of text whose style will be set to BOLD in the test");
  const std::string text(" normal text");
  InitStyledLabel(bold_text + text);

  // Pretend disabled text becomes bold for testing.
  bold_provider.SetFont(
      style::CONTEXT_LABEL, style::STYLE_DISABLED,
      styled()->GetDefaultFontList().DeriveWithWeight(gfx::Font::Weight::BOLD));

  StyledLabel::RangeStyleInfo style_info;
  style_info.text_style = style::STYLE_DISABLED;
  styled()->AddStyleRange(gfx::Range(0u, bold_text.size()), style_info);

  // Calculate the bold text width if it were a pure label view, both with bold
  // and normal style.
  Label label(ASCIIToUTF16(bold_text));
  const gfx::Size normal_label_size = label.GetPreferredSize();
  label.SetFontList(
      label.font_list().DeriveWithWeight(gfx::Font::Weight::BOLD));
  const gfx::Size bold_label_size = label.GetPreferredSize();

  ASSERT_GE(bold_label_size.width(), normal_label_size.width());

  // Set the width so |bold_text| doesn't fit on a single line with bold style,
  // but does with normal font style.
  int styled_width = (normal_label_size.width() + bold_label_size.width()) / 2;
  int pref_height = styled()->GetHeightForWidth(styled_width);

  // Sanity check that |bold_text| with normal font style would fit on a single
  // line in a styled label with width |styled_width|.
  StyledLabel unstyled(ASCIIToUTF16(bold_text), this);
  unstyled.SetBounds(0, 0, styled_width, pref_height);
  unstyled.Layout();
  EXPECT_EQ(1u, unstyled.children().size());

  styled()->SetBounds(0, 0, styled_width, pref_height);
  styled()->Layout();

  ASSERT_EQ(3u, styled()->children().size());

  // The bold text should be broken up into two parts.
  const auto* label_0 = LabelAt(0);
  const auto* label_1 = LabelAt(1);
  const auto* label_2 = LabelAt(2);
  EXPECT_EQ(gfx::Font::Weight::BOLD, label_0->font_list().GetFontWeight());
  EXPECT_EQ(gfx::Font::Weight::BOLD, label_1->font_list().GetFontWeight());
  EXPECT_EQ(gfx::Font::NORMAL, label_2->font_list().GetFontStyle());

  // The second bold part should start on a new line.
  EXPECT_EQ(0, label_0->x());
  EXPECT_EQ(0, label_1->x());
  EXPECT_EQ(label_1->bounds().right(), label_2->x());
}

TEST_F(StyledLabelTest, Color) {
  const std::string text_blue("BLUE");
  const std::string text_link("link");
  const std::string text("word");
  InitStyledLabel(text_blue + text_link + text);

  StyledLabel::RangeStyleInfo style_info_blue;
  style_info_blue.override_color = SK_ColorBLUE;
  styled()->AddStyleRange(gfx::Range(0u, text_blue.size()), style_info_blue);

  StyledLabel::RangeStyleInfo style_info_link =
      StyledLabel::RangeStyleInfo::CreateForLink();
  styled()->AddStyleRange(
      gfx::Range(text_blue.size(), text_blue.size() + text_link.size()),
      style_info_link);

  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();

  Widget* widget = new Widget();
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(params));
  View* container = new View();
  widget->SetContentsView(container);

  // The code below is not prepared to deal with dark mode.
  widget->GetNativeTheme()->set_use_dark_colors(false);

  container->AddChildView(styled());

  // Obtain the default text color for a label.
  Label* label = new Label(ASCIIToUTF16(text));
  container->AddChildView(label);
  const SkColor kDefaultTextColor = label->GetEnabledColor();

  // Obtain the default text color for a link.
  Link* link = new Link(ASCIIToUTF16(text_link));
  container->AddChildView(link);
  const SkColor kDefaultLinkColor = link->GetEnabledColor();

  EXPECT_EQ(SK_ColorBLUE, LabelAt(0)->GetEnabledColor());
  EXPECT_EQ(kDefaultLinkColor,
            LabelAt(1, Link::kViewClassName)->GetEnabledColor());
  EXPECT_EQ(kDefaultTextColor, LabelAt(2)->GetEnabledColor());

  // Test adjusted color readability.
  styled()->SetDisplayedOnBackgroundColor(SK_ColorBLACK);
  styled()->Layout();
  label->SetBackgroundColor(SK_ColorBLACK);

  const SkColor kAdjustedTextColor = label->GetEnabledColor();
  EXPECT_NE(kAdjustedTextColor, kDefaultTextColor);
  EXPECT_EQ(kAdjustedTextColor, LabelAt(2)->GetEnabledColor());

  widget->CloseNow();
}

TEST_F(StyledLabelTest, StyledRangeWithTooltip) {
  const std::string text("This is a test block of text, ");
  const std::string tooltip_text("this should have a tooltip,");
  const std::string normal_text(" this should not have a tooltip, ");
  const std::string link_text("and this should be a link");

  const size_t tooltip_start = text.size();
  const size_t link_start =
      text.size() + tooltip_text.size() + normal_text.size();

  InitStyledLabel(text + tooltip_text + normal_text + link_text);
  StyledLabel::RangeStyleInfo tooltip_style;
  tooltip_style.tooltip = ASCIIToUTF16("tooltip");
  styled()->AddStyleRange(
      gfx::Range(tooltip_start, tooltip_start + tooltip_text.size()),
      tooltip_style);
  styled()->AddStyleRange(gfx::Range(link_start, link_start + link_text.size()),
                          StyledLabel::RangeStyleInfo::CreateForLink());

  // Break line inside the range with the tooltip.
  Label label(ASCIIToUTF16(
       text + tooltip_text.substr(0, tooltip_text.size() - 3)));
  gfx::Size label_preferred_size = label.GetPreferredSize();
  int pref_height = styled()->GetHeightForWidth(label_preferred_size.width());
  EXPECT_EQ(label_preferred_size.height() * 3,
            pref_height - styled()->GetInsets().height());

  styled()->SetBounds(0, 0, label_preferred_size.width(), pref_height);
  styled()->Layout();

  EXPECT_EQ(label_preferred_size.width(), styled()->width());

  ASSERT_EQ(5u, styled()->children().size());

  // The labels shouldn't be offset to cater for focus rings.
  EXPECT_EQ(0, styled()->children()[0]->x());
  EXPECT_EQ(0, styled()->children()[2]->x());

  EXPECT_EQ(styled()->children()[0]->bounds().right(),
            styled()->children()[1]->x());
  EXPECT_EQ(styled()->children()[2]->bounds().right(),
            styled()->children()[3]->x());
  EXPECT_EQ(0, styled()->children()[4]->x());

  base::string16 tooltip =
      styled()->children()[1]->GetTooltipText(gfx::Point(1, 1));
  EXPECT_EQ(ASCIIToUTF16("tooltip"), tooltip);
  tooltip = styled()->children()[2]->GetTooltipText(gfx::Point(1, 1));
  EXPECT_EQ(ASCIIToUTF16("tooltip"), tooltip);
}

TEST_F(StyledLabelTest, SetTextContextAndDefaultStyle) {
  const std::string text("This is a test block of text.");
  InitStyledLabel(text);
  styled()->SetTextContext(style::CONTEXT_DIALOG_TITLE);
  styled()->SetDefaultTextStyle(style::STYLE_DISABLED);
  Label label(ASCIIToUTF16(text), style::CONTEXT_DIALOG_TITLE,
              style::STYLE_DISABLED);

  styled()->SetBounds(0,
                      0,
                      label.GetPreferredSize().width(),
                      label.GetPreferredSize().height());

  // Make sure we have the same sizing as a label with the same style.
  EXPECT_EQ(label.GetPreferredSize().height(), styled()->height());
  EXPECT_EQ(label.GetPreferredSize().width(), styled()->width());

  styled()->Layout();
  ASSERT_EQ(1u, styled()->children().size());
  Label* sublabel = LabelAt(0);
  EXPECT_EQ(style::CONTEXT_DIALOG_TITLE, sublabel->GetTextContext());

  EXPECT_NE(SK_ColorBLACK, label.GetEnabledColor());  // Sanity check,
  EXPECT_EQ(label.GetEnabledColor(), sublabel->GetEnabledColor());
}

TEST_F(StyledLabelTest, LineHeight) {
  const std::string text("one\ntwo\nthree");
  InitStyledLabel(text);
  styled()->SetLineHeight(18);
  EXPECT_EQ(18 * 3, styled()->GetHeightForWidth(100));
}

TEST_F(StyledLabelTest, LineHeightWithBorder) {
  const std::string text("one\ntwo\nthree");
  InitStyledLabel(text);
  styled()->SetLineHeight(18);
  styled()->SetBorder(views::CreateSolidBorder(1, SK_ColorGRAY));
  EXPECT_EQ(18 * 3 + 2, styled()->GetHeightForWidth(100));
}

TEST_F(StyledLabelTest, LineHeightWithLink) {
  const std::string text("one\ntwo\nthree");
  InitStyledLabel(text);
  styled()->SetLineHeight(18);

  styled()->AddStyleRange(gfx::Range(0, 3),
                          StyledLabel::RangeStyleInfo::CreateForLink());
  styled()->AddStyleRange(gfx::Range(4, 7),
                          StyledLabel::RangeStyleInfo::CreateForLink());
  styled()->AddStyleRange(gfx::Range(8, 13),
                          StyledLabel::RangeStyleInfo::CreateForLink());
  EXPECT_EQ(18 * 3, styled()->GetHeightForWidth(100));
}

TEST_F(StyledLabelTest, HandleEmptyLayout) {
  const std::string text("This is a test block of text.");
  InitStyledLabel(text);
  styled()->Layout();
  EXPECT_EQ(0u, styled()->children().size());
}

TEST_F(StyledLabelTest, CacheSize) {
  const int preferred_height = 50;
  const int preferred_width = 100;
  const std::string text("This is a test block of text.");
  const base::string16 another_text(base::ASCIIToUTF16(
      "This is a test block of text. This text is much longer than previous"));

  InitStyledLabel(text);

  // we should be able to calculate height without any problem
  // no controls should be created
  int precalculated_height = styled()->GetHeightForWidth(preferred_width);
  EXPECT_LT(0, precalculated_height);
  EXPECT_EQ(0u, styled()->children().size());

  styled()->SetBounds(0, 0, preferred_width, preferred_height);
  styled()->Layout();

  // controls should be created after layout
  // height should be the same as precalculated
  int real_height = styled()->GetHeightForWidth(styled()->width());
  View* first_child_after_layout =
      styled()->children().empty() ? nullptr : styled()->children().front();
  EXPECT_LT(0u, styled()->children().size());
  EXPECT_LT(0, real_height);
  EXPECT_EQ(real_height, precalculated_height);

  // another call to Layout should not kill and recreate all controls
  styled()->Layout();
  View* first_child_after_second_layout =
      styled()->children().empty() ? nullptr : styled()->children().front();
  EXPECT_EQ(first_child_after_layout, first_child_after_second_layout);

  // if text is changed:
  // layout should be recalculated
  // all controls should be recreated
  styled()->SetText(another_text);
  int updated_height = styled()->GetHeightForWidth(styled()->width());
  EXPECT_NE(updated_height, real_height);
  View* first_child_after_text_update =
      styled()->children().empty() ? nullptr : styled()->children().front();
  EXPECT_NE(first_child_after_text_update, first_child_after_layout);
}

TEST_F(StyledLabelTest, Border) {
  const std::string text("One line");
  InitStyledLabel(text);
  Label label(ASCIIToUTF16(text));
  gfx::Size label_preferred_size = label.GetPreferredSize();
  styled()->SetBorder(
      CreateEmptyBorder(5 /*top*/, 10 /*left*/, 6 /*bottom*/, 20 /*right*/));
  styled()->SetBounds(0, 0, 1000, 0);
  styled()->Layout();
  EXPECT_EQ(
      label_preferred_size.height() + 5 /*top border*/ + 6 /*bottom border*/,
      styled()->GetPreferredSize().height());
  EXPECT_EQ(
      label_preferred_size.width() + 10 /*left border*/ + 20 /*right border*/,
      styled()->GetPreferredSize().width());
}

TEST_F(StyledLabelTest, LineHeightWithShorterCustomView) {
  const std::string text("one ");
  InitStyledLabel(text);
  int default_height = styled()->GetHeightForWidth(1000);

  const std::string custom_view_text("with custom view");
  const int less_height = 10;
  std::unique_ptr<View> custom_view = std::make_unique<StaticSizedView>(
      gfx::Size(20, default_height - less_height));
  custom_view->set_owned_by_client();
  StyledLabel::RangeStyleInfo style_info;
  style_info.custom_view = custom_view.get();
  InitStyledLabel(text + custom_view_text);
  styled()->AddStyleRange(
      gfx::Range(text.size(), text.size() + custom_view_text.size()),
      style_info);
  styled()->AddCustomView(std::move(custom_view));
  EXPECT_EQ(default_height, styled()->GetHeightForWidth(100));
}

TEST_F(StyledLabelTest, LineHeightWithTallerCustomView) {
  const std::string text("one ");
  InitStyledLabel(text);
  int default_height = styled()->GetHeightForWidth(100);

  const std::string custom_view_text("with custom view");
  const int more_height = 10;
  std::unique_ptr<View> custom_view = std::make_unique<StaticSizedView>(
      gfx::Size(20, default_height + more_height));
  custom_view->set_owned_by_client();
  StyledLabel::RangeStyleInfo style_info;
  style_info.custom_view = custom_view.get();
  InitStyledLabel(text + custom_view_text);
  styled()->AddStyleRange(
      gfx::Range(text.size(), text.size() + custom_view_text.size()),
      style_info);
  styled()->AddCustomView(std::move(custom_view));
  EXPECT_EQ(default_height + more_height, styled()->GetHeightForWidth(100));
}

TEST_F(StyledLabelTest, LineWrapperWithCustomView) {
  const std::string text_before("one ");
  InitStyledLabel(text_before);
  int default_height = styled()->GetHeightForWidth(100);
  const std::string custom_view_text("two with custom view ");
  const std::string text_after("three");

  int custom_view_height = 25;
  std::unique_ptr<View> custom_view =
      std::make_unique<StaticSizedView>(gfx::Size(200, custom_view_height));
  custom_view->set_owned_by_client();
  StyledLabel::RangeStyleInfo style_info;
  style_info.custom_view = custom_view.get();
  InitStyledLabel(text_before + custom_view_text + text_after);
  styled()->AddStyleRange(
      gfx::Range(text_before.size(),
                 text_before.size() + custom_view_text.size()),
      style_info);
  styled()->AddCustomView(std::move(custom_view));
  EXPECT_EQ(default_height * 2 + custom_view_height,
            styled()->GetHeightForWidth(100));
}

TEST_F(StyledLabelTest, AlignmentInLTR) {
  const std::string text("text");
  InitStyledLabel(text);
  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();
  const auto& children = styled()->children();
  ASSERT_EQ(1u, children.size());

  // Test the default alignment puts the text on the leading side (left).
  EXPECT_EQ(0, children.front()->bounds().x());

  styled()->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  styled()->Layout();
  EXPECT_EQ(1000, children.front()->bounds().right());

  styled()->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  styled()->Layout();
  EXPECT_EQ(0, children.front()->bounds().x());

  styled()->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  styled()->Layout();
  Label label(ASCIIToUTF16(text));
  EXPECT_EQ((1000 - label.GetPreferredSize().width()) / 2,
            children.front()->bounds().x());
}

TEST_F(StyledLabelTest, AlignmentInRTL) {
  // |g_icu_text_direction| is cached to prevent reading new commandline switch.
  // Set |g_icu_text_direction| to |UNKNOWN_DIRECTION| in order to read the new
  // commandline switch.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale("en_US");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceUIDirection, switches::kForceDirectionRTL);

  const std::string text("text");
  InitStyledLabel(text);
  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();
  const auto& children = styled()->children();
  ASSERT_EQ(1u, children.size());

  // Test the default alignment puts the text on the leading side (right).
  // Note that x-coordinates in RTL place the origin (0) on the right.
  EXPECT_EQ(0, children.front()->bounds().x());

  // Setting |ALIGN_LEFT| should be flipped to |ALIGN_RIGHT|.
  styled()->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  styled()->Layout();
  EXPECT_EQ(1000, children.front()->bounds().right());

  // Setting |ALIGN_RIGHT| should be flipped to |ALIGN_LEFT|.
  styled()->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  styled()->Layout();
  EXPECT_EQ(0, children.front()->bounds().x());

  styled()->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  styled()->Layout();
  Label label(ASCIIToUTF16(text));
  EXPECT_EQ((1000 - label.GetPreferredSize().width()) / 2,
            children.front()->bounds().x());
}

TEST_F(StyledLabelTest, ViewsCenteredWithLinkAndCustomView) {
  const std::string text("This is a test block of text, ");
  const std::string link_text("and this should be a link");
  const std::string custom_view_text("And this is a custom view");
  InitStyledLabel(text + link_text + custom_view_text);
  styled()->AddStyleRange(
      gfx::Range(text.size(), text.size() + link_text.size()),
      StyledLabel::RangeStyleInfo::CreateForLink());

  int custom_view_height = 25;
  std::unique_ptr<View> custom_view =
      std::make_unique<StaticSizedView>(gfx::Size(20, custom_view_height));
  custom_view->set_owned_by_client();
  StyledLabel::RangeStyleInfo style_info;
  style_info.custom_view = custom_view.get();
  styled()->AddStyleRange(
      gfx::Range(text.size() + link_text.size(),
                 text.size() + link_text.size() + custom_view_text.size()),
      style_info);
  styled()->AddCustomView(std::move(custom_view));

  styled()->SetBounds(0, 0, 1000, 500);
  styled()->Layout();
  const int height = styled()->GetPreferredSize().height();
  for (const auto* child : styled()->children())
    EXPECT_EQ(height / 2, child->bounds().CenterPoint().y());
}

TEST_F(StyledLabelTest, ViewsCenteredForEvenAndOddSizes) {
  constexpr int kViewWidth = 30;
  for (int height : {60, 61}) {
    InitStyledLabel("abc");

    const int view_heights[] = {height, height / 2, height / 2 + 1};
    for (uint32_t i = 0; i < 3; ++i) {
      auto view = std::make_unique<StaticSizedView>(
          gfx::Size(kViewWidth, view_heights[i]));
      view->set_owned_by_client();
      StyledLabel::RangeStyleInfo style_info;
      style_info.custom_view = view.get();
      styled()->AddStyleRange(gfx::Range(i, i + 1), style_info);
      styled()->AddCustomView(std::move(view));
    }

    styled()->SetBounds(0, 0, kViewWidth * 3, height);
    styled()->Layout();

    for (const auto* child : styled()->children())
      EXPECT_EQ(height / 2, child->bounds().CenterPoint().y());
  }
}

TEST_F(StyledLabelTest, CacheSizeWithAlignment) {
  const std::string text("text");
  InitStyledLabel(text);
  styled()->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();
  ASSERT_EQ(1u, styled()->children().size());
  const View* child = styled()->children().front();
  EXPECT_EQ(1000, child->bounds().right());

  styled()->SetSize({800, 1000});
  styled()->Layout();
  ASSERT_EQ(1u, styled()->children().size());
  const View* new_child = styled()->children().front();
  EXPECT_EQ(child, new_child);
  EXPECT_EQ(800, new_child->bounds().right());
}

// Verifies that calling SizeToFit() on a label which requires less width still
// causes it to take the whole requested width.
TEST_F(StyledLabelTest, SizeToFit) {
  const std::string text("text");
  InitStyledLabel(text);
  styled()->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  styled()->SizeToFit(1000);
  styled()->Layout();
  ASSERT_EQ(1u, styled()->children().size());
  EXPECT_EQ(1000, styled()->children().front()->bounds().right());
}

// Verifies that a non-empty label has a preferred size by default.
TEST_F(StyledLabelTest, PreferredSizeNonEmpty) {
  const std::string text("text");
  InitStyledLabel(text);
  EXPECT_FALSE(styled()->GetPreferredSize().IsEmpty());
}

// Verifies that GetPreferredSize() respects the existing wrapping.
TEST_F(StyledLabelTest, PreferredSizeRespectsWrapping) {
  const std::string text("Long text that can be split across lines");
  InitStyledLabel(text);
  gfx::Size size = styled()->GetPreferredSize();
  size.set_width(size.width() / 2);
  size.set_height(styled()->GetHeightForWidth(size.width()));
  styled()->SetSize(size);
  styled()->Layout();
  const gfx::Size new_size = styled()->GetPreferredSize();
  EXPECT_LE(new_size.width(), size.width());
  EXPECT_EQ(new_size.height(), size.height());
}

// Verifies that calling a const method does not change the preferred size.
TEST_F(StyledLabelTest, PreferredSizeAcrossConstCall) {
  const std::string text("Long text that can be split across lines");
  InitStyledLabel(text);
  const gfx::Size size = styled()->GetPreferredSize();
  styled()->GetHeightForWidth(size.width() / 2);
  EXPECT_EQ(size, styled()->GetPreferredSize());
}

}  // namespace views
