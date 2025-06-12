// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/md_text_button_with_spinner.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"

namespace views {

constexpr int kSpinnerDiameter = 20;
constexpr int kSpinnerLabelSpacing = 8;
constexpr std::u16string_view kSpinnerText = u"Reloading site";
constexpr std::u16string_view kSuperLongText =
    u"Very long piece of text for the button label that should be shortened";

class MdTextButtonWithSpinnerTestPeer {
 public:
  explicit MdTextButtonWithSpinnerTestPeer(MdTextButtonWithSpinner* button)
      : button_(button) {}
  ~MdTextButtonWithSpinnerTestPeer() = default;
  Throbber* spinner() { return button_->spinner_; }
  Label* label() { return button_->label(); }
  void UpdateSpinnerColor() { button_->UpdateSpinnerColor(); }

 private:
  raw_ptr<MdTextButtonWithSpinner> button_;
};

class MdTextButtonWithSpinnerTest : public ViewsTestBase {
 public:
  MdTextButtonWithSpinnerTest() = default;
  ~MdTextButtonWithSpinnerTest() override = default;
  MdTextButtonWithSpinnerTest(const MdTextButtonWithSpinnerTest&) = delete;
  MdTextButtonWithSpinnerTest& operator=(const MdTextButtonWithSpinnerTest&) =
      delete;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(
        Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);

    widget_->Init(std::move(params));
    widget_->Show();

    auto button = std::make_unique<MdTextButtonWithSpinner>(
        Button::PressedCallback(), kSpinnerText);
    spinner_button_ = widget_->SetContentsView(std::move(button));
    spinner_button_->SizeToPreferredSize();
    peer_ = std::make_unique<MdTextButtonWithSpinnerTestPeer>(
        spinner_button_.get());
  }

  void TearDown() override {
    peer_.reset();
    spinner_button_ = nullptr;
    if (widget_) {
      widget_->CloseNow();
      widget_.reset();
    }
    ViewsTestBase::TearDown();
  }

  void AdjustWidgetAndButtonSizeForAlignment() {
    gfx::Size spinner_button_size =
        spinner_button_->GetPreferredSize(SizeBounds());
    spinner_button_size.Enlarge(400, 0);
    spinner_button_->SetSize(spinner_button_size);
    widget_->SetSize(spinner_button_size);
  }

 protected:
  std::unique_ptr<Widget> widget_;
  raw_ptr<MdTextButtonWithSpinner> spinner_button_;
  std::unique_ptr<MdTextButtonWithSpinnerTestPeer> peer_;
};

TEST_F(MdTextButtonWithSpinnerTest, SetAndGetSpinnerVisible) {
  // Initial button spinner should not be visible.
  EXPECT_FALSE(spinner_button_->GetSpinnerVisible());

  spinner_button_->SetSpinnerVisible(true);
  EXPECT_TRUE(spinner_button_->GetSpinnerVisible());

  spinner_button_->SetSpinnerVisible(false);
  EXPECT_FALSE(spinner_button_->GetSpinnerVisible());
}

TEST_F(MdTextButtonWithSpinnerTest,
       CalculatePreferredSizeWhenSpinnerNotVisible) {
  spinner_button_->SetSpinnerVisible(false);

  // Get the preferred size of a standard MdTextButton with the same text.
  MdTextButton standard_button(Button::PressedCallback(), kSpinnerText);
  gfx::Size expected_size =
      standard_button.CalculatePreferredSize(SizeBounds());

  EXPECT_EQ(expected_size,
            spinner_button_->CalculatePreferredSize(SizeBounds()));
}

TEST_F(MdTextButtonWithSpinnerTest, CalculatePreferredSizeWhenSpinnerVisible) {
  spinner_button_->SetSpinnerVisible(true);
  MdTextButton standard_button(Button::PressedCallback(), kSpinnerText);
  gfx::Size standard_size =
      standard_button.CalculatePreferredSize(SizeBounds());

  // Expect width to increase by kSpinnerDiameter + kSpinnerLabelSpacing.
  gfx::Size expected_size = standard_size;
  expected_size.set_width(standard_size.width() + kSpinnerDiameter +
                          kSpinnerLabelSpacing);

  EXPECT_EQ(expected_size,
            spinner_button_->CalculatePreferredSize(SizeBounds()));
}

TEST_F(MdTextButtonWithSpinnerTest,
       CalculateLeftHorizontalAlignmentWithSpinner) {
  spinner_button_->SetSpinnerVisible(true);
  AdjustWidgetAndButtonSizeForAlignment();

  // ALIGN_LEFT
  spinner_button_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  views::test::RunScheduledLayout(spinner_button_);
  EXPECT_LT(peer_->spinner()->bounds().right(), peer_->label()->bounds().x());
}

TEST_F(MdTextButtonWithSpinnerTest,
       CalculateCenterHorizontalAlignmentWithSpinner) {
  spinner_button_->SetSpinnerVisible(true);
  AdjustWidgetAndButtonSizeForAlignment();

  // Get left alignment for comparison.
  spinner_button_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  views::test::RunScheduledLayout(spinner_button_);
  EXPECT_LT(peer_->spinner()->bounds().right(), peer_->label()->bounds().x());
  int left_align_x = peer_->spinner()->bounds().x();
  int left_align_label_midpoint = peer_->label()->bounds().CenterPoint().x();

  // ALIGN_CENTER
  spinner_button_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  views::test::RunScheduledLayout(spinner_button_);
  EXPECT_LT(peer_->spinner()->bounds().right(), peer_->label()->bounds().x());
  int center_align_x = peer_->spinner()->bounds().x();
  int center_align_label_midpoint = peer_->label()->bounds().CenterPoint().x();
  EXPECT_LT(left_align_x, center_align_x);
  EXPECT_LT(left_align_label_midpoint, center_align_label_midpoint);
}

TEST_F(MdTextButtonWithSpinnerTest,
       CalculateRightzHorizontalAlignmentWithSpinner) {
  spinner_button_->SetSpinnerVisible(true);
  AdjustWidgetAndButtonSizeForAlignment();

  // Get left alignment for comparison.
  spinner_button_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  views::test::RunScheduledLayout(spinner_button_);
  EXPECT_LT(peer_->spinner()->bounds().right(), peer_->label()->bounds().x());
  int left_align_x = peer_->spinner()->bounds().x();
  int left_align_label_midpoint = peer_->label()->bounds().CenterPoint().x();

  // Get center alignment for comparison.
  spinner_button_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  views::test::RunScheduledLayout(spinner_button_);
  EXPECT_LT(peer_->spinner()->bounds().right(), peer_->label()->bounds().x());
  int center_align_x = peer_->spinner()->bounds().x();
  int center_align_label_midpoint = peer_->label()->bounds().CenterPoint().x();

  // ALIGN_RIGHT
  spinner_button_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  views::test::RunScheduledLayout(spinner_button_);
  int right_align_x = peer_->spinner()->bounds().x();
  EXPECT_LT(peer_->spinner()->bounds().x(), peer_->label()->bounds().right());
  int right_align_label_midpoint = peer_->label()->bounds().CenterPoint().x();
  EXPECT_LT(left_align_x, right_align_x);
  EXPECT_LT(center_align_x, right_align_x);
  EXPECT_LT(left_align_label_midpoint, right_align_label_midpoint);
  EXPECT_LT(center_align_label_midpoint, right_align_label_midpoint);
}

TEST_F(MdTextButtonWithSpinnerTest,
       TruncatesLongTextLabelWhenAvailableWidthIsSmaller) {
  spinner_button_->SetSpinnerVisible(true);
  spinner_button_->SetText(kSuperLongText);

  // Get the preferred width of the label if it were not truncated.
  const gfx::FontList font_list = peer_->label()->font_list();
  const int original_full_label_width =
      gfx::GetStringWidth(kSuperLongText, font_list);

  // Calculate a constrained width for the button (50 is for the label spacing).
  const int kConstrainedButtonContentWidth =
      kSpinnerDiameter + kSpinnerLabelSpacing + 50;
  const int kConstrainedButtonTotalWidth = kConstrainedButtonContentWidth +
                                           spinner_button_->GetInsets().left() +
                                           spinner_button_->GetInsets().right();

  // Apply the constrained size to the button and widget.
  spinner_button_->SetSize(
      gfx::Size(kConstrainedButtonTotalWidth,
                spinner_button_->GetPreferredSize(SizeBounds()).height()));
  widget_->SetSize(
      gfx::Size(kConstrainedButtonTotalWidth,
                spinner_button_->GetPreferredSize(SizeBounds()).height()));
  // Force layout after sizing.
  views::test::RunScheduledLayout(spinner_button_.get());
  views::test::RunScheduledLayout(widget_.get());

  EXPECT_LT(peer_->label()->bounds().width(), original_full_label_width);
  EXPECT_EQ(peer_->label()->bounds().width(), 50);
  EXPECT_EQ(
      peer_->spinner()->bounds().x() + kSpinnerDiameter + kSpinnerLabelSpacing,
      peer_->label()->bounds().x());
  EXPECT_EQ(spinner_button_->bounds().width(), kConstrainedButtonTotalWidth);
}

TEST_F(MdTextButtonWithSpinnerTest,
       UpdatesSpinnerColorForDifferentButtonStyles) {
  // Default style.
  spinner_button_->SetStyle(ui::ButtonStyle::kDefault);
  peer_->UpdateSpinnerColor();
  EXPECT_EQ(ui::kColorButtonForeground, peer_->spinner()->GetColorId());

  // Prominent style.
  spinner_button_->SetStyle(ui::ButtonStyle::kProminent);
  peer_->UpdateSpinnerColor();
  EXPECT_EQ(ui::kColorButtonForegroundProminent,
            peer_->spinner()->GetColorId());

  // Tonal style.
  spinner_button_->SetStyle(ui::ButtonStyle::kTonal);
  peer_->UpdateSpinnerColor();
  EXPECT_EQ(ui::kColorButtonForegroundTonal, peer_->spinner()->GetColorId());

  // Disabled state.
  spinner_button_->SetEnabled(false);
  peer_->UpdateSpinnerColor();
  EXPECT_EQ(ui::kColorButtonForegroundDisabled, peer_->spinner()->GetColorId());

  // Re-enable and check default state reverts correctly.
  spinner_button_->SetEnabled(true);
  spinner_button_->SetStyle(ui::ButtonStyle::kDefault);  // Reset style
  peer_->UpdateSpinnerColor();
  EXPECT_EQ(ui::kColorButtonForeground, peer_->spinner()->GetColorId());
}

}  // namespace views
