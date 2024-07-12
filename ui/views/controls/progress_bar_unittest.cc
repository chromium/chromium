// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/progress_bar.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/color/color_id.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"

namespace views {

class ProgressBarTest : public ViewsTestBase {
 protected:
  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
    container_view_ = widget_->SetContentsView(std::make_unique<View>());
    auto* layout =
        container_view_->SetLayoutManager(std::make_unique<FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kVertical);
    bar_ = container_view_->AddChildView(std::make_unique<ProgressBar>());
    views::test::RunScheduledLayout(container_view_);
    widget_->Show();
  }

  void TearDown() override {
    container_view_ = nullptr;
    bar_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  ProgressBar* bar() { return bar_.get(); }

  std::unique_ptr<Widget> widget_;
  raw_ptr<views::View> container_view_;
  raw_ptr<ProgressBar> bar_;
};

TEST_F(ProgressBarTest, AccessibleNodeData) {
  bar()->SetValue(0.626);

  ui::AXNodeData node_data;
  bar()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ax::mojom::Role::kProgressIndicator, node_data.role);
  EXPECT_EQ(std::u16string(),
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(std::string("62%"),
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));
  EXPECT_FALSE(
      node_data.HasIntAttribute(ax::mojom::IntAttribute::kRestriction));
}

// Verifies the correct a11y events are raised for an accessible progress bar.
TEST_F(ProgressBarTest, AccessibilityEvents) {
  test::AXEventCounter ax_counter(views::AXEventManager::Get());
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kValueChanged));

  bar()->SetValue(0.50);
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kValueChanged));

  bar()->SetValue(0.63);
  EXPECT_EQ(2, ax_counter.GetCount(ax::mojom::Event::kValueChanged));

  bar()->SetValue(0.636);
  EXPECT_EQ(2, ax_counter.GetCount(ax::mojom::Event::kValueChanged));

  bar()->SetValue(0.642);
  EXPECT_EQ(3, ax_counter.GetCount(ax::mojom::Event::kValueChanged));

  widget_->Hide();
  widget_->Show();
  EXPECT_EQ(3, ax_counter.GetCount(ax::mojom::Event::kValueChanged));

  widget_->Hide();
  bar()->SetValue(0.8);
  EXPECT_EQ(3, ax_counter.GetCount(ax::mojom::Event::kValueChanged));

  widget_->Show();
  EXPECT_EQ(4, ax_counter.GetCount(ax::mojom::Event::kValueChanged));
}

// Test that default colors can be overridden. Used by Chromecast.
TEST_F(ProgressBarTest, OverrideDefaultColors) {
  EXPECT_NE(SK_ColorRED, bar()->GetForegroundColor());
  EXPECT_NE(SK_ColorGREEN, bar()->GetBackgroundColor());
  EXPECT_NE(bar()->GetForegroundColor(), bar()->GetBackgroundColor());

  bar()->SetForegroundColor(SK_ColorRED);
  bar()->SetBackgroundColor(SK_ColorGREEN);
  EXPECT_EQ(SK_ColorRED, bar()->GetForegroundColor());
  EXPECT_EQ(SK_ColorGREEN, bar()->GetBackgroundColor());

  // Override colors with color ID. It will also override the colors set with
  // SkColor.
  bar()->SetForegroundColorId(ui::kColorSysPrimary);
  bar()->SetBackgroundColorId(ui::kColorSysPrimaryContainer);
  const auto* color_provider = bar()->GetColorProvider();
  EXPECT_EQ(color_provider->GetColor(ui::kColorSysPrimary),
            bar()->GetForegroundColor());
  EXPECT_EQ(color_provider->GetColor(ui::kColorSysPrimaryContainer),
            bar()->GetBackgroundColor());
  EXPECT_EQ(ui::kColorSysPrimary, bar()->GetForegroundColorId().value());
  EXPECT_EQ(ui::kColorSysPrimaryContainer,
            bar()->GetBackgroundColorId().value());

  // Override the colors set with color ID by SkColor.
  bar()->SetForegroundColor(SK_ColorRED);
  bar()->SetBackgroundColor(SK_ColorGREEN);
  EXPECT_EQ(SK_ColorRED, bar()->GetForegroundColor());
  EXPECT_EQ(SK_ColorGREEN, bar()->GetBackgroundColor());
  EXPECT_EQ(std::nullopt, bar()->GetForegroundColorId());
  EXPECT_EQ(std::nullopt, bar()->GetBackgroundColorId());
}

// Test that if no `preferred_corner_radii` are provided the default radius is
// 3, and a value of `std::nullopt` will not round the corners.
TEST_F(ProgressBarTest, RoundCornerDefault) {
  // The default bar should have a rounded corner radius of 3.
  EXPECT_EQ(gfx::RoundedCornersF(3), bar()->GetPreferredCornerRadii());

  // Setting `std::nullopt` for the corner radius should make the bar have no
  // rounded corners.
  bar()->SetPreferredHeight(12);
  bar()->SetPreferredCornerRadii(std::nullopt);
  views::test::RunScheduledLayout(container_view_);
  EXPECT_EQ(gfx::RoundedCornersF(0), bar()->GetPreferredCornerRadii());
  EXPECT_TRUE(bar()->GetPreferredCornerRadii().IsEmpty());
}

// Test that a value set for `preferred_corner_radii` is saved and can be
// retrieved from `GetPreferredCornerRadii()`.
TEST_F(ProgressBarTest, RoundCornerRetrieval) {
  // Setting custom corners should result in them being saved.
  bar()->SetPreferredHeight(12);
  bar()->SetPreferredCornerRadii(gfx::RoundedCornersF(6));
  views::test::RunScheduledLayout(container_view_);
  EXPECT_EQ(gfx::RoundedCornersF(6), bar()->GetPreferredCornerRadii());
}

// Test that `GetPreferredCornerRadii()` will return no corner with a radius
// greater than the height of the bar.
TEST_F(ProgressBarTest, RoundCornerMax) {
  // The max corner radius for a bar with a height of 12 should be 12.
  bar()->SetPreferredHeight(12);
  bar()->SetPreferredCornerRadii(gfx::RoundedCornersF(13, 14, 15, 16));
  views::test::RunScheduledLayout(container_view_);
  EXPECT_EQ(gfx::RoundedCornersF(12, 12, 12, 12),
            bar()->GetPreferredCornerRadii());
}

// Test that if value is set negative, which means progress bar is
// indeterminate, the string attribute value should be empty.
TEST_F(ProgressBarTest, RemoveValue) {
  // setting negative progress bar value
  bar()->SetValue(-0.626);

  ui::AXNodeData node_data;
  bar()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(std::string(""),
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));
}

}  // namespace views
