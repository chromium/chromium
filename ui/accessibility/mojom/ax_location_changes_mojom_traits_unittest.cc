// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_location_changes_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/accessibility/mojom/ax_location_changes.mojom.h"

using mojo::test::SerializeAndDeserialize;

TEST(AXLocationChangesMojomTraitsTest, RoundTrip) {
  ui::AXLocationChanges input;
  input.id = 1;
  ui::AXTreeID input_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  input.ax_tree_id = input_tree_id;

  ui::AXRelativeBounds input_bounds;
  input_bounds.offset_container_id = 111;
  input_bounds.bounds = gfx::RectF(1, 2, 3, 4);
  input_bounds.transform = std::make_unique<gfx::Transform>();
  input_bounds.transform->Scale(1.0, 2.0);
  input.new_location = input_bounds;

  ui::AXLocationChanges output;
  EXPECT_TRUE(
      SerializeAndDeserialize<ax::mojom::AXLocationChanges>(input, output));
  EXPECT_EQ(1, output.id);
  EXPECT_EQ(input_tree_id, output.ax_tree_id);
  EXPECT_EQ(111, output.new_location.offset_container_id);
  EXPECT_EQ(1, output.new_location.bounds.x());
  EXPECT_EQ(2, output.new_location.bounds.y());
  EXPECT_EQ(3, output.new_location.bounds.width());
  EXPECT_EQ(4, output.new_location.bounds.height());
  EXPECT_FALSE(output.new_location.transform->IsIdentity());
}
