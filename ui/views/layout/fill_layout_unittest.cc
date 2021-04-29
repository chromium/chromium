// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/fill_layout.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/border.h"
#include "ui/views/test/test_views.h"
#include "ui/views/view.h"

namespace views {

namespace {

class FillLayoutTest : public testing::Test {
 public:
  static const int kDefaultHostWidth = 100;
  static const int kDefaultHostHeight = 200;

  FillLayoutTest() : host_(new View) {
    layout_ = host_->SetLayoutManager(std::make_unique<FillLayout>());
    SetHostSize(kDefaultHostWidth, kDefaultHostHeight);
  }

 protected:
  // Convenience function to get the preferred size from |layout_|.
  gfx::Size GetPreferredSize() const {
    return layout_->GetPreferredSize(host_.get());
  }

  // Convenience function to get the preferred height for width from |layout_|.
  int GetPreferredHeightForWidth(int width) const {
    return layout_->GetPreferredHeightForWidth(host_.get(), width);
  }

  // Creates a View with the given |width| and |height| and adds it to |host_|.
  StaticSizedView* AddChildView(int width, int height) {
    StaticSizedView* child_view = new StaticSizedView(gfx::Size(width, height));
    child_view->SizeToPreferredSize();
    host_->AddChildView(child_view);
    return child_view;
  }

  void SetHostSize(int width, int height) {
    host_->SetSize(gfx::Size(width, height));
  }

  void SetHostInsets(int top, int left, int bottom, int right) {
    host_->SetBorder(CreateEmptyBorder(gfx::Insets(top, left, bottom, right)));
  }

  // The test target.
  FillLayout* layout_ = nullptr;

  std::unique_ptr<View> host_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FillLayoutTest);
};

}  // namespace

TEST_F(FillLayoutTest, GetPreferredSizeWithNoChildren) {
  EXPECT_EQ(gfx::Size(0, 0), GetPreferredSize());

  SetHostSize(0, 0);
  EXPECT_EQ(gfx::Size(0, 0), GetPreferredSize());
}

TEST_F(FillLayoutTest, GetPreferredSizeWithOneChild) {
  AddChildView(25, 50);
  EXPECT_EQ(gfx::Size(25, 50), GetPreferredSize());
}

TEST_F(FillLayoutTest, GetPreferredSizeWithInsets) {
  const int kChildWidth = 25;
  const int kChildHeight = 50;
  const int kTopInset = 2;
  const int kLeftInset = 3;
  const int kBottomInset = 8;
  const int kRightInset = 7;

  AddChildView(kChildWidth, kChildHeight);
  SetHostInsets(kTopInset, kLeftInset, kBottomInset, kRightInset);

  EXPECT_EQ(gfx::Size(kChildWidth + kLeftInset + kRightInset,
                      kChildHeight + kTopInset + kBottomInset),
            GetPreferredSize());
}

TEST_F(FillLayoutTest, GetPreferredSizeWithMultipleChildren) {
  AddChildView(10, 50);
  AddChildView(5, 5);
  AddChildView(25, 10);

  EXPECT_EQ(gfx::Size(25, 50), GetPreferredSize());
}

TEST_F(FillLayoutTest, GetPreferredHeightForWidthWithNoChildren) {
  EXPECT_EQ(0, GetPreferredHeightForWidth(0));
  EXPECT_EQ(0, GetPreferredHeightForWidth(100));

  SetHostSize(0, 0);
  EXPECT_EQ(0, GetPreferredHeightForWidth(0));
  EXPECT_EQ(0, GetPreferredHeightForWidth(100));
}

TEST_F(FillLayoutTest, GetPreferredHeightForWidthWithOneChild) {
  AddChildView(25, 50);

  EXPECT_EQ(50, GetPreferredHeightForWidth(0));
  EXPECT_EQ(50, GetPreferredHeightForWidth(25));
  EXPECT_EQ(50, GetPreferredHeightForWidth(100));
}

TEST_F(FillLayoutTest, GetPreferredHeightForWidthWithInsets) {
  const int kChildWidth = 25;
  const int kChildHeight = 50;
  const int kTopInset = 2;
  const int kLeftInset = 3;
  const int kBottomInset = 8;
  const int kRightInset = 7;

  const int kExpectedHeight = kChildHeight + kTopInset + kBottomInset;

  AddChildView(kChildWidth, kChildHeight);
  SetHostInsets(kTopInset, kLeftInset, kBottomInset, kRightInset);

  EXPECT_EQ(kExpectedHeight, GetPreferredHeightForWidth(0));
  EXPECT_EQ(kExpectedHeight, GetPreferredHeightForWidth(25));
  EXPECT_EQ(kExpectedHeight, GetPreferredHeightForWidth(100));
}

TEST_F(FillLayoutTest, GetPreferredHeightForWidthWithMultipleChildren) {
  AddChildView(10, 50);
  AddChildView(5, 5);
  AddChildView(25, 10);

  EXPECT_EQ(50, GetPreferredHeightForWidth(0));
  EXPECT_EQ(50, GetPreferredHeightForWidth(25));
  EXPECT_EQ(50, GetPreferredHeightForWidth(100));
}

TEST_F(FillLayoutTest, LayoutWithNoChildren) {
  host_->Layout();
  // Makes sure there is no crash.
}

TEST_F(FillLayoutTest, LayoutWithOneChild) {
  View* const child = AddChildView(25, 50);
  host_->Layout();

  EXPECT_EQ(gfx::Rect(0, 0, kDefaultHostWidth, kDefaultHostHeight),
            child->bounds());
}

TEST_F(FillLayoutTest, LayoutWithInsets) {
  const int kChildWidth = 25;
  const int kChildHeight = 50;
  const int kTopInset = 2;
  const int kLeftInset = 3;
  const int kBottomInset = 8;
  const int kRightInset = 7;

  View* const child = AddChildView(kChildWidth, kChildHeight);
  SetHostInsets(kTopInset, kLeftInset, kBottomInset, kRightInset);
  host_->Layout();

  EXPECT_EQ(gfx::Rect(kLeftInset, kTopInset,
                      kDefaultHostWidth - kLeftInset - kRightInset,
                      kDefaultHostHeight - kTopInset - kBottomInset),
            child->bounds());
}

TEST_F(FillLayoutTest, LayoutMultipleChildren) {
  View* const child_1 = AddChildView(10, 50);
  View* const child_2 = AddChildView(5, 5);
  View* const child_3 = AddChildView(25, 10);

  const gfx::Rect kExpectedBounds(0, 0, kDefaultHostWidth, kDefaultHostHeight);

  host_->Layout();

  EXPECT_EQ(kExpectedBounds, child_1->bounds());
  EXPECT_EQ(kExpectedBounds, child_2->bounds());
  EXPECT_EQ(kExpectedBounds, child_3->bounds());
}

TEST_F(FillLayoutTest, LayoutIgnoreView) {
  View* const child_1 = AddChildView(10, 50);
  View* const child_2 = AddChildView(5, 5);
  View* const child_3 = AddChildView(25, 10);

  layout_->SetChildViewIgnoredByLayout(child_3, true);
  EXPECT_EQ(gfx::Size(10, 50), GetPreferredSize());
  host_->Layout();

  const gfx::Size kExpectedSize(kDefaultHostWidth, kDefaultHostHeight);
  EXPECT_EQ(kExpectedSize, child_1->size());
  EXPECT_EQ(kExpectedSize, child_2->size());
  EXPECT_EQ(gfx::Size(25, 10), child_3->size());
}

TEST_F(FillLayoutTest, LayoutIgnoresHiddenView) {
  View* const child_1 = AddChildView(10, 50);
  View* const child_2 = AddChildView(5, 5);
  View* const child_3 = AddChildView(25, 10);
  child_3->SetVisible(false);
  layout_->SetIncludeHiddenViews(false);

  EXPECT_EQ(gfx::Size(10, 50), GetPreferredSize());
  host_->Layout();

  const gfx::Size kExpectedSize(kDefaultHostWidth, kDefaultHostHeight);
  EXPECT_EQ(kExpectedSize, child_1->size());
  EXPECT_EQ(kExpectedSize, child_2->size());
  EXPECT_EQ(gfx::Size(25, 10), child_3->size());
}

TEST_F(FillLayoutTest, LayoutIncludesHiddenView) {
  View* const child_1 = AddChildView(10, 50);
  View* const child_2 = AddChildView(5, 5);
  View* const child_3 = AddChildView(25, 10);
  child_3->SetVisible(false);

  EXPECT_EQ(gfx::Size(25, 50), GetPreferredSize());
  host_->Layout();

  const gfx::Size kExpectedSize(kDefaultHostWidth, kDefaultHostHeight);
  EXPECT_EQ(kExpectedSize, child_1->size());
  EXPECT_EQ(kExpectedSize, child_2->size());
  EXPECT_EQ(kExpectedSize, child_3->size());
}

TEST_F(FillLayoutTest, MinimumSizeDisabled) {
  StaticSizedView* const child_1 = AddChildView(10, 50);
  StaticSizedView* const child_2 = AddChildView(5, 5);
  StaticSizedView* const child_3 = AddChildView(25, 10);
  child_1->set_minimum_size({1, 3});
  child_2->set_minimum_size({3, 1});
  child_3->set_minimum_size({2, 2});
  EXPECT_EQ(host_->GetPreferredSize(), host_->GetMinimumSize());
}

TEST_F(FillLayoutTest, MinimumSizeEnabled) {
  layout_->SetMinimumSizeEnabled(true);
  StaticSizedView* const child_1 = AddChildView(10, 50);
  StaticSizedView* const child_2 = AddChildView(5, 5);
  StaticSizedView* const child_3 = AddChildView(25, 10);
  child_1->set_minimum_size({1, 3});
  child_2->set_minimum_size({3, 1});
  child_3->set_minimum_size({2, 2});
  EXPECT_EQ(gfx::Size(3, 3), host_->GetMinimumSize());
}

TEST_F(FillLayoutTest, MinimumSizeExcludesView) {
  layout_->SetMinimumSizeEnabled(true);
  StaticSizedView* const child_1 = AddChildView(10, 50);
  StaticSizedView* const child_2 = AddChildView(5, 5);
  StaticSizedView* const child_3 = AddChildView(25, 10);
  child_1->set_minimum_size({1, 3});
  child_2->set_minimum_size({3, 1});
  child_3->set_minimum_size({2, 2});
  layout_->SetChildViewIgnoredByLayout(child_2, true);
  EXPECT_EQ(gfx::Size(2, 3), host_->GetMinimumSize());
}

}  // namespace views
