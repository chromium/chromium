// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_serializer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_update.h"

using testing::UnorderedElementsAre;

namespace ui {

using BasicAXTreeSerializer =
    AXTreeSerializer<const AXNode*,
                     std::vector<raw_ptr<const AXNode, VectorExperimental>>,
                     AXTreeUpdate*,
                     AXTreeData*,
                     AXNodeData>;

// The framework for these tests is that each test sets up |treedata0_|
// and |treedata1_| and then calls GetTreeSerializer, which creates a
// serializer for a tree that's initially in state |treedata0_|, but then
// changes to state |treedata1_|. This allows each test to check the
// updates created by AXTreeSerializer or unit-test its private
// member functions.
class AXTreeSerializerTest : public testing::Test {
 public:
  AXTreeSerializerTest() {}

  AXTreeSerializerTest(const AXTreeSerializerTest&) = delete;
  AXTreeSerializerTest& operator=(const AXTreeSerializerTest&) = delete;

  ~AXTreeSerializerTest() override {}

 protected:
  void CreateTreeSerializer();

  AXTreeUpdate treedata0_;
  AXTreeUpdate treedata1_;
  std::unique_ptr<AXSerializableTree> tree0_;
  std::unique_ptr<AXSerializableTree> tree1_;
  std::unique_ptr<AXTreeSource<const AXNode*, AXTreeData*, AXNodeData>>
      tree0_source_;
  std::unique_ptr<AXTreeSource<const AXNode*, AXTreeData*, AXNodeData>>
      tree1_source_;
  std::unique_ptr<BasicAXTreeSerializer> serializer_;
};

void AXTreeSerializerTest::CreateTreeSerializer() {
  if (serializer_)
    return;

  tree0_ = std::make_unique<AXSerializableTree>(treedata0_);
  tree1_ = std::make_unique<AXSerializableTree>(treedata1_);

  // Serialize tree0 so that AXTreeSerializer thinks that its client
  // is totally in sync.
  tree0_source_.reset(tree0_->CreateTreeSource());
  serializer_ = std::make_unique<BasicAXTreeSerializer>(tree0_source_.get());
  AXTreeUpdate unused_update;
  ASSERT_TRUE(serializer_->SerializeChanges(tree0_->root(), &unused_update));

  // Pretend that tree0_ turned into tree1_. The next call to
  // AXTreeSerializer will force it to consider these changes to
  // the tree and send them as part of the next update.
  tree1_source_.reset(tree1_->CreateTreeSource());
  serializer_->ChangeTreeSourceForTesting(tree1_source_.get());
}

// In this test, one child is added to the root. Only the root and
// new child should be added.
TEST_F(AXTreeSerializerTest, UpdateContainsOnlyChangedNodes) {
  // (1 (2 3))
  treedata0_.root_id = 1;
  treedata0_.nodes.resize(3);
  treedata0_.nodes[0].id = 1;
  treedata0_.nodes[0].child_ids.push_back(2);
  treedata0_.nodes[0].child_ids.push_back(3);
  treedata0_.nodes[1].id = 2;
  treedata0_.nodes[2].id = 3;

  // (1 (4 2 3))
  treedata1_.root_id = 1;
  treedata1_.nodes.resize(4);
  treedata1_.nodes[0].id = 1;
  treedata1_.nodes[0].child_ids.push_back(4);
  treedata1_.nodes[0].child_ids.push_back(2);
  treedata1_.nodes[0].child_ids.push_back(3);
  treedata1_.nodes[1].id = 2;
  treedata1_.nodes[2].id = 3;
  treedata1_.nodes[3].id = 4;

  CreateTreeSerializer();
  AXTreeUpdate update;
  ASSERT_TRUE(serializer_->SerializeChanges(tree1_->GetFromId(1), &update));

  // The update should only touch nodes 1 and 4 - nodes 2 and 3 are unchanged
  // and shouldn't be affected.
  EXPECT_EQ(0, update.node_id_to_clear);
  ASSERT_EQ(2u, update.nodes.size());
  EXPECT_EQ(1, update.nodes[0].id);
  EXPECT_EQ(4, update.nodes[1].id);
}

// When the root changes, the whole tree is updated, even if some of it
// is unaffected.
TEST_F(AXTreeSerializerTest, NewRootUpdatesEntireTree) {
  // (1 (2 (3 (4))))
  treedata0_.root_id = 1;
  treedata0_.nodes.resize(4);
  treedata0_.nodes[0].id = 1;
  treedata0_.nodes[0].child_ids.push_back(2);
  treedata0_.nodes[1].id = 2;
  treedata0_.nodes[1].child_ids.push_back(3);
  treedata0_.nodes[2].id = 3;
  treedata0_.nodes[2].child_ids.push_back(4);
  treedata0_.nodes[3].id = 4;

  // (5 (2 (3 (4))))
  treedata1_.root_id = 5;
  treedata1_.nodes.resize(4);
  treedata1_.nodes[0].id = 5;
  treedata1_.nodes[0].child_ids.push_back(2);
  treedata1_.nodes[1].id = 2;
  treedata1_.nodes[1].child_ids.push_back(3);
  treedata1_.nodes[2].id = 3;
  treedata1_.nodes[2].child_ids.push_back(4);
  treedata1_.nodes[3].id = 4;

  CreateTreeSerializer();
  AXTreeUpdate update;
  ASSERT_TRUE(serializer_->SerializeChanges(tree1_->GetFromId(4), &update));

  // The update should delete the subtree rooted at node id=1, and
  // then include all four nodes in the update, even though the
  // subtree rooted at id=2 didn't actually change.
  EXPECT_EQ(1, update.node_id_to_clear);
  ASSERT_EQ(4u, update.nodes.size());
  EXPECT_EQ(5, update.nodes[0].id);
  EXPECT_EQ(2, update.nodes[1].id);
  EXPECT_EQ(3, update.nodes[2].id);
  EXPECT_EQ(4, update.nodes[3].id);
}

// When a node is reparented, the subtree including both the old parent
// and new parent of the reparented node must be deleted and recreated.
TEST_F(AXTreeSerializerTest, ReparentingUpdatesSubtree) {
  // (1 (2 (3 (4) 5)))
  treedata0_.root_id = 1;
  treedata0_.nodes.resize(5);
  treedata0_.nodes[0].id = 1;
  treedata0_.nodes[0].child_ids.push_back(2);
  treedata0_.nodes[1].id = 2;
  treedata0_.nodes[1].child_ids.push_back(3);
  treedata0_.nodes[1].child_ids.push_back(5);
  treedata0_.nodes[2].id = 3;
  treedata0_.nodes[2].child_ids.push_back(4);
  treedata0_.nodes[3].id = 4;
  treedata0_.nodes[4].id = 5;

  // Node 5 has been reparented from being a child of node 2,
  // to a child of node 4.
  // (1 (2 (3 (4 (5)))))
  treedata1_.root_id = 1;
  treedata1_.nodes.resize(5);
  treedata1_.nodes[0].id = 1;
  treedata1_.nodes[0].child_ids.push_back(2);
  treedata1_.nodes[1].id = 2;
  treedata1_.nodes[1].child_ids.push_back(3);
  treedata1_.nodes[2].id = 3;
  treedata1_.nodes[2].child_ids.push_back(4);
  treedata1_.nodes[3].id = 4;
  treedata1_.nodes[3].child_ids.push_back(5);
  treedata1_.nodes[4].id = 5;

  CreateTreeSerializer();
  AXTreeUpdate update;
  ASSERT_TRUE(serializer_->SerializeChanges(tree1_->GetFromId(4), &update));

  // The update should unserialize without errors.
  AXTree dst_tree(treedata0_);
  EXPECT_TRUE(dst_tree.Unserialize(update)) << dst_tree.error();

  // The update should delete the subtree rooted at node id=2, and
  // then include nodes 2...5.
  EXPECT_EQ(2, update.node_id_to_clear);
  ASSERT_EQ(4u, update.nodes.size());
  EXPECT_EQ(2, update.nodes[0].id);
  EXPECT_EQ(3, update.nodes[1].id);
  EXPECT_EQ(4, update.nodes[2].id);
  EXPECT_EQ(5, update.nodes[3].id);
}

// When a node is reparented, the subtree including both the old parent
// and new parent of the reparented node must be deleted and recreated.
TEST_F(AXTreeSerializerTest, ReparentingUpdatesSubtree2) {
  // (1 (2 (3 (444) 5)) 6 7)
  treedata0_.root_id = 1;
  treedata0_.nodes.resize(7);
  treedata0_.nodes[0].id = 1;
  treedata0_.nodes[0].child_ids.push_back(2);
  treedata0_.nodes[0].child_ids.push_back(6);
  treedata0_.nodes[0].child_ids.push_back(7);
  treedata0_.nodes[1].id = 2;
  treedata0_.nodes[1].child_ids.push_back(3);
  treedata0_.nodes[1].child_ids.push_back(5);
  treedata0_.nodes[2].id = 3;
  treedata0_.nodes[2].child_ids.push_back(444);
  treedata0_.nodes[3].id = 444;
  treedata0_.nodes[4].id = 5;
  treedata0_.nodes[5].id = 6;
  treedata0_.nodes[6].id = 7;

  // Node 444 has been reparented from being a child of node 3,
  // to a child of node 7.
  // (1 (2 (3 (4))) 6 7 (444))
  treedata1_.root_id = 1;
  treedata1_.nodes.resize(7);
  treedata1_.nodes[0].id = 1;
  treedata1_.nodes[0].child_ids.push_back(2);
  treedata1_.nodes[0].child_ids.push_back(6);
  treedata1_.nodes[0].child_ids.push_back(7);
  treedata1_.nodes[1].id = 2;
  treedata1_.nodes[1].child_ids.push_back(3);
  treedata1_.nodes[1].child_ids.push_back(5);
  treedata1_.nodes[2].id = 3;
  treedata1_.nodes[3].id = 5;
  treedata1_.nodes[4].id = 6;
  treedata1_.nodes[5].id = 7;
  treedata1_.nodes[5].child_ids.push_back(444);
  treedata1_.nodes[6].id = 444;

  CreateTreeSerializer();
  AXTreeUpdate update;
  ASSERT_TRUE(serializer_->SerializeChanges(tree1_->GetFromId(7), &update));

  // The update should unserialize without errors.
  AXTree dst_tree(treedata0_);
  EXPECT_TRUE(dst_tree.Unserialize(update)) << dst_tree.error();

  // The update should delete the subtree rooted at node id=1 (because the LCA
  // of node 3 and node 7 is node 1). Therefore, all descendants of root 1 will
  // be serialized (which is all nodes).
  EXPECT_EQ(1, update.node_id_to_clear);
  ASSERT_EQ(7u, update.nodes.size());
  EXPECT_EQ(1, update.nodes[0].id);
  EXPECT_EQ(2, update.nodes[1].id);
  EXPECT_EQ(3, update.nodes[2].id);
  EXPECT_EQ(5, update.nodes[3].id);
  EXPECT_EQ(6, update.nodes[4].id);
  EXPECT_EQ(7, update.nodes[5].id);
  EXPECT_EQ(444, update.nodes[6].id);
}

// Similar to ReparentingUpdatesSubtree, except that InvalidateSubtree is
// called on id=1 - we need to make sure that the reparenting is still
// detected.
TEST_F(AXTreeSerializerTest, ReparentingWithDirtySubtreeUpdates) {
  // (1 (2 (3 (4 (5)))))
  treedata0_.root_id = 1;
  treedata0_.nodes.resize(5);
  treedata0_.nodes[0].id = 1;
  treedata0_.nodes[0].child_ids.push_back(2);
  treedata0_.nodes[1].id = 2;
  treedata0_.nodes[1].child_ids.push_back(3);
  treedata0_.nodes[2].id = 3;
  treedata0_.nodes[2].child_ids.push_back(4);
  treedata0_.nodes[3].id = 4;
  treedata0_.nodes[3].child_ids.push_back(5);
  treedata0_.nodes[4].id = 5;

  // Node 5 has been reparented from being a child of node 4,
  // to a child of node 2.
  // (1 (2 (3 (4) 5)))
  treedata1_.root_id = 1;
  treedata1_.nodes.resize(5);
  treedata1_.nodes[0].id = 1;
  treedata1_.nodes[0].child_ids.push_back(2);
  treedata1_.nodes[1].id = 2;
  treedata1_.nodes[1].child_ids.push_back(3);
  treedata1_.nodes[1].child_ids.push_back(5);
  treedata1_.nodes[2].id = 3;
  treedata1_.nodes[2].child_ids.push_back(4);
  treedata1_.nodes[3].id = 4;
  treedata1_.nodes[4].id = 5;

  CreateTreeSerializer();
  AXTreeUpdate update;
  serializer_->MarkSubtreeDirty(1);
  ASSERT_TRUE(serializer_->SerializeChanges(tree1_->GetFromId(4), &update));

  // The update should unserialize without errors.
  AXTree dst_tree(treedata0_);
  EXPECT_TRUE(dst_tree.Unserialize(update)) << dst_tree.error();
}

// A variant of AXTreeSource that does not serialize one particular id,
// returning nullptr from methods that try to retrieve it.
class AXTreeSourceWithInvalidId
    : public AXTreeSource<const AXNode*, AXTreeData*, AXNodeData> {
 public:
  AXTreeSourceWithInvalidId(AXTree* tree, int invalid_id)
      : tree_(tree),
        invalid_id_(invalid_id) {}

  AXTreeSourceWithInvalidId(const AXTreeSourceWithInvalidId&) = delete;
  AXTreeSourceWithInvalidId& operator=(const AXTreeSourceWithInvalidId&) =
      delete;

  ~AXTreeSourceWithInvalidId() override {}

  // AXTreeSource implementation.
  bool GetTreeData(AXTreeData* data) const override {
    *data = AXTreeData();
    return true;
  }
  AXNode* GetRoot() const override { return tree_->root(); }
  AXNode* GetFromId(AXNodeID id) const override {
    return id == invalid_id_ ? nullptr : tree_->GetFromId(id);
  }
  AXNodeID GetId(const AXNode* node) const override { return node->id(); }
  void CacheChildrenIfNeeded(const AXNode*) override {}
  size_t GetChildCount(const AXNode* node) const override {
    return node->children().size();
  }
  AXNode* ChildAt(const AXNode* node, size_t index) const override {
    AXNode* result = node->children()[index];
    return result->id() == invalid_id_ ? nullptr : result;
  }
  void ClearChildCache(const AXNode*) override {}

  AXNode* GetParent(const AXNode* node) const override {
    return node->parent();
  }
  bool IsIgnored(const AXNode* node) const override {
    return node->IsIgnored();
  }
  bool IsEqual(const AXNode* node1, const AXNode* node2) const override {
    return node1 == node2;
  }
  const AXNode* GetNull() const override { return nullptr; }
  void SerializeNode(const AXNode* node, AXNodeData* out_data) const override {
    *out_data = node->data();
    if (node->id() == invalid_id_)
      out_data->id = -1;
  }

 private:
  raw_ptr<AXTree> tree_;
  int invalid_id_;
};

// Test that the serializer CHECKs when it finds invalid children.
TEST(AXTreeSerializerInvalidDeathTest, InvalidChild) {
  // (1 (2 3))
  AXTreeUpdate treedata;
  treedata.root_id = 1;
  treedata.nodes.resize(3);
  treedata.nodes[0].id = 1;
  treedata.nodes[0].child_ids.push_back(2);
  treedata.nodes[0].child_ids.push_back(3);
  treedata.nodes[1].id = 2;
  treedata.nodes[2].id = 3;

  AXTree tree(treedata);
  AXTreeSourceWithInvalidId source(&tree, 3);

  BasicAXTreeSerializer serializer(&source);
  AXTreeUpdate update;
  EXPECT_CHECK_DEATH_WITH(serializer.SerializeChanges(tree.root(), &update),
                          "child");
}

// Test that we can set a maximum number of nodes to serialize.
TEST_F(AXTreeSerializerTest, MaximumSerializedNodeCount) {
  // (1 (2 (3 4) 5 (6 7)))
  treedata0_.root_id = 1;
  treedata0_.nodes.resize(7);
  treedata0_.nodes[0].id = 1;
  treedata0_.nodes[0].child_ids.push_back(2);
  treedata0_.nodes[0].child_ids.push_back(5);
  treedata0_.nodes[1].id = 2;
  treedata0_.nodes[1].child_ids.push_back(3);
  treedata0_.nodes[1].child_ids.push_back(4);
  treedata0_.nodes[2].id = 3;
  treedata0_.nodes[3].id = 4;
  treedata0_.nodes[4].id = 5;
  treedata0_.nodes[4].child_ids.push_back(6);
  treedata0_.nodes[4].child_ids.push_back(7);
  treedata0_.nodes[5].id = 6;
  treedata0_.nodes[6].id = 7;

  tree0_ = std::make_unique<AXSerializableTree>(treedata0_);
  tree0_source_.reset(tree0_->CreateTreeSource());
  serializer_ = std::make_unique<BasicAXTreeSerializer>(tree0_source_.get());
  serializer_->set_max_node_count(4);
  AXTreeUpdate update;
  ASSERT_TRUE(serializer_->SerializeChanges(tree0_->root(), &update));
  // It actually serializes 5 nodes, not 4 - to be consistent.
  // It skips the children of node 5.
  ASSERT_EQ(5u, update.nodes.size());
}

#if defined(GTEST_HAS_DEATH_TEST)
// If duplicate ids are encountered, it crashes via CHECK(false).
TEST_F(AXTreeSerializerTest, DuplicateIdsCrashes) {
  // (1 (2 (3 (4) 5)))
  treedata0_.root_id = 1;
  treedata0_.nodes.resize(5);
  treedata0_.nodes[0].id = 1;
  treedata0_.nodes[0].child_ids.push_back(2);
  treedata0_.nodes[1].id = 2;
  treedata0_.nodes[1].child_ids.push_back(3);
  treedata0_.nodes[1].child_ids.push_back(5);
  treedata0_.nodes[2].id = 3;
  treedata0_.nodes[2].child_ids.push_back(4);
  treedata0_.nodes[3].id = 4;
  treedata0_.nodes[4].id = 5;

  // (1 (2 (6 (7) 5)))
  treedata1_.root_id = 1;
  treedata1_.nodes.resize(5);
  treedata1_.nodes[0].id = 1;
  treedata1_.nodes[0].child_ids.push_back(2);
  treedata1_.nodes[1].id = 2;
  treedata1_.nodes[1].child_ids.push_back(6);
  treedata1_.nodes[1].child_ids.push_back(5);
  treedata1_.nodes[2].id = 6;
  treedata1_.nodes[2].child_ids.push_back(7);
  treedata1_.nodes[3].id = 7;
  treedata1_.nodes[4].id = 5;

  CreateTreeSerializer();

  // Do some open-heart surgery on tree1, giving it a duplicate node.
  // This could not happen with an AXTree, but could happen with
  // another AXTreeSource if the structure it wraps is buggy. We want to
  // fail but not crash when that happens.
  std::vector<raw_ptr<AXNode, VectorExperimental>> node2_children;
  node2_children.push_back(tree1_->GetFromId(7));
  node2_children.push_back(tree1_->GetFromId(6));
  tree1_->GetFromId(2)->SwapChildren(&node2_children);

  AXTreeUpdate update;
  EXPECT_DEATH(serializer_->SerializeChanges(tree1_->GetFromId(7), &update),
               "");

  // Swap it back, fixing the tree. Given the above crash, this is just to
  // ensure the test can clean up properly and avoid a different failure.
  tree1_->GetFromId(2)->SwapChildren(&node2_children);
  update = AXTreeUpdate();
  EXPECT_TRUE(serializer_->SerializeChanges(tree1_->GetFromId(7), &update));
}
#endif

// If a tree serializer is reset, that means it doesn't know about
// the state of the client tree anymore. The safest thing to do in
// that circumstance is to force the client to clear everything.
TEST_F(AXTreeSerializerTest, ResetUpdatesNodeIdToClear) {
  // (1 (2 (3 (4 (5)))))
  treedata0_.root_id = 1;
  treedata0_.nodes.resize(5);
  treedata0_.nodes[0].id = 1;
  treedata0_.nodes[0].child_ids.push_back(2);
  treedata0_.nodes[1].id = 2;
  treedata0_.nodes[1].child_ids.push_back(3);
  treedata0_.nodes[2].id = 3;
  treedata0_.nodes[2].child_ids.push_back(4);
  treedata0_.nodes[3].id = 4;
  treedata0_.nodes[3].child_ids.push_back(5);
  treedata0_.nodes[4].id = 5;

  // Node 5 has been reparented from being a child of node 4,
  // to a child of node 2.
  // (1 (2 (3 (4) 5)))
  treedata1_.root_id = 1;
  treedata1_.nodes.resize(5);
  treedata1_.nodes[0].id = 1;
  treedata1_.nodes[0].child_ids.push_back(2);
  treedata1_.nodes[1].id = 2;
  treedata1_.nodes[1].child_ids.push_back(3);
  treedata1_.nodes[1].child_ids.push_back(5);
  treedata1_.nodes[2].id = 3;
  treedata1_.nodes[2].child_ids.push_back(4);
  treedata1_.nodes[3].id = 4;
  treedata1_.nodes[4].id = 5;

  CreateTreeSerializer();

  serializer_->Reset();

  AXTreeUpdate update;
  ASSERT_TRUE(serializer_->SerializeChanges(tree1_->GetFromId(4), &update));

  // The update should unserialize without errors.
  AXTree dst_tree(treedata0_);
  EXPECT_TRUE(dst_tree.Unserialize(update)) << dst_tree.error();
}

// Ensure that calling Reset doesn't cause any problems if
// the root changes.
TEST_F(AXTreeSerializerTest, ResetWorksWithNewRootId) {
  // (1 (2))
  treedata0_.root_id = 1;
  treedata0_.nodes.resize(2);
  treedata0_.nodes[0].id = 1;
  treedata0_.nodes[0].child_ids.push_back(2);
  treedata0_.nodes[1].id = 2;

  // (3 (4))
  treedata1_.root_id = 3;
  treedata1_.nodes.resize(2);
  treedata1_.nodes[0].id = 3;
  treedata1_.nodes[0].child_ids.push_back(4);
  treedata1_.nodes[1].id = 4;

  CreateTreeSerializer();
  serializer_->Reset();

  AXTreeUpdate update;
  ASSERT_TRUE(serializer_->SerializeChanges(tree1_->GetFromId(4), &update));

  // The update should unserialize without errors.
  AXTree dst_tree(treedata0_);
  EXPECT_TRUE(dst_tree.Unserialize(update)) << dst_tree.error();
}

TEST_F(AXTreeSerializerTest, TestPartialSerialization) {
  // Serialize only part of the tree.

  // (1)
  treedata0_.root_id = 1;
  treedata0_.nodes.resize(1);
  treedata0_.nodes[0].id = 1;

  // (1 (2 (3 4)) (5 (6 7)))
  treedata1_.root_id = 1;
  treedata1_.nodes.resize(7);
  treedata1_.nodes[0].id = 1;
  treedata1_.nodes[0].child_ids = {2, 5};
  treedata1_.nodes[1].id = 2;
  treedata1_.nodes[1].child_ids = {3, 4};
  treedata1_.nodes[2].id = 3;
  treedata1_.nodes[3].id = 4;
  treedata1_.nodes[4].id = 5;
  treedata1_.nodes[4].child_ids = {6, 7};
  treedata1_.nodes[5].id = 6;
  treedata1_.nodes[6].id = 7;

  for (int max_node_count = 1; max_node_count <= 4; max_node_count++) {
    SCOPED_TRACE(base::StringPrintf("Max node count: %d", max_node_count));
    CreateTreeSerializer();

    serializer_->Reset();
    serializer_->set_max_node_count(max_node_count);

    AXTreeUpdate update;
    ASSERT_TRUE(serializer_->SerializeChanges(tree1_->GetFromId(1), &update));

    // The update should unserialize without errors.
    AXSerializableTree dst_tree(treedata0_);
    EXPECT_TRUE(dst_tree.Unserialize(update)) << dst_tree.error();

    // The tree should be incomplete; it should have too few nodes.
    EXPECT_LT(update.nodes.size(), treedata1_.nodes.size());
    EXPECT_LT(dst_tree.size(), static_cast<int>(treedata1_.nodes.size()));

    // The serializer should give us a list of nodes that have yet to
    // be serialized.
    std::vector<AXNodeID> incomplete_node_ids =
        serializer_->GetIncompleteNodeIds();
    EXPECT_FALSE(incomplete_node_ids.empty());

    // Serialize the incomplete nodes, with no more limit.
    serializer_->set_max_node_count(0);
    for (AXNodeID id : incomplete_node_ids) {
      update = AXTreeUpdate();
      ASSERT_TRUE(
          serializer_->SerializeChanges(tree1_->GetFromId(id), &update));
      EXPECT_TRUE(dst_tree.Unserialize(update)) << dst_tree.error();
    }

    // The result should be indistinguishable from the source tree.
    std::unique_ptr<AXTreeSource<const AXNode*, AXTreeData*, AXNodeData>>
        dst_tree_source(dst_tree.CreateTreeSource());
    BasicAXTreeSerializer serializer(dst_tree_source.get());
    AXTreeUpdate dst_update;
    CHECK(serializer.SerializeChanges(dst_tree.root(), &dst_update));
    ASSERT_EQ(treedata1_.ToString(), dst_update.ToString());
  }
}

}  // namespace ui
