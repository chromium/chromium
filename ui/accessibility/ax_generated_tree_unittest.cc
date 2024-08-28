// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <numeric>
#include <optional>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/tree_generator.h"

namespace ui {
namespace {

// Do a more exhaustive test in release mode. If you're modifying
// the algorithm you may want to try even larger tree sizes if you
// can afford the time.
#ifdef NDEBUG
constexpr int kMax_tree_size = 4;
#else
constexpr int kMax_tree_size = 3;
#endif

// We split the test into four by splitting the two nested loops that builds the
// trees. To do so, we need to know the maximum number of (permuted) trees.
constexpr int kMax_number_of_trees0 =
    TreeGenerator::ComputeUniqueTreeCount(kMax_tree_size,
                                          /* permutations */ false);
constexpr int kMax_number_of_trees1 =
    TreeGenerator::ComputeUniqueTreeCount(kMax_tree_size,
                                          /* permutations */ true);

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
// (1 (2 (3x))) is the same with node 3 ignored.
std::string TreeToStringHelper(const AXNode* node) {
  std::string result = base::NumberToString(node->id());
  if (node->IsIgnored())
    result += "x";
  if (node->children().empty())
    return result;
  const auto add_children = [](const std::string& str, const AXNode* node) {
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

AXTreeUpdate SerializeEntireTree(AXSerializableTree& tree) {
  std::unique_ptr<AXTreeSource<const AXNode*, AXTreeData*, AXNodeData>>
      tree_source(tree.CreateTreeSource());
  AXTreeSerializer<const AXNode*, std::vector<const AXNode*>, AXTreeUpdate*,
                   AXTreeData*, AXNodeData>
      serializer(tree_source.get());
  AXTreeUpdate update;
  CHECK(serializer.SerializeChanges(tree.root(), &update));
  return update;
}

// Create an AXTreeUpdate consisting of only those nodes from
// |tree0| that changed their ignored status in |tree1|.
AXTreeUpdate MakeTreeUpdateFromIgnoredChanges(AXSerializableTree& tree0,
                                              AXSerializableTree& tree1) {
  AXTreeUpdate update = SerializeEntireTree(tree1);
  AXTreeUpdate result;
  for (auto& node : update.nodes) {
    AXNode* tree0_node = tree0.GetFromId(node.id);
    AXNode* tree1_node = tree1.GetFromId(node.id);
    if (tree0_node->IsIgnored() != tree1_node->IsIgnored())
      result.nodes.push_back(node);
  }
  return result;
}

void SerializeUnignoredNodes(AXNode* node, AXTreeUpdate* update) {
  AXNodeData data = node->data();
  data.child_ids.clear();
  for (size_t i = 0; i < node->GetUnignoredChildCount(); i++) {
    AXNode* child = node->GetUnignoredChildAtIndex(i);
    data.child_ids.push_back(child->id());
  }
  update->nodes.push_back(data);
  for (size_t i = 0; i < node->GetUnignoredChildCount(); i++) {
    AXNode* child = node->GetUnignoredChildAtIndex(i);
    SerializeUnignoredNodes(child, update);
  }
}

void MakeTreeOfUnignoredNodesOnly(AXSerializableTree& src,
                                  AXSerializableTree* dst) {
  AXTreeUpdate update;
  update.root_id = src.root()->id();
  SerializeUnignoredNodes(src.root(), &update);
  CHECK(dst->Unserialize(update));
}

}  // namespace

// Test the TreeGenerator class by building all possible trees with
// 3 nodes and the ids [1...3], with no permutations of ids.
TEST(AXGeneratedTreeTest, TestTreeGeneratorNoPermutations) {
  int tree_size = 3;
  TreeGenerator generator(tree_size, false);
  // clang-format off
  const char* EXPECTED_TREES[] = {
    "(1)",
    "(1 (2))",
    "(1 (2 3))",
    "(1 (2 (3)))",
  };
  // clang-format on

  int n = generator.UniqueTreeCount();
  ASSERT_EQ(static_cast<int>(std::size(EXPECTED_TREES)), n);

  for (int i = 0; i < n; ++i) {
    AXTree tree;
    generator.BuildUniqueTree(i, &tree);
    std::string str = TreeToString(tree);
    EXPECT_EQ(EXPECTED_TREES[i], str);
  }
}

// Test generating trees with permutations of ignored nodes.
TEST(AXGeneratedTreeTest, TestGeneratingTreesWithIgnoredNodes) {
  int tree_size = 3;
  TreeGenerator generator(tree_size, false);
  // clang-format off
  const char* EXPECTED_TREES[] = {
      "(1)",
      "(1 (2))",
      "(1 (2x))",
      "(1 (2 3))",
      "(1 (2x 3))",
      "(1 (2 3x))",
      "(1 (2x 3x))",
      "(1 (2 (3)))",
      "(1 (2x (3)))",
      "(1 (2 (3x)))",
      "(1 (2x (3x)))",
  };
  // clang-format on

  int n = generator.UniqueTreeCount();
  int expected_index = 0;
  for (int i = 0; i < n; ++i) {
    int ignored_permutation_count =
        generator.IgnoredPermutationCountPerUniqueTree(i);
    for (int j = 0; j < ignored_permutation_count; j++) {
      AXTree tree;
      generator.BuildUniqueTreeWithIgnoredNodes(
          i, j, /* focused_node */ std::nullopt, &tree);
      std::string str = TreeToString(tree);
      EXPECT_EQ(EXPECTED_TREES[expected_index++], str);
    }
  }
  EXPECT_EQ(11, expected_index);
}

// Test the TreeGenerator class by building all possible trees with
// 3 nodes and the ids [1...3] permuted in any order.
TEST(AXGeneratedTreeTest, TestTreeGeneratorWithPermutations) {
  int tree_size = 3;
  TreeGenerator generator(tree_size, true);
  // clang-format off
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
  // clang-format on

  int n = generator.UniqueTreeCount();
  ASSERT_EQ(static_cast<int>(std::size(EXPECTED_TREES)), n);

  for (int i = 0; i < n; i++) {
    AXTree tree;
    generator.BuildUniqueTree(i, &tree);
    std::string str = TreeToString(tree);
    EXPECT_EQ(EXPECTED_TREES[i], str);
  }
}

struct PermutationBlock {
  PermutationBlock(int first_unique_tree0,
                   int last_unique_tree0,
                   int first_unique_tree1,
                   int last_unique_tree1)
      : first_unique_tree0(first_unique_tree0),
        last_unique_tree0(last_unique_tree0),
        first_unique_tree1(first_unique_tree1),
        last_unique_tree1(last_unique_tree1) {}
  int first_unique_tree0;
  int last_unique_tree0;
  int first_unique_tree1;
  int last_unique_tree1;
};

class SerializeGeneratedTreesTest
    : public testing::TestWithParam<PermutationBlock> {};

TEST_P(SerializeGeneratedTreesTest, SerializeGeneratedTrees) {
  const int first_tree0_ = GetParam().first_unique_tree0;
  const int last_tree0_ = GetParam().last_unique_tree0;
  const int first_tree1_ = GetParam().first_unique_tree1;
  const int last_tree1_ = GetParam().last_unique_tree1;
  TreeGenerator generator0(kMax_tree_size, /* permutations */ false);
  TreeGenerator generator1(kMax_tree_size, /* permutations */ true);

  for (int i = first_tree0_; i < last_tree0_; i++) {
    // Build the first tree, tree0.
    AXSerializableTree tree0;
    generator0.BuildUniqueTree(i, &tree0);
    SCOPED_TRACE("tree0 is " + TreeToString(tree0));

    for (int j = first_tree1_; j < last_tree1_; j++) {
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
          std::unique_ptr<AXTreeSource<const AXNode*, AXTreeData*, AXNodeData>>
              tree0_source(tree0.CreateTreeSource());
          AXTreeSerializer<const AXNode*, std::vector<const AXNode*>,
                           AXTreeUpdate*, AXTreeData*, AXNodeData>
              serializer(tree0_source.get());
          AXTreeUpdate update0;
          ASSERT_TRUE(serializer.SerializeChanges(tree0.root(), &update0));

          AXTree dst_tree;
          ASSERT_TRUE(dst_tree.Unserialize(update0))
              << dst_tree.error() << "\n"
              << TreeToString(dst_tree)
              << "\nTree update: " << update0.ToString();

          // At this point, |dst_tree| should now be identical to |tree0|.
          EXPECT_EQ(TreeToString(tree0), TreeToString(dst_tree));

          // Next, pretend that tree0 turned into tree1.
          std::unique_ptr<AXTreeSource<const AXNode*, AXTreeData*, AXNodeData>>
              tree1_source(tree1.CreateTreeSource());
          serializer.ChangeTreeSourceForTesting(tree1_source.get());

          // Mark as dirty the subtree rooted at one of the nodes.
          if (l > 0)
            serializer.MarkSubtreeDirty(l);

          // Serialize a sequence of updates to |dst_tree| to match.
          for (int k_index = 0; k_index < tree_size; ++k_index) {
            int id = 1 + (k + k_index) % tree_size;
            AXTreeUpdate update;
            ASSERT_TRUE(
                serializer.SerializeChanges(tree1.GetFromId(id), &update));
            std::string tree_before_str = TreeToString(dst_tree);
            ASSERT_TRUE(dst_tree.Unserialize(update))
                << dst_tree.error() << "\nTree before   : " << tree_before_str
                << "\nTree after    : " << TreeToString(dst_tree)
                << "\nExpected after: " << TreeToString(tree1)
                << "\nTree update   : " << update.ToString();
          }

          // After the sequence of updates, |dst_tree| should now be
          // identical to |tree1|.
          EXPECT_EQ(TreeToString(tree1), TreeToString(dst_tree));
        }
      }
    }
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
//
// Sheriffs: this test is actually very stable and reliable, but it's
// cpu-bound so under extremely heavy load it sometimes times out even
// though it only takes 1 - 2 seconds to run under normal load.
// Please don't disable unless it's actually flaking frequently (e.g.,
// every day). Check Flake Portal first.

INSTANTIATE_TEST_SUITE_P(
    AXGeneratedTreeTest0,
    SerializeGeneratedTreesTest,
    testing::Values(PermutationBlock(0,
                                     kMax_number_of_trees0 / 2,
                                     0,
                                     kMax_number_of_trees1 / 2),
                    PermutationBlock(0,
                                     kMax_number_of_trees0 / 2,
                                     kMax_number_of_trees1 / 2,
                                     kMax_number_of_trees1),
                    PermutationBlock(kMax_number_of_trees0 / 2,
                                     kMax_number_of_trees0,
                                     0,
                                     kMax_number_of_trees1 / 2),
                    PermutationBlock(kMax_number_of_trees0 / 2,
                                     kMax_number_of_trees0,
                                     kMax_number_of_trees1 / 2,
                                     kMax_number_of_trees1)));

// Sheriffs: this test is actually very stable and reliable, but it's
// cpu-bound so under extremely heavy load it sometimes times out even
// though it only takes 1 - 2 seconds to run under normal load.
// Please don't disable unless it's actually flaking frequently (e.g.,
// every day). Check Flake Portal first.
TEST(AXGeneratedTreeTest, GeneratedTreesWithIgnoredNodes) {
  // Do a more exhaustive test in release mode. If you're modifying
  // the algorithm you may want to try even larger tree sizes if you
  // can afford the time.
#ifdef NDEBUG
  int max_tree_size = 5;
#else
  LOG(WARNING) << "Debug build, only testing trees with 4 nodes and not 5.";
  int max_tree_size = 4;
#endif

  TreeGenerator generator(max_tree_size, false);
  int unique_tree_count = generator.UniqueTreeCount();

  // Loop over every possible tree up to a certain size.
  for (int tree_index = 0; tree_index < unique_tree_count; tree_index++) {
    // Try each permutation of nodes other than the root being ignored.
    // We'll call this tree the "fat" tree because it has redundant
    // ignored nodes. Also try permuting the focused node, because focus affects
    // the ignored state of a node by removing it.
    int ignored_permutation_count =
        generator.IgnoredPermutationCountPerUniqueTree(tree_index);
    for (int perm_index0 = 0; perm_index0 < ignored_permutation_count;
         perm_index0++) {
      AXSerializableTree fat_tree;
      generator.BuildUniqueTreeWithIgnoredNodes(
          tree_index, perm_index0, /* focused_node */ std::nullopt, &fat_tree);
      SCOPED_TRACE("fat_tree is " + TreeToString(fat_tree));

      // Create a second tree, also with each permutations of nodes other than
      // the root being ignored.
      for (int perm_index1 = 1; perm_index1 < ignored_permutation_count;
           perm_index1++) {
        AXSerializableTree fat_tree1;
        generator.BuildUniqueTreeWithIgnoredNodes(
            tree_index, perm_index1, /* focused_node */ std::nullopt,
            &fat_tree1);
        SCOPED_TRACE("fat_tree1 is " + TreeToString(fat_tree1));

        // Make a source and destination tree using only the unignored nodes.
        // We call this one the "skinny" tree.
        AXSerializableTree skinny_tree;
        MakeTreeOfUnignoredNodesOnly(fat_tree, &skinny_tree);
        AXSerializableTree skinny_tree1;
        MakeTreeOfUnignoredNodesOnly(fat_tree1, &skinny_tree1);

        // Now, turn fat_tree into fat_tree1, and record the generated events.
        AXEventGenerator event_generator(&fat_tree);
        AXTreeUpdate update =
            MakeTreeUpdateFromIgnoredChanges(fat_tree, fat_tree1);
        std::string fat_tree_before_str = TreeToString(fat_tree);
        ASSERT_TRUE(fat_tree.Unserialize(update))
            << fat_tree.error() << "\nTree before   : " << fat_tree_before_str
            << "\nTree after    :" << TreeToString(fat_tree)
            << "\nExpected after: " << TreeToString(fat_tree1)
            << "\nTree update   : " << update.ToString();

        EXPECT_EQ(TreeToString(fat_tree), TreeToString(fat_tree1));

        // Capture the events generated.
        std::map<AXNodeID, std::set<AXEventGenerator::Event>> actual_events;
        for (const AXEventGenerator::TargetedEvent& event : event_generator) {
          const AXNode* node = fat_tree.GetFromId(event.node_id);
          ASSERT_NE(nullptr, node);
          if (node->IsIgnored() ||
              event.event_params->event ==
                  AXEventGenerator::Event::IGNORED_CHANGED) {
            continue;
          }

          actual_events[event.node_id].insert(event.event_params->event);
        }

        // Now, turn skinny_tree into skinny_tree1 and compare
        // the generated events.
        AXEventGenerator skinny_event_generator(&skinny_tree);
        AXTreeUpdate skinny_update = SerializeEntireTree(skinny_tree1);
        std::string skinny_tree_before_str = TreeToString(skinny_tree);
        ASSERT_TRUE(skinny_tree.Unserialize(skinny_update))
            << skinny_tree.error()
            << "\nTree before   : " << skinny_tree_before_str
            << "\nTree after    :" << TreeToString(skinny_tree)
            << "\nExpected after: " << TreeToString(skinny_tree1)
            << "\nTree update   : " << skinny_update.ToString();

        EXPECT_EQ(TreeToString(skinny_tree), TreeToString(skinny_tree1));

        std::map<AXNodeID, std::set<AXEventGenerator::Event>> expected_events;
        for (const AXEventGenerator::TargetedEvent& event :
             skinny_event_generator)
          expected_events[event.node_id].insert(event.event_params->event);

        for (auto& entry : expected_events) {
          AXNodeID node_id = entry.first;
          for (auto& event_type : entry.second) {
            EXPECT_TRUE(actual_events[node_id].find(event_type) !=
                        actual_events[node_id].end())
                << "Expected " << event_type << " on node " << node_id;
          }
        }

        for (auto& entry : actual_events) {
          AXNodeID node_id = entry.first;
          for (auto& event_type : entry.second) {
            EXPECT_TRUE(expected_events[node_id].find(event_type) !=
                        expected_events[node_id].end())
                << "Unexpected " << event_type << " on node " << node_id;
          }
        }

        // For each node in skinny_tree (the tree with only the unignored
        // nodes), check the node in fat_tree (the tree with ignored nodes).
        // Make sure that the parents, children, and siblings are all computed
        // correctly.
        AXTreeUpdate skinny_tree_serialized = SerializeEntireTree(skinny_tree);
        for (const AXNodeData& skinny_tree_node_data :
             skinny_tree_serialized.nodes) {
          AXNodeID id = skinny_tree_node_data.id;

          AXNode* skinny_tree_node = skinny_tree.GetFromId(id);
          AXNode* fat_tree_node = fat_tree.GetFromId(id);

          SCOPED_TRACE("Testing node ID " + base::NumberToString(id));

          // Check children.
          EXPECT_EQ(skinny_tree_node->children().size(),
                    fat_tree_node->GetUnignoredChildCount());

          // Check child IDs.
          for (size_t j = 0; j < skinny_tree_node->children().size(); j++) {
            AXNode* skinny_tree_child = skinny_tree_node->children()[j];
            AXNode* fat_tree_child = fat_tree_node->GetUnignoredChildAtIndex(j);
            EXPECT_TRUE(skinny_tree_child);
            EXPECT_TRUE(fat_tree_child);
            if (fat_tree_child)
              EXPECT_EQ(skinny_tree_child->id(), fat_tree_child->id());
          }

          // Check parent.
          if (skinny_tree_node->parent()) {
            EXPECT_EQ(skinny_tree_node->parent()->id(),
                      fat_tree_node->GetUnignoredParent()->id());
          } else {
            EXPECT_FALSE(fat_tree_node->GetUnignoredParent());
          }

          // Check index in parent.
          EXPECT_EQ(skinny_tree_node->index_in_parent(),
                    fat_tree_node->GetUnignoredIndexInParent());

          // Unignored previous sibling.
          size_t index_in_parent = skinny_tree_node->index_in_parent();
          size_t num_siblings =
              skinny_tree_node->parent()
                  ? skinny_tree_node->parent()->children().size()
                  : 1;
          if (index_in_parent > 0) {
            AXNode* skinny_tree_previous_sibling =
                skinny_tree_node->parent()->children()[index_in_parent - 1];
            AXNode* fat_tree_previous_sibling =
                fat_tree_node->GetPreviousUnignoredSibling();
            EXPECT_TRUE(fat_tree_previous_sibling);
            if (fat_tree_previous_sibling) {
              EXPECT_EQ(skinny_tree_previous_sibling->id(),
                        fat_tree_previous_sibling->id());
            }
          }

          // Unignored next sibling.
          if (index_in_parent < num_siblings - 1) {
            AXNode* skinny_tree_next_sibling =
                skinny_tree_node->parent()->children()[index_in_parent + 1];
            AXNode* fat_tree_next_sibling =
                fat_tree_node->GetNextUnignoredSibling();
            EXPECT_TRUE(fat_tree_next_sibling);
            if (fat_tree_next_sibling) {
              EXPECT_EQ(skinny_tree_next_sibling->id(),
                        fat_tree_next_sibling->id());
            }
          }
        }
      }
    }
  }
}

}  // namespace ui
