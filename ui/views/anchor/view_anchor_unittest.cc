// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/anchor/view_anchor.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views::test {

class ViewAnchorTest : public ViewsTestBase {
 public:
  ViewAnchorTest() = default;
  ViewAnchorTest(const ViewAnchorTest&) = delete;
  ViewAnchorTest& operator=(const ViewAnchorTest&) = delete;
  ~ViewAnchorTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
    view_ = widget_->SetContentsView(std::make_unique<View>());
    view_->SetBounds(10, 20, 30, 40);
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<Widget> widget_;
  raw_ptr<View> view_ = nullptr;
};

TEST_F(ViewAnchorTest, BasicProperties) {
  ui::Anchor anchor(view_);
  EXPECT_FALSE(anchor.IsEmpty());
  EXPECT_TRUE(anchor.IsView());
  EXPECT_EQ(anchor.GetView(), view_);
}

TEST_F(ViewAnchorTest, GetScreenBounds) {
  ui::Anchor anchor(view_);
  EXPECT_EQ(anchor.GetScreenBounds(), view_->GetBoundsInScreen());
}

TEST_F(ViewAnchorTest, ViewDeleted) {
  ui::Anchor anchor(view_);
  EXPECT_FALSE(anchor.IsEmpty());
  EXPECT_TRUE(anchor.IsView());

  // Delete the view. The anchor should become empty.
  view_.ClearAndDelete();

  EXPECT_TRUE(anchor.IsEmpty());
  EXPECT_TRUE(anchor.IsView());
  EXPECT_EQ(anchor.GetView(), nullptr);
  EXPECT_EQ(anchor.GetScreenBounds(), gfx::Rect());
}

TEST_F(ViewAnchorTest, WidgetDestroyed) {
  ui::Anchor anchor(view_);
  EXPECT_FALSE(anchor.IsEmpty());
  EXPECT_TRUE(anchor.IsView());

  // Destroy the widget. This will also destroy the view.
  view_ = nullptr;
  widget_.reset();

  EXPECT_TRUE(anchor.IsEmpty());
  EXPECT_TRUE(anchor.IsView());
  EXPECT_EQ(anchor.GetView(), nullptr);
  EXPECT_EQ(anchor.GetScreenBounds(), gfx::Rect());
}

}  // namespace views::test
