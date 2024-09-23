// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/tree_generator.h"

#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ui {

TreeGenerator::TreeGenerator(int max_node_count, bool permutations)
    : max_node_count_(max_node_count),
      permutations_(permutations),
      total_unique_tree_count_(0) {
  unique_tree_count_by_size_.push_back(0);
  for (int i = 1; i <= max_node_count; ++i) {
    int unique_tree_count = UniqueTreeCountForNodeCount(i, permutations);
    unique_tree_count_by_size_.push_back(unique_tree_count);
    total_unique_tree_count_ += unique_tree_count;
  }
}

TreeGenerator::~TreeGenerator() = default;

int TreeGenerator::UniqueTreeCount() const {
  return total_unique_tree_count_;
}

void TreeGenerator::BuildUniqueTree(int tree_index, AXTree* out_tree) const {
  AXTreeUpdate update;
  BuildUniqueTreeUpdate(tree_index, &update);
  CHECK(out_tree->Unserialize(update)) << out_tree->error();
}

int TreeGenerator::IgnoredPermutationCountPerUniqueTree(int tree_index) const {
  int unique_tree_count_so_far = 0;
  for (int node_count = 1; node_count <= max_node_count_; ++node_count) {
    int unique_tree_count = unique_tree_count_by_size_[node_count];
    if (tree_index - unique_tree_count_so_far < unique_tree_count) {
      // Each node other than the root can be either ignored or not,
      // so return 2 ^ (node_count - 1)
      return 1 << (node_count - 1);
    }
    unique_tree_count_so_far += unique_tree_count;
  }

  NOTREACHED_IN_MIGRATION();
  return 0;
}

void TreeGenerator::BuildUniqueTreeWithIgnoredNodes(
    int tree_index,
    int ignored_index,
    std::optional<int> focused_node,
    AXTree* out_tree) const {
  // Enable the behavior whereby all focused nodes will be exposed to the
  // platform accessibility layer. This behavior is currently disabled in
  // production code, but is enabled in tests so that it could be tested
  // thoroughly before it is turned on for all code.
  //
  // TODO(nektar): Turn this on in a followup patch.
  // AXTree::SetFocusedNodeShouldNeverBeIgnored();

  AXTreeUpdate update;
  BuildUniqueTreeUpdate(tree_index, &update);

  int node_count = static_cast<int>(update.nodes.size());
  CHECK_GE(ignored_index, 0);
  CHECK_LT(ignored_index, 1 << (node_count - 1));
  CHECK(!focused_node || *focused_node >= 0);
  CHECK(!focused_node || *focused_node < node_count);

  for (int i = 0; i < node_count - 1; i++) {
    if (ignored_index & (1 << i))
      update.nodes[i + 1].AddState(ax::mojom::State::kIgnored);
  }

  if (focused_node) {
    AXTreeData tree_data;
    tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
    tree_data.focused_tree_id = tree_data.tree_id;
    tree_data.focus_id = update.nodes[*focused_node].id;
    update.has_tree_data = true;
    update.tree_data = tree_data;
  }

  CHECK(out_tree->Unserialize(update)) << out_tree->error();
}

void TreeGenerator::BuildUniqueTreeUpdate(int tree_index,
                                          AXTreeUpdate* out_update) const {
  CHECK_LT(tree_index, total_unique_tree_count_);

  int unique_tree_count_so_far = 0;
  for (int node_count = 1; node_count <= max_node_count_; ++node_count) {
    int unique_tree_count = unique_tree_count_by_size_[node_count];
    if (tree_index - unique_tree_count_so_far < unique_tree_count) {
      BuildUniqueTreeUpdateWithSize(
          node_count, tree_index - unique_tree_count_so_far, out_update);
      return;
    }
    unique_tree_count_so_far += unique_tree_count;
  }
}

void TreeGenerator::BuildUniqueTreeUpdateWithSize(
    int node_count,
    int tree_index,
    AXTreeUpdate* out_update) const {
  std::vector<int> indices;
  std::vector<int> permuted;
  int unique_tree_count = unique_tree_count_by_size_[node_count];
  CHECK_LT(tree_index, unique_tree_count);

  if (permutations_) {
    // Use the first few bits of |tree_index| to permute the indices.
    for (int i = 0; i < node_count; ++i)
      indices.push_back(i + 1);
    for (int i = 0; i < node_count; ++i) {
      int p = (node_count - i);
      int index = tree_index % p;
      tree_index /= p;
      permuted.push_back(indices[index]);
      indices.erase(indices.begin() + index);
    }
  } else {
    for (int i = 0; i < node_count; ++i)
      permuted.push_back(i + 1);
  }

  // Build an AXTreeUpdate. The first two nodes of the tree always
  // go in the same place.
  out_update->root_id = permuted[0];
  out_update->nodes.resize(node_count);
  out_update->nodes[0].id = permuted[0];
  if (node_count > 1) {
    out_update->nodes[0].child_ids.push_back(permuted[1]);
    out_update->nodes[1].id = permuted[1];
  }

  // The remaining nodes are assigned based on their parent
  // selected from the next bits from |tree_index|.
  for (int i = 2; i < node_count; ++i) {
    out_update->nodes[i].id = permuted[i];
    int parent_index = (tree_index % i);
    tree_index /= i;
    out_update->nodes[parent_index].child_ids.push_back(permuted[i]);
  }
}

}  // namespace ui
