// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/styled_label.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_fragment.h"
#include "ui/views/style/typography.h"
#include "ui/views/test/test_layout_provider.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;

namespace views {

namespace {

using ::testing::SizeIs;

Label* LabelAt(StyledLabel* styled,
               size_t index,
               std::string expected_classname = Label::kViewClassName) {
  View* const child = styled->children()[index];
  EXPECT_EQ(expected_classname, child->GetClassName());
  return static_cast<Label*>(child);
}

int StyledLabelContentHeightForWidth(StyledLabel* styled, int w) {
  return styled->GetHeightForWidth(w) - styled->GetInsets().height();
}

}  // namespace

class StyledLabelTest : public ViewsTestBase {
 public:
  StyledLabelTest() = default;
  StyledLabelTest(const StyledLabelTest&) = delete;
  StyledLabelTest& operator=(const StyledLabelTest&) = delete;
  ~StyledLabelTest() override = default;

 protected:
  StyledLabel* InitStyledLabel(const std::string& ascii_text) {
    styled_ = std::make_unique<StyledLabel>();
    styled_->SetText(ASCIIToUTF16(ascii_text));
    return styled_.get();
  }

 private:
  std::unique_ptr<StyledLabel> styled_;
};

class StyledLabelInWidgetTest : public ViewsTestBase {
 public:
  StyledLabelInWidgetTest() = default;
  StyledLabelInWidgetTest(const StyledLabelInWidgetTest&) = delete;
  StyledLabelInWidgetTest& operator=(const StyledLabelInWidgetTest&) = delete;
  ~StyledLabelInWidgetTest() override = default;

 protected:
  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  Widget* widget() const { return widget_.get(); }

  StyledLabel* InitStyledLabel(const std::string& ascii_text) {
    View* container = widget_->SetContentsView(std::make_unique<View>());
    StyledLabel* styled =
        container->AddChildView(std::make_unique<StyledLabel>());
    styled->SetText(ASCIIToUTF16(ascii_text));
    return styled;
  }

 private:
  std::unique_ptr<Widget> widget_;
};

TEST_F(StyledLabelTest, NoWrapping) {
  const std::string text("This is a test block of text");
  StyledLabel* styled = InitStyledLabel(text);
  Label label(ASCIIToUTF16(text));
  const gfx::Size label_preferred_size = label.GetPreferredSize({});
  EXPECT_EQ(label_preferred_size.height(),
            StyledLabelContentHeightForWidth(styled,
                                             label_preferred_size.width() * 2));
}

TEST_F(StyledLabelTest, TrailingWhitespaceiIgnored) {
  const std::string text("This is a test block of text   ");
  StyledLabel* styled = InitStyledLabel(text);

  styled->SetBounds(0, 0, 1000, 1000);
  test::RunScheduledLayout(styled);

  ASSERT_EQ(1u, styled->children().size());
  EXPECT_EQ(u"This is a test block of text", LabelAt(styled, 0)->GetText());
}

TEST_F(StyledLabelTest, RespectLeadingWhitespace) {
  const std::string text("   This is a test block of text");
  StyledLabel* styled = InitStyledLabel(text);

  styled->SetBounds(0, 0, 1000, 1000);
  test::RunScheduledLayout(styled);

  ASSERT_EQ(1u, styled->children().size());
  EXPECT_EQ(u"   This is a test block of text", LabelAt(styled, 0)->GetText());
}

TEST_F(StyledLabelTest, RespectLeadingSpacesInNonFirstLine) {
  const std::string indented_line = "  indented line";
  const std::string text(std::string("First line\n") + indented_line);
  StyledLabel* styled = InitStyledLabel(text);
  styled->SetBounds(0, 0, 1000, 1000);
  test::RunScheduledLayout(styled);
  ASSERT_EQ(2u, styled->children().size());
  EXPECT_EQ(ASCIIToUTF16(indented_line), LabelAt(styled, 1)->GetText());
}

TEST_F(StyledLabelTest, CorrectWrapAtNewline) {
  const std::string first_line = "Line one";
  const std::string second_line = "  two";
  const std::string multiline_text(first_line + "\n" + second_line);
  StyledLabel* styled = InitStyledLabel(multiline_text);
  Label label(ASCIIToUTF16(first_line));
  gfx::Size label_preferred_size = label.GetPreferredSize({});
  // Correct handling of \n and label width limit encountered at the same place
  styled->SetBounds(0, 0, label_preferred_size.width(), 1000);
  test::RunScheduledLayout(styled);
  ASSERT_EQ(2u, styled->children().size());
  EXPECT_EQ(ASCIIToUTF16(first_line), LabelAt(styled, 0)->GetText());
  const auto* label_1 = LabelAt(styled, 1);
  EXPECT_EQ(ASCIIToUTF16(second_line), label_1->GetText());
  EXPECT_EQ(styled->GetHeightForWidth(1000), label_1->bounds().bottom());
}

TEST_F(StyledLabelTest, FirstLineNotEmptyWhenLeadingWhitespaceTooLong) {
  const std::string text("                                     a");
  StyledLabel* styled = InitStyledLabel(text);

  Label label(ASCIIToUTF16(text));
  gfx::Size label_preferred_size = label.GetPreferredSize({});

  styled->SetBounds(0, 0, label_preferred_size.width() / 2, 1000);
  test::RunScheduledLayout(styled);

  ASSERT_EQ(1u, styled->children().size());
  EXPECT_EQ(u"a", LabelAt(styled, 0)->GetText());
  EXPECT_EQ(label_preferred_size.height(),
            styled->GetHeightForWidth(label_preferred_size.width() / 2));
}

TEST_F(StyledLabelTest, BasicWrapping) {
  const std::string text("This is a test block of text");
  StyledLabel* styled = InitStyledLabel(text);
  Label label(ASCIIToUTF16(text.substr(0, text.size() * 2 / 3)));
  gfx::Size label_preferred_size = label.GetPreferredSize({});
  EXPECT_EQ(
      label_preferred_size.height() * 2,
      StyledLabelContentHeightForWidth(styled, label_preferred_size.width()));

  // Also respect the border.
  styled->SetBorder(CreateEmptyBorder(3));
  styled->SetBounds(
      0, 0, styled->GetInsets().width() + label_preferred_size.width(),
      styled->GetInsets().height() + 2 * label_preferred_size.height());
  test::RunScheduledLayout(styled);
  ASSERT_EQ(2u, styled->children().size());
  EXPECT_EQ(3, styled->children()[0]->x());
  EXPECT_EQ(3, styled->children()[0]->y());
  EXPECT_EQ(styled->height() - 3, styled->children()[1]->bounds().bottom());
}

TEST_F(StyledLabelTest, AllowEmptyLines) {
  const std::string text("one");
  StyledLabel* styled = InitStyledLabel(text);
  int default_height = styled->GetHeightForWidth(1000);
  const std::string multiline_text("one\n\nthree");
  styled = InitStyledLabel(multiline_text);
  styled->SetBounds(0, 0, 1000, 1000);
  test::RunScheduledLayout(styled);
  EXPECT_EQ(3 * default_height, styled->GetHeightForWidth(1000));
  ASSERT_EQ(2u, styled->children().size());
  EXPECT_EQ(styled->GetHeightForWidth(1000),
            styled->children()[1]->bounds().bottom());
}

TEST_F(StyledLabelTest, WrapLongWords) {
  const std::string text("ThisIsTextAsASingleWord");
  StyledLabel* styled = InitStyledLabel(text);
  Label label(ASCIIToUTF16(text.substr(0, text.size() * 2 / 3)));
  gfx::Size label_preferred_size = label.GetPreferredSize({});
  EXPECT_EQ(
      label_preferred_size.height() * 2,
      StyledLabelContentHeightForWidth(styled, label_preferred_size.width()));

  styled->SetBounds(
      0, 0, styled->GetInsets().width() + label_preferred_size.width(),
      styled->GetInsets().height() + 2 * label_preferred_size.height());
  test::RunScheduledLayout(styled);

  ASSERT_EQ(2u, styled->children().size());
  ASSERT_EQ(gfx::Point(), styled->origin());
  const auto* label_0 = LabelAt(styled, 0);
  const auto* label_1 = LabelAt(styled, 1);
  EXPECT_EQ(gfx::Point(), label_0->origin());
  EXPECT_EQ(gfx::Point(0, styled->height() / 2), label_1->origin());

  EXPECT_FALSE(label_0->GetText().empty());
  EXPECT_FALSE(label_1->GetText().empty());
  EXPECT_EQ(ASCIIToUTF16(text), label_0->GetText() + label_1->GetText());
}

TEST_F(StyledLabelTest, CreateLinks) {
  const std::string text("This is a test block of text.");
  StyledLabel* styled = InitStyledLabel(text);

  // Without links, there should be no focus border.
  EXPECT_TRUE(styled->GetInsets().IsEmpty());

  // Now let's add some links.
  styled->AddStyleRange(
      gfx::Range(0, 1),
      StyledLabel::RangeStyleInfo::CreateForLink(base::RepeatingClosure()));
  styled->AddStyleRange(
      gfx::Range(1, 2),
      StyledLabel::RangeStyleInfo::CreateForLink(base::RepeatingClosure()));
  styled->AddStyleRange(
      gfx::Range(10, 11),
      StyledLabel::RangeStyleInfo::CreateForLink(base::RepeatingClosure()));
  styled->AddStyleRange(
      gfx::Range(12, 13),
      StyledLabel::RangeStyleInfo::CreateForLink(base::RepeatingClosure()));

  // Insets shouldn't change when links are added, since the links indicate
  // focus by adding an underline instead.
  EXPECT_TRUE(styled->GetInsets().IsEmpty());

  // Verify layout creates the right number of children.
  styled->SetBounds(0, 0, 1000, 1000);
  test::RunScheduledLayout(styled);
  EXPECT_EQ(7u, styled->children().size());
}

TEST_F(StyledLabelTest, StyledRangeCustomFontUnderlined) {
  const std::string text("This is a test block of text, ");
  const std::string underlined_text("and this should be undelined");
  StyledLabel* styled = InitStyledLabel(text + underlined_text);
  StyledLabel::RangeStyleInfo style_info;
  style_info.tooltip = u"tooltip";
  style_info.custom_font =
      styled->GetFontList().DeriveWithStyle(gfx::Font::UNDERLINE);
  styled->AddStyleRange(
      gfx::Range(text.size(), text.size() + underlined_text.size()),
      style_info);

  styled->SetBounds(0, 0, 1000, 1000);
  test::RunScheduledLayout(styled);

  ASSERT_EQ(2u, styled->children().size());
  EXPECT_EQ(gfx::Font::UNDERLINE,
            LabelAt(styled, 1)->font_list().GetFontStyle());
}

TEST_F(StyledLabelTest, StyledRangeTextStyleBold) {
  test::TestLayoutProvider bold_provider;
  const std::string bold_text(
      "This is a block of text whose style will be set to BOLD in the test");
  const std::string text(" normal text");
  StyledLabel* styled = InitStyledLabel(bold_text + text);

  // Pretend disabled text becomes bold for testing.
  auto details =
      bold_provider.GetFontDetails(style::CONTEXT_LABEL, style::STYLE_DISABLED);
  details.weight = gfx::Font::Weight::BOLD;
  bold_provider.SetFontDetails(style::CONTEXT_LABEL, style::STYLE_DISABLED,
                               details);

  StyledLabel::RangeStyleInfo style_info;
  style_info.text_style = style::STYLE_DISABLED;
  styled->AddStyleRange(gfx::Range(0u, bold_text.size()), style_info);

  // Calculate the bold text width if it were a pure label view, both with bold
  // and normal style.
  Label label(ASCIIToUTF16(bold_text));
  const gfx::Size normal_label_size = label.GetPreferredSize({});
  label.SetFontList(
      label.font_list().DeriveWithWeight(gfx::Font::Weight::BOLD));
  const gfx::Size bold_label_size = label.GetPreferredSize({});

  ASSERT_GE(bold_label_size.width(), normal_label_size.width());

  // Set the width so |bold_text| doesn't fit on a single line with bold style,
  // but does with normal font style.
  int styled_width = (normal_label_size.width() + bold_label_size.width()) / 2;
  int pref_height = styled->GetHeightForWidth(styled_width);

  // Sanity check that |bold_text| with normal font style would fit on a single
  // line in a styled label with width |styled_width|.
  StyledLabel unstyled;
  unstyled.SetText(ASCIIToUTF16(bold_text));
  unstyled.SetBounds(0, 0, styled_width, pref_height);
  test::RunScheduledLayout(&unstyled);
  EXPECT_EQ(1u, unstyled.children().size());

  styled->SetBounds(0, 0, styled_width, pref_height);
  test::RunScheduledLayout(styled);

  ASSERT_EQ(3u, styled->children().size());

  // The bold text should be broken up into two parts.
  const auto* label_0 = LabelAt(styled, 0);
  const auto* label_1 = LabelAt(styled, 1);
  const auto* label_2 = LabelAt(styled, 2);
  EXPECT_EQ(gfx::Font::Weight::BOLD, label_0->font_list().GetFontWeight());
  EXPECT_EQ(gfx::Font::Weight::BOLD, label_1->font_list().GetFontWeight());
  EXPECT_EQ(gfx::Font::NORMAL, label_2->font_list().GetFontStyle());

  // The second bold part should start on a new line.
  EXPECT_EQ(0, label_0->x());
  EXPECT_EQ(0, label_1->x());
  EXPECT_EQ(label_1->bounds().right(), label_2->x());
}

TEST_F(StyledLabelInWidgetTest, Color) {
  const std::string text_blue("BLUE");
  const std::string text_link("link");
  const std::string text("word");
  StyledLabel* styled = InitStyledLabel(text_blue + text_link + text);

  StyledLabel::RangeStyleInfo style_info_blue;
  style_info_blue.override_color = SK_ColorBLUE;
  styled->AddStyleRange(gfx::Range(0u, text_blue.size()), style_info_blue);

  StyledLabel::RangeStyleInfo style_info_link =
      StyledLabel::RangeStyleInfo::CreateForLink(base::RepeatingClosure());
  styled->AddStyleRange(
      gfx::Range(text_blue.size(), text_blue.size() + text_link.size()),
      style_info_link);

  styled->SetBounds(0, 0, 1000, 1000);
  test::RunScheduledLayout(styled);

  // The code below is not prepared to deal with dark mode.
  auto* const native_theme = widget()->GetNativeTheme();
  native_theme->set_use_dark_colors(false);
  native_theme->NotifyOnNativeThemeUpdated();

  auto* container = widget()->GetContentsView();
  // Obtain the default text color for a label.
  Label* label =
      container->AddChildView(std::make_unique<Label>(ASCIIToUTF16(text)));
  const SkColor kDefaultTextColor = label->GetEnabledColor();

  // Obtain the default text color for a link.
  Link* link =
      container->AddChildView(std::make_unique<Link>(ASCIIToUTF16(text_link)));
  const SkColor kDefaultLinkColor = link->GetEnabledColor();

  ASSERT_EQ(3u, styled->children().size());
  EXPECT_EQ(SK_ColorBLUE, LabelAt(styled, 0)->GetEnabledColor());
  EXPECT_EQ(
      kDefaultLinkColor,
      LabelAt(styled, 1, LinkFragment::kViewClassName)->GetEnabledColor());
  EXPECT_EQ(kDefaultTextColor, LabelAt(styled, 2)->GetEnabledColor());

  // Test adjusted color readability.
  styled->SetDisplayedOnBackgroundColor(SK_ColorBLACK);
  test::RunScheduledLayout(styled);
  label->SetBackgroundColor(SK_ColorBLACK);

  const SkColor kAdjustedTextColor = label->GetEnabledColor();
  EXPECT_NE(kAdjustedTextColor, kDefaultTextColor);
  EXPECT_EQ(kAdjustedTextColor, LabelAt(styled, 2)->GetEnabledColor());
}

TEST_F(StyledLabelInWidgetTest, SetBackgroundColor) {
  StyledLabel* styled = InitStyledLabel("test label");

  styled->SetBounds(0, 0, 1000, 1000);
  test::RunScheduledLayout(styled);

  ASSERT_THAT(styled->children(), SizeIs(1u));
  // The default background color is `ui::kColorDialogBackground`.
  EXPECT_EQ(widget()->GetColorProvider()->GetColor(ui::kColorDialogBackground),
            LabelAt(styled, 0)->GetBackgroundColor());

  styled->SetDisplayedOnBackgroundColor(SK_ColorBLUE);
  EXPECT_EQ(SK_ColorBLUE, LabelAt(styled, 0)->GetBackgroundColor());

  styled->SetDisplayedOnBackgroundColor(ui::kColorAlertHighSeverity);
  EXPECT_EQ(widget()->GetColorProvider()->GetColor(ui::kColorAlertHighSeverity),
            LabelAt(styled, 0)->GetBackgroundColor());

  // Setting a color overwrites the color id.
  styled->SetDisplayedOnBackgroundColor(SK_ColorCYAN);
  EXPECT_EQ(SK_ColorCYAN, LabelAt(styled, 0)->GetBackgroundColor());
}

TEST_F(StyledLabelInWidgetTest, SetBackgroundColorIdReactsToThemeChange) {
  StyledLabel* styled = InitStyledLabel("test label");

  styled->SetBounds(0, 0, 1000, 1000);
  test::RunScheduledLayout(styled);

  ASSERT_THAT(styled->children(), SizeIs(1u));
  auto* const native_theme = widget()->GetNativeTheme();
  native_theme->set_use_dark_colors(true);
  native_theme->NotifyOnNativeThemeUpdated();
  EXPECT_EQ(widget()->GetColorProvider()->GetColor(ui::kColorDialogBackground),
            LabelAt(styled, 0)->GetBackgroundColor());

  native_theme->set_use_dark_colors(false);
  native_theme->NotifyOnNativeThemeUpdated();
  EXPECT_EQ(widget()->GetColorProvider()->GetColor(ui::kColorDialogBackground),
            LabelAt(styled, 0)->GetBackgroundColor());

  styled->SetDisplayedOnBackgroundColor(ui::kColorAlertHighSeverity);
  EXPECT_EQ(widget()->GetColorProvider()->GetColor(ui::kColorAlertHighSeverity),
            LabelAt(styled, 0)->GetBackgroundColor());

  native_theme->set_use_dark_colors(true);
  native_theme->NotifyOnNativeThemeUpdated();
  EXPECT_EQ(widget()->GetColorProvider()->GetColor(ui::kColorAlertHighSeverity),
            LabelAt(styled, 0)->GetBackgroundColor());
}

TEST_F(StyledLabelTest, StyledRangeWithTooltip) {
  const std::string text("This is a test block of text, ");
  const std::string tooltip_text("this should have a tooltip,");
  const std::string normal_text(" this should not have a tooltip, ");
  const std::string link_text("and this should be a link");

  const size_t tooltip_start = text.size();
  const size_t link_start =
      text.size() + tooltip_text.size() + normal_text.size();

  StyledLabel* styled =
      InitStyledLabel(text + tooltip_text + normal_text + link_text);
  StyledLabel::RangeStyleInfo tooltip_style;
  tooltip_style.tooltip = u"tooltip";
  styled->AddStyleRange(
      gfx::Range(tooltip_start, tooltip_start + tooltip_text.size()),
      tooltip_style);
  styled->AddStyleRange(
      gfx::Range(link_start, link_start + link_text.size()),
      StyledLabel::RangeStyleInfo::CreateForLink(base::RepeatingClosure()));

  // Break line inside the range with the tooltip.
  Label label(
      ASCIIToUTF16(text + tooltip_text.substr(0, tooltip_text.size() - 3)));
  gfx::Size label_preferred_size = label.GetPreferredSize({});
  int pref_height = styled->GetHeightForWidth(label_preferred_size.width());
  EXPECT_EQ(label_preferred_size.height() * 3,
            pref_height - styled->GetInsets().height());

  styled->SetBounds(0, 0, label_preferred_size.width(), pref_height);
  test::RunScheduledLayout(styled);

  EXPECT_EQ(label_preferred_size.width(), styled->width());

  ASSERT_EQ(6u, styled->children().size());

  // The labels shouldn't be offset to cater for focus rings.
  EXPECT_EQ(0, styled->children()[0]->x());
  EXPECT_EQ(0, styled->children()[2]->x());

  EXPECT_EQ(styled->children()[0]->bounds().right(),
            styled->children()[1]->x());
  EXPECT_EQ(styled->children()[2]->bounds().right(),
            styled->children()[3]->x());

  std::u16string tooltip =
      styled->children()[1]->GetTooltipText(gfx::Point(1, 1));
  EXPECT_EQ(u"tooltip", tooltip);
  tooltip = styled->children()[2]->GetTooltipText(gfx::Point(1, 1));
  EXPECT_EQ(u"tooltip", tooltip);
}

TEST_F(StyledLabelTest, SetTextContextAndDefaultStyle) {
  const std::string text("This is a test block of text.");
  StyledLabel* styled = InitStyledLabel(text);
  styled->SetTextContext(style::CONTEXT_DIALOG_TITLE);
  styled->SetDefaultTextStyle(style::STYLE_DISABLED);
  Label label(ASCIIToUTF16(text), style::CONTEXT_DIALOG_TITLE,
              style::STYLE_DISABLED);

  gfx::Size preferred_size = label.GetPreferredSize({});
  styled->SetBounds(0, 0, preferred_size.width(), preferred_size.height());

  // Make sure we have the same sizing as a label with the same style.
  EXPECT_EQ(preferred_size.height(), styled->height());
  EXPECT_EQ(preferred_size.width(), styled->width());

  test::RunScheduledLayout(styled);
  ASSERT_EQ(1u, styled->children().size());
  Label* sublabel = LabelAt(styled, 0);
  EXPECT_EQ(style::CONTEXT_DIALOG_TITLE, sublabel->GetTextContext());

  EXPECT_NE(SK_ColorBLACK, label.GetEnabledColor());  // Sanity check,
  EXPECT_EQ(label.GetEnabledColor(), sublabel->GetEnabledColor());
}

TEST_F(StyledLabelTest, LineHeight) {
  const std::string text("one\ntwo\nthree");
  StyledLabel* styled = InitStyledLabel(text);
  styled->SetLineHeight(18);
  EXPECT_EQ(18 * 3, styled->GetHeightForWidth(100));
}

TEST_F(StyledLabelTest, LineHeightWithBorder) {
  const std::string text("one\ntwo\nthree");
  StyledLabel* styled = InitStyledLabel(text);
  styled->SetLineHeight(18);
  styled->SetBorder(views::CreateSolidBorder(1, SK_ColorGRAY));
  EXPECT_EQ(18 * 3 + 2, styled->GetHeightForWidth(100));
}

TEST_F(StyledLabelTest, LineHeightWithLink) {
  const std::string text("one\ntwo\nthree");
  StyledLabel* styled = InitStyledLabel(text);
  styled->SetLineHeight(18);

  styled->AddStyleRange(
      gfx::Range(0, 3),
      StyledLabel::RangeStyleInfo::CreateForLink(base::RepeatingClosure()));
  styled->AddStyleRange(
      gfx::Range(4, 7),
      StyledLabel::RangeStyleInfo::CreateForLink(base::RepeatingClosure()));
  styled->AddStyleRange(
      gfx::Range(8, 13),
      StyledLabel::RangeStyleInfo::CreateForLink(base::RepeatingClosure()));
  EXPECT_EQ(18 * 3, styled->GetHeightForWidth(100));
}

TEST_F(StyledLabelTest, HandleEmptyLayout) {
  const std::string text("This is a test block of text.");
  StyledLabel* styled = InitStyledLabel(text);
  test::RunScheduledLayout(styled);
  EXPECT_EQ(0u, styled->children().size());
}

TEST_F(StyledLabelTest, CacheSize) {
  const int preferred_height = 50;
  const int preferred_width = 100;
  const std::string text("This is a test block of text.");
  const std::u16string another_text(
      u"This is a test block of text. This text is much longer than previous");

  StyledLabel* styled = InitStyledLabel(text);

  // we should be able to calculate height without any problem
  // no controls should be created
  int precalculated_height = styled->GetHeightForWidth(preferred_width);
  EXPECT_LT(0, precalculated_height);
  EXPECT_EQ(0u, styled->children().size());

  styled->SetBounds(0, 0, preferred_width, preferred_height);
  test::RunScheduledLayout(styled);

  // controls should be created after layout
  // height should be the same as precalculated
  int real_height = styled->GetHeightForWidth(styled->width());
  View* first_child_after_layout =
      styled->children().empty() ? nullptr : styled->children().front();
  EXPECT_LT(0u, styled->children().size());
  EXPECT_LT(0, real_height);
  EXPECT_EQ(real_height, precalculated_height);

  // another call to Layout should not kill and recreate all controls
  test::RunScheduledLayout(styled);
  View* first_child_after_second_layout =
      styled->children().empty() ? nullptr : styled->children().front();
  EXPECT_EQ(first_child_after_layout, first_child_after_second_layout);

  // if text is changed:
  // layout should be recalculated
  // all controls should be recreated
  styled->SetText(another_text);
  int updated_height = styled->GetHeightForWidth(styled->width());
  EXPECT_NE(updated_height, real_height);
  View* first_child_after_text_update =
      styled->children().empty() ? nullptr : styled->children().front();
  EXPECT_NE(first_child_after_text_update, first_child_after_layout);
}

TEST_F(StyledLabelTest, Border) {
  const std::string text("One line");
  StyledLabel* styled = InitStyledLabel(text);
  Label label(ASCIIToUTF16(text));
  gfx::Size label_preferred_size = label.GetPreferredSize({});
  styled->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(5, 10, 6, 20)));
  styled->SetBounds(0, 0, 1000, 0);
  test::RunScheduledLayout(styled);
  EXPECT_EQ(
      label_preferred_size.height() + 5 /*top border*/ + 6 /*bottom border*/,
      styled->GetPreferredSize(SizeBounds(1000, {})).height());
  EXPECT_EQ(
      label_preferred_size.width() + 10 /*left border*/ + 20 /*right border*/,
      styled->GetPreferredSize(SizeBounds(1000, {})).width());
}

TEST_F(StyledLabelTest, LineHeightWithShorterCustomView) {
  const std::string text("one ");
  StyledLabel* styled = InitStyledLabel(text);
  int default_height = styled->GetHeightForWidth(1000);

  const std::string custom_view_text("with custom view");
  const int less_height = 10;
  std::unique_ptr<View> custom_view = std::make_unique<StaticSizedView>(
      gfx::Size(20, default_height - less_height));
  StyledLabel::RangeStyleInfo style_info;
  style_info.custom_view = custom_view.get();
  styled = InitStyledLabel(text + custom_view_text);
  styled->AddStyleRange(
      gfx::Range(text.size(), text.size() + custom_view_text.size()),
      style_info);
  styled->AddCustomView(std::move(custom_view));
  EXPECT_EQ(default_height, styled->GetHeightForWidth(100));
}

TEST_F(StyledLabelTest, LineHeightWithTallerCustomView) {
  const std::string text("one ");
  StyledLabel* styled = InitStyledLabel(text);
  int default_height = styled->GetHeightForWidth(100);

  const std::string custom_view_text("with custom view");
  const int more_height = 10;
  std::unique_ptr<View> custom_view = std::make_unique<StaticSizedView>(
      gfx::Size(20, default_height + more_height));
  StyledLabel::RangeStyleInfo style_info;
  style_info.custom_view = custom_view.get();
  styled = InitStyledLabel(text + custom_view_text);
  styled->AddStyleRange(
      gfx::Range(text.size(), text.size() + custom_view_text.size()),
      style_info);
  styled->AddCustomView(std::move(custom_view));
  EXPECT_EQ(default_height + more_height, styled->GetHeightForWidth(100));
}

TEST_F(StyledLabelTest, LineWrapperWithCustomView) {
  const std::string text_before("one ");
  StyledLabel* styled = InitStyledLabel(text_before);
  int default_height = styled->GetHeightForWidth(100);
  const std::string custom_view_text("two with custom view ");
  const std::string text_after("three");

  int custom_view_height = 25;
  std::unique_ptr<View> custom_view =
      std::make_unique<StaticSizedView>(gfx::Size(200, custom_view_height));
  StyledLabel::RangeStyleInfo style_info;
  style_info.custom_view = custom_view.get();
  styled = InitStyledLabel(text_before + custom_view_text + text_after);
  styled->AddStyleRange(
      gfx::Range(text_before.size(),
                 text_before.size() + custom_view_text.size()),
      style_info);
  styled->AddCustomView(std::move(custom_view));
  EXPECT_EQ(default_height * 2 + custom_view_height,
            styled->GetHeightForWidth(100));
}

TEST_F(StyledLabelTest, AlignmentInLTR) {
  const std::string text("text");
  StyledLabel* styled = InitStyledLabel(text);
  styled->SetBounds(0, 0, 1000, 1000);
  test::RunScheduledLayout(styled);
  const auto& children = styled->children();
  ASSERT_EQ(1u, children.size());

  // Test the default alignment puts the text on the leading side (left).
  EXPECT_EQ(0, children.front()->bounds().x());

  // Setting |ALIGN_RIGHT| indicates the text should be aligned to the trailing
  // side, and hence its trailing side coordinates (i.e. right) should align
  // with the trailing side coordinate of the label (right).
  styled->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  test::RunScheduledLayout(styled);
  EXPECT_EQ(1000, children.front()->bounds().right());

  // Setting |ALIGN_LEFT| indicates the text should be aligned to the leading
  // side, and hence its leading side coordinates (i.e. x) should align with the
  // leading side coordinate of the label (x).
  styled->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  test::RunScheduledLayout(styled);
  EXPECT_EQ(0, children.front()->bounds().x());

  styled->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  test::RunScheduledLayout(styled);
  Label label(ASCIIToUTF16(text));
  EXPECT_EQ((1000 - label.GetPreferredSize({}).width()) / 2,
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
  StyledLabel* styled = InitStyledLabel(text);
  styled->SetBounds(0, 0, 1000, 1000);
  test::RunScheduledLayout(styled);
  const auto& children = styled->children();
  ASSERT_EQ(1u, children.size());

  // Test the default alignment puts the text on the leading side (right).
  // Note that x-coordinates in RTL place the origin (0) on the right.
  EXPECT_EQ(0, children.front()->bounds().x());

  // Setting |ALIGN_RIGHT| indicates the text should be aligned to the trailing
  // side, and hence its trailing side coordinates (i.e. right) should align
  // with the trailing side coordinate of the label (right).
  styled->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  test::RunScheduledLayout(styled);
  EXPECT_EQ(1000, children.front()->bounds().right());

  // Setting |ALIGN_LEFT| indicates the text should be aligned to the leading
  // side, and hence its leading side coordinates (i.e. x) should align with the
  // leading side coordinate of the label (x).
  styled->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  test::RunScheduledLayout(styled);
  EXPECT_EQ(0, children.front()->bounds().x());

  styled->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  test::RunScheduledLayout(styled);
  Label label(ASCIIToUTF16(text));
  EXPECT_EQ((1000 - label.GetPreferredSize({}).width()) / 2,
            children.front()->bounds().x());
}

TEST_F(StyledLabelTest, ViewsCenteredWithLinkAndCustomView) {
  const std::string text("This is a test block of text, ");
  const std::string link_text("and this should be a link");
  const std::string custom_view_text("And this is a custom view");
  StyledLabel* styled = InitStyledLabel(text + link_text + custom_view_text);
  styled->AddStyleRange(
      gfx::Range(text.size(), text.size() + link_text.size()),
      StyledLabel::RangeStyleInfo::CreateForLink(base::RepeatingClosure()));

  int custom_view_height = 25;
  std::unique_ptr<View> custom_view =
      std::make_unique<StaticSizedView>(gfx::Size(20, custom_view_height));
  StyledLabel::RangeStyleInfo style_info;
  style_info.custom_view = custom_view.get();
  styled->AddStyleRange(
      gfx::Range(text.size() + link_text.size(),
                 text.size() + link_text.size() + custom_view_text.size()),
      style_info);
  styled->AddCustomView(std::move(custom_view));

  styled->SetBounds(0, 0, 1000, 500);
  test::RunScheduledLayout(styled);
  const int height =
      styled->GetPreferredSize(SizeBounds(styled->size())).height();
  for (const views::View* child : styled->children()) {
    EXPECT_EQ(height / 2, child->bounds().CenterPoint().y());
  }
}

TEST_F(StyledLabelTest, ViewsCenteredForEvenAndOddSizes) {
  constexpr int kViewWidth = 30;
  for (int height : {60, 61}) {
    StyledLabel* styled = InitStyledLabel("abc");

    const auto view_heights =
        std::to_array<int>({height, height / 2, height / 2 + 1});
    for (uint32_t i = 0; i < view_heights.size(); ++i) {
      auto view = std::make_unique<StaticSizedView>(
          gfx::Size(kViewWidth, view_heights[i]));
      StyledLabel::RangeStyleInfo style_info;
      style_info.custom_view = view.get();
      styled->AddStyleRange(gfx::Range(i, i + 1), style_info);
      styled->AddCustomView(std::move(view));
    }

    styled->SetBounds(0, 0, kViewWidth * 3, height);
    test::RunScheduledLayout(styled);

    for (const views::View* child : styled->children()) {
      EXPECT_EQ(height / 2, child->bounds().CenterPoint().y());
    }
  }
}

TEST_F(StyledLabelTest, CacheSizeWithAlignment) {
  const std::string text("text");
  StyledLabel* styled = InitStyledLabel(text);
  styled->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  styled->SetBounds(0, 0, 1000, 1000);
  test::RunScheduledLayout(styled);
  ASSERT_EQ(1u, styled->children().size());
  const View* child = styled->children().front();
  EXPECT_EQ(1000, child->bounds().right());

  styled->SetSize({800, 1000});
  test::RunScheduledLayout(styled);
  ASSERT_EQ(1u, styled->children().size());
  const View* new_child = styled->children().front();
  EXPECT_EQ(child, new_child);
  EXPECT_EQ(800, new_child->bounds().right());
}

// Verifies that calling SizeToFit() on a label which requires less width still
// causes it to take the whole requested width.
TEST_F(StyledLabelTest, SizeToFit) {
  const std::string text("text");
  StyledLabel* styled = InitStyledLabel(text);
  styled->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  styled->SizeToFit(1000);
  test::RunScheduledLayout(styled);
  ASSERT_EQ(1u, styled->children().size());
  EXPECT_EQ(1000, styled->children().front()->bounds().right());
}

// Verifies that a non-empty label has a preferred size by default.
TEST_F(StyledLabelTest, PreferredSizeNonEmpty) {
  const std::string text("text");
  StyledLabel* styled = InitStyledLabel(text);
  EXPECT_FALSE(styled->GetPreferredSize({}).IsEmpty());
}

// Verifies that GetPreferredSize() respects the existing wrapping.
TEST_F(StyledLabelTest, PreferredSizeRespectsWrapping) {
  const std::string text("Long text that can be split across lines");
  StyledLabel* styled = InitStyledLabel(text);
  gfx::Size size = styled->GetPreferredSize({});
  size.set_width(size.width() / 2);
  size.set_height(styled->GetHeightForWidth(size.width()));
  styled->SetSize(size);
  test::RunScheduledLayout(styled);
  const gfx::Size new_size = styled->GetPreferredSize(SizeBounds(size));
  EXPECT_LE(new_size.width(), size.width());
  EXPECT_EQ(new_size.height(), size.height());
}

// Verifies that calling a const method does not change the preferred size.
TEST_F(StyledLabelTest, PreferredSizeAcrossConstCall) {
  const std::string text("Long text that can be split across lines");
  StyledLabel* styled = InitStyledLabel(text);
  const gfx::Size size = styled->GetPreferredSize({});
  styled->GetHeightForWidth(size.width() / 2);
  EXPECT_EQ(size, styled->GetPreferredSize({}));
}

TEST_F(StyledLabelTest, AccessibleNameAndRole) {
  const std::string text("Text");
  StyledLabel* styled = InitStyledLabel(text);

  IgnoreMissingWidgetForTestingScopedSetter a11y_ignore_missing_widget_(
      styled->GetViewAccessibility());

  EXPECT_EQ(styled->GetViewAccessibility().GetCachedName(),
            base::UTF8ToUTF16(text));
  EXPECT_EQ(styled->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kStaticText);

  ui::AXNodeData data;
  styled->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetStringAttribute(ax::mojom::StringAttribute::kName), text);
  EXPECT_EQ(data.role, ax::mojom::Role::kStaticText);

  styled->SetTextContext(style::CONTEXT_DIALOG_TITLE);

  EXPECT_EQ(styled->GetViewAccessibility().GetCachedName(),
            base::UTF8ToUTF16(text));
  EXPECT_EQ(styled->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kTitleBar);

  data = ui::AXNodeData();
  styled->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetStringAttribute(ax::mojom::StringAttribute::kName), text);
  EXPECT_EQ(data.role, ax::mojom::Role::kTitleBar);

  styled->SetText(u"New Text");
  styled->GetViewAccessibility().SetRole(ax::mojom::Role::kLink);
  EXPECT_EQ(styled->GetViewAccessibility().GetCachedName(), u"New Text");
  EXPECT_EQ(styled->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kLink);

  data = ui::AXNodeData();
  styled->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"New Text");
  EXPECT_EQ(data.role, ax::mojom::Role::kLink);
}

// Regression test for crbug.com/361639416.
// Tests that the child views (text fragments) are still alive after layout.
TEST_F(StyledLabelTest, OldChildViewsAreAliveAfterLayout) {
  class ViewDestroyObserser : public ViewObserver {
   public:
    MOCK_METHOD(void, OnViewIsDeleting, (View*), (override));
  };

  ViewDestroyObserser view_destroy_observer;

  const std::string text("This is a test block of text.");
  StyledLabel* styled = InitStyledLabel(text);

  styled->AddStyleRange(
      gfx::Range(0, 1),
      StyledLabel::RangeStyleInfo::CreateForLink(base::RepeatingClosure()));

  EXPECT_EQ(styled->GetFirstLinkForTesting(), nullptr);
  styled->SetBounds(0, 0, 1000, 1000);
  test::RunScheduledLayout(styled);

  views::View* link = styled->GetFirstLinkForTesting();
  EXPECT_NE(link, nullptr);
  link->AddObserver(&view_destroy_observer);
  EXPECT_CALL(view_destroy_observer, OnViewIsDeleting(testing::_)).Times(0);
  styled->SetBounds(0, 0, 10, 1000);
  test::RunScheduledLayout(styled);

  link->RemoveObserver(&view_destroy_observer);
}

}  // namespace views
