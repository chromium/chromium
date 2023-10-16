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
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_utils.h"

namespace views {

class ProgressBarTest : public ViewsTestBase {
 protected:
  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = CreateTestWidget();
    widget_->SetContentsView(std::make_unique<ProgressBar>());
    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  ProgressBar* bar() {
    return static_cast<ProgressBar*>(widget_->GetContentsView());
  }

  std::unique_ptr<Widget> widget_;
};

TEST_F(ProgressBarTest, AccessibleNodeData) {
  bar()->SetValue(0.626);

  ui::AXNodeData node_data;
  bar()->GetAccessibleNodeData(&node_data);
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
  EXPECT_EQ(absl::nullopt, bar()->GetForegroundColorId());
  EXPECT_EQ(absl::nullopt, bar()->GetBackgroundColorId());
}

}  // namespace views
