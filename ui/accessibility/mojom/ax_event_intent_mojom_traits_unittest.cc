// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_event_intent_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event_intent.h"
#include "ui/accessibility/mojom/ax_event_intent.mojom.h"

using mojo::test::SerializeAndDeserialize;

TEST(AXEventIntentMojomTraitsTest, RoundTripWithEditingIntent) {
  ui::AXEventIntent input;
  input.command = ax::mojom::Command::kInsert;
  input.input_event_type = ax::mojom::InputEventType::kInsertLineBreak;

  ui::AXEventIntent output;
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::EventIntent>(input, output));
  EXPECT_EQ(ax::mojom::Command::kInsert, output.command);
  EXPECT_EQ(ax::mojom::InputEventType::kInsertLineBreak,
            output.input_event_type);
  EXPECT_EQ(ax::mojom::TextBoundary::kNone, output.text_boundary);
  EXPECT_EQ(ax::mojom::MoveDirection::kNone, output.move_direction);
}

TEST(AXEventIntentMojomTraitsTest, RoundTripWithSelectionIntent) {
  ui::AXEventIntent input;
  input.command = ax::mojom::Command::kMoveSelection;
  input.text_boundary = ax::mojom::TextBoundary::kWordEnd;
  input.move_direction = ax::mojom::MoveDirection::kForward;

  ui::AXEventIntent output;
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::EventIntent>(input, output));
  EXPECT_EQ(ax::mojom::Command::kMoveSelection, output.command);
  EXPECT_EQ(ax::mojom::InputEventType::kNone, output.input_event_type);
  EXPECT_EQ(ax::mojom::TextBoundary::kWordEnd, output.text_boundary);
  EXPECT_EQ(ax::mojom::MoveDirection::kForward, output.move_direction);
}
