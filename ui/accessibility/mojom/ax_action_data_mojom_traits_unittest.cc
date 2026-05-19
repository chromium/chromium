// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_action_data_mojom_traits.h"

#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/mojom/ax_action_data.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

using mojo::test::SerializeAndDeserialize;

TEST(AXActionDataMojomTraitsTest, RoundTrip) {
  ui::AXActionData input;
  input.action = ax::mojom::Action::kBlur;
  input.target_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  EXPECT_EQ(32U, input.target_tree_id.ToString().size());
  input.child_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  EXPECT_EQ(32U, input.child_tree_id.ToString().size());
  input.source_extension_id = "extension_id";
  input.target_node_id = 2;
  input.request_id = 3;
  input.anchor_node_id = 4;
  input.anchor_offset = 5;
  input.focus_node_id = 6;
  input.focus_offset = 7;
  input.custom_action_id = 8;
  input.target_rect = gfx::Rect(9, 10, 11, 12);
  input.target_point = gfx::Point(13, 14);
  input.value = "value";
  input.hit_test_event_to_fire = ax::mojom::Event::kFocus;
  input.AddIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets,
                            {16, 17, 18});
  input.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts, {19, 20});
  input.AddStringListAttribute(
      ax::mojom::StringListAttribute::kTextOperationReplacementStrings,
      {"first", "second", "third"});
  input.AddStringListAttribute(
      ax::mojom::StringListAttribute::kAriaNotificationAnnouncements,
      {"announcement"});

  ui::AXActionData output;
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXActionData>(input, output));

  EXPECT_EQ(output.action, ax::mojom::Action::kBlur);
  EXPECT_EQ(output.target_tree_id, input.target_tree_id);
  EXPECT_EQ(output.child_tree_id, input.child_tree_id);
  EXPECT_EQ(output.target_tree_id.ToString(), input.target_tree_id.ToString());
  EXPECT_EQ(output.child_tree_id.ToString(), input.child_tree_id.ToString());
  EXPECT_EQ(output.source_extension_id, "extension_id");
  EXPECT_EQ(output.target_node_id, 2);
  EXPECT_EQ(output.request_id, 3);
  EXPECT_EQ(output.anchor_node_id, 4);
  EXPECT_EQ(output.anchor_offset, 5);
  EXPECT_EQ(output.focus_node_id, 6);
  EXPECT_EQ(output.focus_offset, 7);
  EXPECT_EQ(output.custom_action_id, 8);
  EXPECT_EQ(output.target_rect, gfx::Rect(9, 10, 11, 12));
  EXPECT_EQ(output.target_point, gfx::Point(13, 14));
  EXPECT_EQ(output.value, "value");
  EXPECT_EQ(output.hit_test_event_to_fire, ax::mojom::Event::kFocus);
  EXPECT_EQ(output.GetIntListAttribute(
                ax::mojom::IntListAttribute::kCharacterOffsets),
            std::vector<int32_t>({16, 17, 18}));
  EXPECT_EQ(
      output.GetIntListAttribute(ax::mojom::IntListAttribute::kWordStarts),
      std::vector<int32_t>({19, 20}));
  EXPECT_EQ(
      output.GetStringListAttribute(
          ax::mojom::StringListAttribute::kTextOperationReplacementStrings),
      std::vector<std::string>({"first", "second", "third"}));
  EXPECT_EQ(output.GetStringListAttribute(
                ax::mojom::StringListAttribute::kAriaNotificationAnnouncements),
            std::vector<std::string>({"announcement"}));
}

TEST(AXActionDataMojomTraitsTest, IntListAttributes) {
  ui::AXActionData data;

  // Initially, we must not have an int list for any attribute.
  EXPECT_FALSE(
      data.HasIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets));
  data.AddIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets,
                           {1, 2, 3});
  EXPECT_TRUE(
      data.HasIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets));

  // Only a previously set attribute should return true for HasIntListAttribute.
  EXPECT_FALSE(
      data.HasIntListAttribute(ax::mojom::IntListAttribute::kWordStarts));
  data.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts, {4, 5});
  EXPECT_TRUE(
      data.HasIntListAttribute(ax::mojom::IntListAttribute::kWordStarts));

  // GetIntListAttribute returns the expected value for each of the attributes.
  EXPECT_EQ(
      data.GetIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets),
      std::vector<int32_t>({1, 2, 3}));
  EXPECT_EQ(data.GetIntListAttribute(ax::mojom::IntListAttribute::kWordStarts),
            std::vector<int32_t>({4, 5}));

  // RemoveIntListAttribute should only remove the value for the provided
  // attribute.
  data.RemoveIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets);
  EXPECT_FALSE(
      data.HasIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets));
  EXPECT_TRUE(
      data.HasIntListAttribute(ax::mojom::IntListAttribute::kWordStarts));
}

TEST(AXActionDataMojomTraitsTest, StringListAttributes) {
  ui::AXActionData data;

  // Initially, we must not have a string list for any attribute.
  EXPECT_FALSE(data.HasStringListAttribute(
      ax::mojom::StringListAttribute::kTextOperationReplacementStrings));
  data.AddStringListAttribute(
      ax::mojom::StringListAttribute::kTextOperationReplacementStrings,
      {"first", "second", "third"});
  EXPECT_TRUE(data.HasStringListAttribute(
      ax::mojom::StringListAttribute::kTextOperationReplacementStrings));

  // Only a previously set attribute should return true for
  // HasStringListAttribute.
  EXPECT_FALSE(data.HasStringListAttribute(
      ax::mojom::StringListAttribute::kAriaNotificationAnnouncements));
  data.AddStringListAttribute(
      ax::mojom::StringListAttribute::kAriaNotificationAnnouncements,
      {"announcement"});
  EXPECT_TRUE(data.HasStringListAttribute(
      ax::mojom::StringListAttribute::kAriaNotificationAnnouncements));

  // GetStringListAttribute returns the expected value for each of the
  // attributes.
  EXPECT_EQ(
      data.GetStringListAttribute(
          ax::mojom::StringListAttribute::kTextOperationReplacementStrings),
      std::vector<std::string>({"first", "second", "third"}));
  EXPECT_EQ(data.GetStringListAttribute(
                ax::mojom::StringListAttribute::kAriaNotificationAnnouncements),
            std::vector<std::string>({"announcement"}));

  // RemoveStringListAttribute should only remove the value for the provided
  // attribute.
  data.RemoveStringListAttribute(
      ax::mojom::StringListAttribute::kTextOperationReplacementStrings);
  EXPECT_FALSE(data.HasStringListAttribute(
      ax::mojom::StringListAttribute::kTextOperationReplacementStrings));
  EXPECT_TRUE(data.HasStringListAttribute(
      ax::mojom::StringListAttribute::kAriaNotificationAnnouncements));
}
