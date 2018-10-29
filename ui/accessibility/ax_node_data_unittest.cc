// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ui {

TEST(AXNodeDataTest, GetAndSetCheckedState) {
  AXNodeData root;
  EXPECT_EQ(ax::mojom::CheckedState::kNone, root.GetCheckedState());
  EXPECT_FALSE(root.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState));

  root.SetCheckedState(ax::mojom::CheckedState::kMixed);
  EXPECT_EQ(ax::mojom::CheckedState::kMixed, root.GetCheckedState());
  EXPECT_TRUE(root.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState));

  root.SetCheckedState(ax::mojom::CheckedState::kFalse);
  EXPECT_EQ(ax::mojom::CheckedState::kFalse, root.GetCheckedState());
  EXPECT_TRUE(root.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState));

  root.SetCheckedState(ax::mojom::CheckedState::kNone);
  EXPECT_EQ(ax::mojom::CheckedState::kNone, root.GetCheckedState());
  EXPECT_FALSE(root.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState));
}

}  // namespace ui
