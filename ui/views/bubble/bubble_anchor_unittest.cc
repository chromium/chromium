// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_anchor.h"

#include <memory>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget_utils.h"

namespace views {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kButtonElementId);

class BubbleAnchorTest : public ViewsTestBase {
 public:
  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->Show();

    button_ = widget_->SetContentsView(std::make_unique<views::LabelButton>(
        views::Button::PressedCallback(), std::u16string()));
    button_->SetProperty(views::kElementIdentifierKey, kButtonElementId);
    button_->SetVisible(true);
  }

  void TearDown() override {
    button_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::LabelButton> button_ = nullptr;
};

TEST_F(BubbleAnchorTest, Default) {
  BubbleAnchor anchor;
  EXPECT_TRUE(anchor.IsNull());
  EXPECT_EQ(nullptr, anchor.GetIfView());
  EXPECT_EQ(nullptr, anchor.GetIfElement());
}

TEST_F(BubbleAnchorTest, View) {
  BubbleAnchor anchor(button_);
  EXPECT_FALSE(anchor.IsNull());
  EXPECT_EQ(button_, anchor.GetIfView());
  EXPECT_EQ(nullptr, anchor.GetIfElement());
}

TEST_F(BubbleAnchorTest, NullView) {
  views::View* null_view = nullptr;
  BubbleAnchor anchor(null_view);
  EXPECT_TRUE(anchor.IsNull());
  EXPECT_EQ(nullptr, anchor.GetIfView());
  EXPECT_EQ(nullptr, anchor.GetIfElement());
}

TEST_F(BubbleAnchorTest, TrackedElement) {
  ui::TrackedElement* button_element =
      views::ElementTrackerViews::GetInstance()->GetElementForView(button_);
  ASSERT_TRUE(button_element);
  BubbleAnchor anchor(button_element);
  EXPECT_FALSE(anchor.IsNull());
  EXPECT_EQ(nullptr, anchor.GetIfView());
  EXPECT_EQ(button_element, anchor.GetIfElement());
}

TEST_F(BubbleAnchorTest, NullTrackedElement) {
  ui::TrackedElement* null_element = nullptr;
  BubbleAnchor anchor(null_element);
  EXPECT_TRUE(anchor.IsNull());
  EXPECT_EQ(nullptr, anchor.GetIfView());
  EXPECT_EQ(nullptr, anchor.GetIfElement());
}

}  // namespace views
