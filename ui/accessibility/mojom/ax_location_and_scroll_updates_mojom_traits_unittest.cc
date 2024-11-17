// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_location_and_scroll_updates_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/accessibility/mojom/ax_location_and_scroll_updates.mojom.h"

using mojo::test::SerializeAndDeserialize;

TEST(AXLocationAndScrollUpdatesMojomTraitsTest, LocationChangeRoundTrip) {
  ui::AXRelativeBounds input_bounds;
  input_bounds.offset_container_id = 111;
  input_bounds.bounds = gfx::RectF(1, 2, 3, 4);
  input_bounds.transform = std::make_unique<gfx::Transform>();
  input_bounds.transform->Scale(1.0, 2.0);
  ui::AXLocationChange input(1, input_bounds);

  ui::AXLocationChange output;
  EXPECT_TRUE(
      SerializeAndDeserialize<ax::mojom::AXLocationChange>(input, output));
  EXPECT_EQ(1, output.id);
  EXPECT_EQ(111, output.new_location.offset_container_id);
  EXPECT_EQ(1, output.new_location.bounds.x());
  EXPECT_EQ(2, output.new_location.bounds.y());
  EXPECT_EQ(3, output.new_location.bounds.width());
  EXPECT_EQ(4, output.new_location.bounds.height());
  EXPECT_FALSE(output.new_location.transform->IsIdentity());
}

TEST(AXLocationAndScrollUpdatesMojomTraitsTest, ScrollChangeRoundTrip) {
  ui::AXScrollChange input(7, 15, 20);

  ui::AXScrollChange output;
  EXPECT_TRUE(
      SerializeAndDeserialize<ax::mojom::AXScrollChange>(input, output));
  EXPECT_EQ(7, output.id);
  EXPECT_EQ(15, output.scroll_x);
  EXPECT_EQ(20, output.scroll_y);
}
