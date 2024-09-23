// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/fill_layout.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/border.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

namespace {

class FillLayoutTest : public testing::Test {
 public:
  static constexpr int kDefaultHostWidth = 100;
  static constexpr int kDefaultHostHeight = 200;

  FillLayoutTest() : host_(std::make_unique<View>()) {
    host_->SetLayoutManager(std::make_unique<FillLayout>());
    SetHostSize(kDefaultHostWidth, kDefaultHostHeight);
  }

  FillLayoutTest(const FillLayoutTest&) = delete;
  FillLayoutTest& operator=(const FillLayoutTest&) = delete;

 protected:
  // Convenience function to get the preferred size from `layout()`.
  gfx::Size GetPreferredSize() {
    return layout()->GetPreferredSize(host_.get());
  }

  // Convenience function to get the preferred height for width from `layout()`.
  int GetPreferredHeightForWidth(int width) {
    return layout()->GetPreferredHeightForWidth(host_.get(), width);
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

  void SetHostInsets(const gfx::Insets& insets) {
    host_->SetBorder(CreateEmptyBorder(insets));
  }

  FillLayout* layout() {
    return static_cast<FillLayout*>(host_->GetLayoutManager());
  }

  std::unique_ptr<View> host_;
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
  SetHostInsets(
      gfx::Insets::TLBR(kTopInset, kLeftInset, kBottomInset, kRightInset));

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
  SetHostInsets(
      gfx::Insets::TLBR(kTopInset, kLeftInset, kBottomInset, kRightInset));

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
  test::RunScheduledLayout(host_.get());
  // Makes sure there is no crash.
}

TEST_F(FillLayoutTest, LayoutWithOneChild) {
  View* const child = AddChildView(25, 50);
  test::RunScheduledLayout(host_.get());

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
  SetHostInsets(
      gfx::Insets::TLBR(kTopInset, kLeftInset, kBottomInset, kRightInset));
  test::RunScheduledLayout(host_.get());

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

  test::RunScheduledLayout(host_.get());

  EXPECT_EQ(kExpectedBounds, child_1->bounds());
  EXPECT_EQ(kExpectedBounds, child_2->bounds());
  EXPECT_EQ(kExpectedBounds, child_3->bounds());
}

TEST_F(FillLayoutTest, LayoutIgnoreView) {
  View* const child_1 = AddChildView(10, 50);
  View* const child_2 = AddChildView(5, 5);
  View* const child_3 = AddChildView(25, 10);

  child_3->SetProperty(kViewIgnoredByLayoutKey, true);
  EXPECT_EQ(gfx::Size(10, 50), GetPreferredSize());
  test::RunScheduledLayout(host_.get());

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
  test::RunScheduledLayout(host_.get());

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
  EXPECT_EQ(host_->GetPreferredSize({}), host_->GetMinimumSize());
}

TEST_F(FillLayoutTest, MinimumSizeEnabled) {
  layout()->SetMinimumSizeEnabled(true);
  StaticSizedView* const child_1 = AddChildView(10, 50);
  StaticSizedView* const child_2 = AddChildView(5, 5);
  StaticSizedView* const child_3 = AddChildView(25, 10);
  child_1->set_minimum_size({1, 3});
  child_2->set_minimum_size({3, 1});
  child_3->set_minimum_size({2, 2});
  EXPECT_EQ(gfx::Size(3, 3), host_->GetMinimumSize());
}

TEST_F(FillLayoutTest, MinimumSizeExcludesView) {
  layout()->SetMinimumSizeEnabled(true);
  StaticSizedView* const child_1 = AddChildView(10, 50);
  StaticSizedView* const child_2 = AddChildView(5, 5);
  StaticSizedView* const child_3 = AddChildView(25, 10);
  child_1->set_minimum_size({1, 3});
  child_2->set_minimum_size({3, 1});
  child_3->set_minimum_size({2, 2});
  child_2->SetProperty(kViewIgnoredByLayoutKey, true);
  EXPECT_EQ(gfx::Size(2, 3), host_->GetMinimumSize());
}

}  // namespace views
