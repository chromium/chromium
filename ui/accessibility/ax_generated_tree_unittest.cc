// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <numeric>

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/tree_generator.h"

namespace ui {
namespace {

// A function to turn a tree into a string, capturing only the node ids
// and their relationship to one another.
//
// The string format is kind of like an S-expression, with each expression
// being either a node id, or a node id followed by a subexpression
// representing its children.
//
// Examples:
//
// (1) is a tree with a single node with id 1.
// (1 (2 3)) is a tree with 1 as the root, and 2 and 3 as its children.
// (1 (2 (3))) has 1 as the root, 2 as its child, and then 3 as the child of 2.
std::string TreeToStringHelper(const AXNode* node) {
  std::string result = base::NumberToString(node->id());
  if (node->children().empty())
    return result;
  const auto add_children = [](const std::string& str, const auto* node) {
    return str + " " + TreeToStringHelper(node);
  };
  return result + " (" +
         std::accumulate(node->children().cbegin() + 1, node->children().cend(),
                         TreeToStringHelper(node->children().front()),
                         add_children) +
         ")";
}

std::string TreeToString(const AXTree& tree) {
  return "(" + TreeToStringHelper(tree.root()) + ")";
}

}  // anonymous namespace

// Test the TreeGenerator class by building all possible trees with
// 3 nodes and the ids [1...3], with no permutations of ids.
TEST(AXGeneratedTreeTest, TestTreeGeneratorNoPermutations) {
  int tree_size = 3;
  TreeGenerator generator(tree_size, false);
  const char* EXPECTED_TREES[] = {
    "(1)",
    "(1 (2))",
    "(1 (2 3))",
    "(1 (2 (3)))",
  };

  int n = generator.UniqueTreeCount();
  ASSERT_EQ(static_cast<int>(base::size(EXPECTED_TREES)), n);

  for (int i = 0; i < n; ++i) {
    AXTree tree;
    generator.BuildUniqueTree(i, &tree);
    std::string str = TreeToString(tree);
    EXPECT_EQ(EXPECTED_TREES[i], str);
  }
}

// Test the TreeGenerator class by building all possible trees with
// 3 nodes and the ids [1...3] permuted in any order.
TEST(AXGeneratedTreeTest, TestTreeGeneratorWithPermutations) {
  int tree_size = 3;
  TreeGenerator generator(tree_size, true);
  const char* EXPECTED_TREES[] = {
    "(1)",
    "(1 (2))",
    "(2 (1))",
    "(1 (2 3))",
    "(2 (1 3))",
    "(3 (1 2))",
    "(1 (3 2))",
    "(2 (3 1))",
    "(3 (2 1))",
    "(1 (2 (3)))",
    "(2 (1 (3)))",
    "(3 (1 (2)))",
    "(1 (3 (2)))",
    "(2 (3 (1)))",
    "(3 (2 (1)))",
  };

  int n = generator.UniqueTreeCount();
  ASSERT_EQ(static_cast<int>(base::size(EXPECTED_TREES)), n);

  for (int i = 0; i < n; i++) {
    AXTree tree;
    generator.BuildUniqueTree(i, &tree);
    std::string str = TreeToString(tree);
    EXPECT_EQ(EXPECTED_TREES[i], str);
  }
}

// Test mutating every possible tree with <n> nodes to every other possible
// tree with <n> nodes, where <n> is 4 in release mode and 3 in debug mode
// (for speed). For each possible combination of trees, we also vary which
// node we serialize first.
//
// For every possible scenario, we check that the AXTreeUpdate is valid,
// that the destination tree can unserialize it and create a valid tree,
// and that after updating all nodes the resulting tree now matches the
// intended tree.
TEST(AXGeneratedTreeTest, SerializeGeneratedTrees) {
  // Do a more exhaustive test in release mode. If you're modifying
  // the algorithm you may want to try even larger tree sizes if you
  // can afford the time.
#ifdef NDEBUG
  int max_tree_size = 4;
#else
  LOG(WARNING) << "Debug build, only testing trees with 3 nodes and not 4.";
  int max_tree_size = 3;
#endif

  TreeGenerator generator0(max_tree_size, false);
  int n0 = generator0.UniqueTreeCount();

  TreeGenerator generator1(max_tree_size, true);
  int n1 = generator1.UniqueTreeCount();

  for (int i = 0; i < n0; i++) {
    // Build the first tree, tree0.
    AXSerializableTree tree0;
    generator0.BuildUniqueTree(i, &tree0);
    SCOPED_TRACE("tree0 is " + TreeToString(tree0));

    for (int j = 0; j < n1; j++) {
      // Build the second tree, tree1.
      AXSerializableTree tree1;
      generator1.BuildUniqueTree(j, &tree1);
      SCOPED_TRACE("tree1 is " + TreeToString(tree1));

      int tree_size = tree1.size();

      // Now iterate over which node to update first, |k|.
      for (int k = 0; k < tree_size; k++) {
        // Iterate over a node to invalidate, |l| (zero means no invalidation).
        for (int l = 0; l <= tree_size; l++) {
          SCOPED_TRACE("i=" + base::NumberToString(i) +
                       " j=" + base::NumberToString(j) +
                       " k=" + base::NumberToString(k) +
                       " l=" + base::NumberToString(l));

          // Start by serializing tree0 and unserializing it into a new
          // empty tree |dst_tree|.
          std::unique_ptr<AXTreeSource<const AXNode*, AXNodeData, AXTreeData>>
              tree0_source(tree0.CreateTreeSource());
          AXTreeSerializer<const AXNode*, AXNodeData, AXTreeData> serializer(
              tree0_source.get());
          AXTreeUpdate update0;
          ASSERT_TRUE(serializer.SerializeChanges(tree0.root(), &update0));

          AXTree dst_tree;
          ASSERT_TRUE(dst_tree.Unserialize(update0));

          // At this point, |dst_tree| should now be identical to |tree0|.
          EXPECT_EQ(TreeToString(tree0), TreeToString(dst_tree));

          // Next, pretend that tree0 turned into tree1.
          std::unique_ptr<AXTreeSource<const AXNode*, AXNodeData, AXTreeData>>
              tree1_source(tree1.CreateTreeSource());
          serializer.ChangeTreeSourceForTesting(tree1_source.get());

          // Invalidate a subtree rooted at one of the nodes.
          if (l > 0)
            serializer.InvalidateSubtree(tree1.GetFromId(l));

          // Serialize a sequence of updates to |dst_tree| to match.
          for (int k_index = 0; k_index < tree_size; ++k_index) {
            int id = 1 + (k + k_index) % tree_size;
            AXTreeUpdate update;
            ASSERT_TRUE(
                serializer.SerializeChanges(tree1.GetFromId(id), &update));
            ASSERT_TRUE(dst_tree.Unserialize(update));
          }

          // After the sequence of updates, |dst_tree| should now be
          // identical to |tree1|.
          EXPECT_EQ(TreeToString(tree1), TreeToString(dst_tree));
        }
      }
    }
  }
}

}  // namespace ui
