// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_id.h"

#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(AXTreeIDTest, ToStringFromString) {
  AXTreeID tree_id = AXTreeID::CreateNewAXTreeID();
  AXTreeID new_tree_id = AXTreeID::FromString(tree_id.ToString());
  ASSERT_EQ(tree_id, new_tree_id);
}

TEST(AXTreeIDTest, ToTokenFromToken) {
  AXTreeID tree_id = AXTreeID::CreateNewAXTreeID();
  AXTreeID new_tree_id = AXTreeID::FromToken(tree_id.token().value());
  ASSERT_EQ(tree_id, new_tree_id);
}

}  // namespace ui
