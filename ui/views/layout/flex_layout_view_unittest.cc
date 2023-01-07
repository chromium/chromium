// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/flex_layout_view.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_test_api.h"

namespace views {

class FlexLayoutViewTest : public testing::Test {
 public:
  void SetUp() override { host_ = std::make_unique<FlexLayoutView>(); }

  FlexLayoutView* host() { return host_.get(); }

 private:
  std::unique_ptr<FlexLayoutView> host_;
};

TEST_F(FlexLayoutViewTest, LayoutInvalidationWhenPropertyChanged) {
  ViewTestApi view_test_api(host());
  auto reset_layout = [&]() {
    EXPECT_TRUE(view_test_api.needs_layout());
    // Call layout() to set layout to a valid state.
    test::RunScheduledLayout(host());
  };

  // Ensure host() starts with a valid layout.
  test::RunScheduledLayout(host());

  EXPECT_NE(LayoutOrientation::kVertical, host()->GetOrientation());
  host()->SetOrientation(LayoutOrientation::kVertical);
  reset_layout();

  EXPECT_NE(LayoutAlignment::kEnd, host()->GetMainAxisAlignment());
  host()->SetMainAxisAlignment(LayoutAlignment::kEnd);
  reset_layout();

  EXPECT_NE(LayoutAlignment::kEnd, host()->GetCrossAxisAlignment());
  host()->SetCrossAxisAlignment(LayoutAlignment::kEnd);
  reset_layout();

  constexpr gfx::Insets interior_margin(10);
  EXPECT_NE(interior_margin, host()->GetInteriorMargin());
  host()->SetInteriorMargin(interior_margin);
  reset_layout();

  constexpr int min_cross_axis_size = 10;
  EXPECT_NE(min_cross_axis_size, host()->GetMinimumCrossAxisSize());
  host()->SetMinimumCrossAxisSize(min_cross_axis_size);
  reset_layout();

  constexpr bool collapse_margins = true;
  EXPECT_NE(collapse_margins, host()->GetCollapseMargins());
  host()->SetCollapseMargins(collapse_margins);
  reset_layout();

  constexpr bool include_host_insets_in_layout = true;
  EXPECT_NE(include_host_insets_in_layout,
            host()->GetIncludeHostInsetsInLayout());
  host()->SetIncludeHostInsetsInLayout(include_host_insets_in_layout);
  reset_layout();

  constexpr bool ignore_default_main_axis_margins = true;
  EXPECT_NE(ignore_default_main_axis_margins,
            host()->GetIgnoreDefaultMainAxisMargins());
  host()->SetIgnoreDefaultMainAxisMargins(ignore_default_main_axis_margins);
  reset_layout();

  constexpr FlexAllocationOrder flex_allocation_order =
      FlexAllocationOrder::kReverse;
  EXPECT_NE(flex_allocation_order, host()->GetFlexAllocationOrder());
  host()->SetFlexAllocationOrder(flex_allocation_order);
  reset_layout();
}

TEST_F(FlexLayoutViewTest, NoLayoutInvalidationWhenPropertyUnchanged) {
  ViewTestApi view_test_api(host());

  // Ensure view starts with a valid layout.
  test::RunScheduledLayout(host());
  host()->SetOrientation(host()->GetOrientation());
  host()->SetMainAxisAlignment(host()->GetMainAxisAlignment());
  host()->SetCrossAxisAlignment(host()->GetCrossAxisAlignment());
  host()->SetInteriorMargin(host()->GetInteriorMargin());
  host()->SetMinimumCrossAxisSize(host()->GetMinimumCrossAxisSize());
  host()->SetCollapseMargins(host()->GetCollapseMargins());
  host()->SetIncludeHostInsetsInLayout(host()->GetIncludeHostInsetsInLayout());
  host()->SetIgnoreDefaultMainAxisMargins(
      host()->GetIgnoreDefaultMainAxisMargins());
  host()->SetFlexAllocationOrder(host()->GetFlexAllocationOrder());
  EXPECT_FALSE(view_test_api.needs_layout());
}

}  // namespace views
