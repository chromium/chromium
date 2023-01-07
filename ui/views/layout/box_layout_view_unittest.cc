// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/box_layout_view.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_test_api.h"

namespace views {

class BoxLayoutViewTest : public testing::Test {
 public:
  void SetUp() override { host_ = std::make_unique<BoxLayoutView>(); }

  BoxLayoutView* host() { return host_.get(); }

 private:
  std::unique_ptr<BoxLayoutView> host_;
};

TEST_F(BoxLayoutViewTest, LayoutInvalidationWhenPropertyChanged) {
  ViewTestApi view_test_api(host());
  auto reset_layout = [&]() {
    EXPECT_TRUE(view_test_api.needs_layout());
    // Call layout() to set layout to a valid state.
    test::RunScheduledLayout(host());
  };

  // Ensure host() starts with a valid layout.
  test::RunScheduledLayout(host());

  EXPECT_FALSE(view_test_api.needs_layout());
  EXPECT_NE(BoxLayout::Orientation::kVertical, host()->GetOrientation());
  host()->SetOrientation(BoxLayout::Orientation::kVertical);
  reset_layout();

  EXPECT_FALSE(view_test_api.needs_layout());
  EXPECT_NE(BoxLayout::MainAxisAlignment::kEnd, host()->GetMainAxisAlignment());
  host()->SetMainAxisAlignment(BoxLayout::MainAxisAlignment::kEnd);
  reset_layout();

  EXPECT_FALSE(view_test_api.needs_layout());
  EXPECT_NE(BoxLayout::CrossAxisAlignment::kEnd,
            host()->GetCrossAxisAlignment());
  host()->SetCrossAxisAlignment(BoxLayout::CrossAxisAlignment::kEnd);
  reset_layout();

  EXPECT_FALSE(view_test_api.needs_layout());
  constexpr gfx::Insets inside_border_insets(10);
  EXPECT_NE(inside_border_insets, host()->GetInsideBorderInsets());
  host()->SetInsideBorderInsets(inside_border_insets);
  reset_layout();

  EXPECT_FALSE(view_test_api.needs_layout());
  constexpr int minimum_cross_axis_size = 10;
  EXPECT_NE(minimum_cross_axis_size, host()->GetMinimumCrossAxisSize());
  host()->SetMinimumCrossAxisSize(minimum_cross_axis_size);
  reset_layout();

  EXPECT_FALSE(view_test_api.needs_layout());
  constexpr int between_child_spacing = 10;
  EXPECT_NE(between_child_spacing, host()->GetBetweenChildSpacing());
  host()->SetBetweenChildSpacing(between_child_spacing);
  reset_layout();

  EXPECT_FALSE(view_test_api.needs_layout());
  constexpr bool collapse_margins_spacing = true;
  EXPECT_NE(collapse_margins_spacing, host()->GetCollapseMarginsSpacing());
  host()->SetCollapseMarginsSpacing(collapse_margins_spacing);
  reset_layout();

  EXPECT_FALSE(view_test_api.needs_layout());
  constexpr int default_flex = 10;
  EXPECT_NE(default_flex, host()->GetDefaultFlex());
  host()->SetDefaultFlex(default_flex);
  reset_layout();
}

TEST_F(BoxLayoutViewTest, NoLayoutInvalidationWhenPropertyUnchanged) {
  ViewTestApi view_test_api(host());

  // Ensure view starts with a valid layout.
  test::RunScheduledLayout(host());
  EXPECT_FALSE(view_test_api.needs_layout());
  host()->SetOrientation(host()->GetOrientation());
  host()->SetMainAxisAlignment(host()->GetMainAxisAlignment());
  host()->SetCrossAxisAlignment(host()->GetCrossAxisAlignment());
  host()->SetInsideBorderInsets(host()->GetInsideBorderInsets());
  host()->SetMinimumCrossAxisSize(host()->GetMinimumCrossAxisSize());
  host()->SetBetweenChildSpacing(host()->GetBetweenChildSpacing());
  host()->SetCollapseMarginsSpacing(host()->GetCollapseMarginsSpacing());
  host()->SetDefaultFlex(host()->GetDefaultFlex());
  EXPECT_FALSE(view_test_api.needs_layout());
}

}  // namespace views
