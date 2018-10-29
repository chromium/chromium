// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_tree_id_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/mojom/ax_tree_id.mojom.h"

using mojo::test::SerializeAndDeserialize;

TEST(AXTreeIDMojomTraitsTest, TestSerializeAndDeserializeAXTreeID) {
  ui::AXTreeID input = ui::AXTreeID::FromString("abc");
  ui::AXTreeID output;
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXTreeID>(&input, &output));
  EXPECT_EQ("abc", output.ToString());
}
