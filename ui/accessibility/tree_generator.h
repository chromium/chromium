// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_TREE_GENERATOR_H_
#define UI_ACCESSIBILITY_TREE_GENERATOR_H_

#include <optional>
#include <vector>

#include "ui/accessibility/ax_tree_update_forward.h"

namespace ui {

class AXTree;

// This class is only used for fuzz testing.
//
// A class to create all possible trees with up to <n> nodes and the
// ids [1...n].
//
// There are two parts to the algorithm:
//
// The tree structure is formed as follows: without loss of generality,
// the first node becomes the root and the second node becomes its
// child. Thereafter, choose every possible parent for every other node.
//
// So for node i in (3...n), there are (i - 1) possible choices for its
// parent, for a total of (n-1)! (n minus 1 factorial) possible trees.
//
// The second optional part is the assignment of ids to the nodes in the tree.
// There are exactly n! (n factorial) permutations of the sequence 1...n,
// and each of these is assigned to every node in every possible tree.
//
// The total number of trees for a given <n>, including permutations of ids, is
// n! * (n-1)!
//
// n = 2: 2 trees
// n = 3: 12 trees
// n = 4: 144 trees
// n = 5: 2880 trees
//
// Note that the generator returns all trees with sizes *up to* <n>, which
// is a bit larger.
//
// This grows really fast! Still, it's very helpful for exhaustively testing
// tree code on smaller trees at least.
class TreeGenerator {
 public:
  // Compute total number of trees.
  static constexpr int ComputeUniqueTreeCount(int max_node_count,
                                              bool permutations) {
    int tree_count = 0;
    for (int i = 1; i <= max_node_count; ++i)
      tree_count += UniqueTreeCountForNodeCount(i, permutations);
    return tree_count;
  }

  // Will generate all trees with up to |max_node_count| nodes.
  // If |permutations| is true, will return every possible permutation of
  // ids, otherwise the root will always have id 1, and so on.
  TreeGenerator(int max_node_count, bool permutations);
  ~TreeGenerator();

  // Build all unique trees (no nodes ignored).
  int UniqueTreeCount() const;
  void BuildUniqueTree(int tree_index, AXTree* out_tree) const;

  // Support for returning every permutation of ignored nodes
  // (other than the root, which is never ignored) per unique tree.
  int IgnoredPermutationCountPerUniqueTree(int tree_index) const;

  // The focused node is never ignored, even if marked as such. To enable
  // testing how focus could affect the ignored state when unserializing
  // trees, the `focused_node` argument (if specified) indicates which node
  // should be focused.
  void BuildUniqueTreeWithIgnoredNodes(int tree_index,
                                       int ignored_index,
                                       std::optional<int> focused_node,
                                       AXTree* out_tree) const;

 private:
  static constexpr int UniqueTreeCountForNodeCount(int node_count,
                                                   bool permutations) {
    int unique_tree_count = 1;

    // (n-1)! for the possible trees.
    for (int i = 2; i < node_count; ++i)
      unique_tree_count *= i;

    // n! for the permutations of ids.
    if (permutations)
      unique_tree_count = unique_tree_count * unique_tree_count * node_count;

    return unique_tree_count;
  }

  void BuildUniqueTreeUpdate(int tree_index,
                             AXTreeUpdate* out_tree_update) const;
  void BuildUniqueTreeUpdateWithSize(int node_count,
                                     int tree_index,
                                     AXTreeUpdate* out_tree_update) const;

  int max_node_count_;
  bool permutations_;
  int total_unique_tree_count_;
  std::vector<int> unique_tree_count_by_size_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_TREE_GENERATOR_H_
