// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_update_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

TEST(AXTreeUpdateUtilTest, AXTreeUpdatesCanBeMerged_NodeIDToClear) {
  AXTreeUpdate u1;
  AXTreeUpdate u2;

  // Both updates have an invalid node ID to clear.
  u1.node_id_to_clear = kInvalidAXNodeID;
  u2.node_id_to_clear = kInvalidAXNodeID;
  EXPECT_TRUE(AXTreeUpdatesCanBeMerged(u1, u2));

  // Update 1 has a node ID to clear.
  u1.node_id_to_clear = 1;
  u2.node_id_to_clear = kInvalidAXNodeID;
  EXPECT_TRUE(AXTreeUpdatesCanBeMerged(u1, u2));

  // Update 2 has a node ID to clear.
  u1.node_id_to_clear = kInvalidAXNodeID;
  u2.node_id_to_clear = 2;
  EXPECT_FALSE(AXTreeUpdatesCanBeMerged(u1, u2));
}

TEST(AXTreeUpdateUtilTest, AXTreeUpdatesCanBeMerged_TreeData) {
  AXTreeUpdate u1;
  AXTreeUpdate u2;

  // Check when no tree data has been set.
  u1.has_tree_data = false;
  u2.has_tree_data = false;
  EXPECT_TRUE(AXTreeUpdatesCanBeMerged(u1, u2));

  u1.has_tree_data = true;
  u2.has_tree_data = false;
  EXPECT_TRUE(AXTreeUpdatesCanBeMerged(u1, u2));

  u1.has_tree_data = false;
  u2.has_tree_data = true;
  EXPECT_TRUE(AXTreeUpdatesCanBeMerged(u1, u2));

  u1.has_tree_data = true;
  u2.has_tree_data = true;
  EXPECT_TRUE(AXTreeUpdatesCanBeMerged(u1, u2));

  // Check when both updates have identical tree data.
  AXTreeData d1;
  d1.tree_id = AXTreeID::CreateNewAXTreeID();
  u1.tree_data = d1;
  u2.tree_data = d1;

  u1.has_tree_data = false;
  u2.has_tree_data = false;
  EXPECT_TRUE(AXTreeUpdatesCanBeMerged(u1, u2));

  u1.has_tree_data = true;
  u2.has_tree_data = false;
  EXPECT_TRUE(AXTreeUpdatesCanBeMerged(u1, u2));

  u1.has_tree_data = false;
  u2.has_tree_data = true;
  EXPECT_TRUE(AXTreeUpdatesCanBeMerged(u1, u2));

  u1.has_tree_data = true;
  u2.has_tree_data = true;
  EXPECT_TRUE(AXTreeUpdatesCanBeMerged(u1, u2));

  // Check when the updates have different tree data.
  AXTreeData d2;
  d2.tree_id = AXTreeID::CreateNewAXTreeID();
  u1.tree_data = d1;
  u2.tree_data = d2;

  u1.has_tree_data = false;
  u2.has_tree_data = false;
  EXPECT_TRUE(AXTreeUpdatesCanBeMerged(u1, u2));

  u1.has_tree_data = true;
  u2.has_tree_data = false;
  EXPECT_TRUE(AXTreeUpdatesCanBeMerged(u1, u2));

  u1.has_tree_data = false;
  u2.has_tree_data = true;
  EXPECT_FALSE(AXTreeUpdatesCanBeMerged(u1, u2));

  u1.has_tree_data = true;
  u2.has_tree_data = true;
  EXPECT_FALSE(AXTreeUpdatesCanBeMerged(u1, u2));
}

TEST(AXTreeUpdateUtilTest, AXTreeUpdatesCanBeMerged_RootID) {
  AXTreeUpdate u1;
  AXTreeUpdate u2;

  // Same root id.
  u1.root_id = 1;
  u2.root_id = 1;
  EXPECT_TRUE(AXTreeUpdatesCanBeMerged(u1, u2));

  // Different root id.
  u1.root_id = 1;
  u2.root_id = 2;
  EXPECT_FALSE(AXTreeUpdatesCanBeMerged(u1, u2));

  // Invalid root id.
  u1.root_id = kInvalidAXNodeID;
  u2.root_id = kInvalidAXNodeID;
  EXPECT_TRUE(AXTreeUpdatesCanBeMerged(u1, u2));
}

TEST(AXTreeUpdateUtilTest, MergeAXTreeUpdates) {
  std::vector<AXTreeUpdate> src;
  for (int i = 0; i < 10; i++) {
    AXNodeData node;
    node.id = i;
    AXTreeUpdate u;
    u.nodes.push_back(node);
    src.push_back(u);
  }

  std::vector<AXTreeUpdate> dst;
  EXPECT_TRUE(MergeAXTreeUpdates(src, &dst));
  EXPECT_EQ(1u, dst.size());
  EXPECT_EQ(10u, dst[0].nodes.size());
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(i, dst[0].nodes[i].id);
  }
}

TEST(AXTreeUpdateUtilTest, MergeAXTreeUpdates_CannotBeMerged) {
  std::vector<AXTreeUpdate> src;
  for (int i = 0; i < 10; i++) {
    AXTreeUpdate u;
    u.root_id = i;
    src.push_back(u);
  }

  std::vector<AXTreeUpdate> dst;
  EXPECT_FALSE(MergeAXTreeUpdates(src, &dst));
  EXPECT_TRUE(dst.empty());
}

TEST(AXTreeUpdateUtilTest, MergeAXTreeUpdates_NoConsecutiveMergeableUpdates) {
  // Alternate updates, where the odd ones cannot be merged, while the even ones
  // can be merged.
  std::vector<AXTreeUpdate> src;
  for (int i = 0; i < 5; i++) {
    // U1 cannot be merged.
    AXTreeUpdate u1;
    u1.root_id = i * 2;
    src.push_back(u1);

    // U2 can be merged.
    AXNodeData node;
    node.id = i * 2 + 1;
    AXTreeUpdate u2;
    u2.nodes.push_back(node);
    src.push_back(u2);
  }

  // Since no consecutive updates can be merged, none are merged. Note that this
  // might not be the desired behavior, but this is how the function is written
  // now.
  std::vector<AXTreeUpdate> dst;
  EXPECT_FALSE(MergeAXTreeUpdates(src, &dst));
  EXPECT_TRUE(dst.empty());
}

TEST(AXTreeUpdateUtilTest, MergeAXTreeUpdates_SomeCanBeMerged) {
  std::vector<AXTreeUpdate> src;
  // The first half updates cannot be merged.
  for (int i = 0; i < 5; i++) {
    AXNodeData node;
    node.id = i;
    AXTreeUpdate u;
    u.root_id = i;
    u.nodes.push_back(node);
    src.push_back(u);
  }

  // The second half updates can be merged.
  for (int i = 5; i < 10; i++) {
    AXNodeData node;
    node.id = i;
    AXTreeUpdate u;
    u.nodes.push_back(node);
    src.push_back(u);
  }

  // The first five updates are not merged and the last five are.
  std::vector<AXTreeUpdate> dst;
  EXPECT_TRUE(MergeAXTreeUpdates(src, &dst));
  EXPECT_EQ(6u, dst.size());
  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(1u, dst[i].nodes.size());
    EXPECT_EQ(i, dst[i].nodes[0].id);
  }
  EXPECT_EQ(5u, dst[5].nodes.size());
  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(i + 5, dst[5].nodes[i].id);
  }
}

TEST(AXTreeUpdateUtilTest, MergeAXTreeUpdates_AtLeastTwoMerges) {
  std::vector<AXTreeUpdate> src;
  for (int i = 0; i < 2; i++) {
    AXNodeData node;
    node.id = i;
    AXTreeUpdate u;
    u.nodes.push_back(node);
    src.push_back(u);
  }

  // If there are fewer than 2 merges, the merge fails.
  std::vector<AXTreeUpdate> dst;
  EXPECT_FALSE(MergeAXTreeUpdates(src, &dst));
  EXPECT_TRUE(dst.empty());

  AXNodeData node;
  node.id = 2;
  AXTreeUpdate u;
  u.nodes.push_back(node);
  src.push_back(u);

  // If there are more than 2 merges, the merge succeeds.
  EXPECT_TRUE(MergeAXTreeUpdates(src, &dst));
  EXPECT_EQ(1u, dst.size());
  EXPECT_EQ(3u, dst[0].nodes.size());
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(i, dst[0].nodes[i].id);
  }
}

}  // namespace ui
