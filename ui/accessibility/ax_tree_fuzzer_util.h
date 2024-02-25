// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef UI_ACCESSIBILITY_AX_TREE_FUZZER_UTIL_H_
#define UI_ACCESSIBILITY_AX_TREE_FUZZER_UTIL_H_

#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/test_single_ax_tree_manager.h"

// TODO(janewman): Replace usage with ...FuzzedDataProvider...
class FuzzerData {
 public:
  FuzzerData(const unsigned char* data, size_t size);
  size_t RemainingBytes();
  unsigned char NextByte();
  const unsigned char* NextBytes(size_t amount);

 private:
  const unsigned char* data_;
  const size_t data_size_;
  size_t data_index_;
};

class AXTreeFuzzerGenerator {
 public:
  AXTreeFuzzerGenerator() = default;
  ~AXTreeFuzzerGenerator() = default;

  ui::AXTree* GetTree();

  void GenerateInitialUpdate(FuzzerData& fuzz_data, int node_count);
  bool GenerateTreeUpdate(FuzzerData& fuzz_data, size_t node_count);

  ui::AXNodeID GetMaxAssignedID() const;

  // This must be kept in sync with the minimum amount of data needed to create
  // any node. Any optional node data should check to ensure there is space.
  static constexpr size_t kMinimumNewNodeFuzzDataSize = 5;
  static constexpr size_t kMinTextFuzzDataSize = 10;
  static constexpr size_t kMaxTextFuzzDataSize = 200;

  // When creating a node, we allow for the next node to be a sibling of an
  // ancestor, this constant determines the maximum nodes we will pop when
  // building the tree.
  static constexpr size_t kMaxAncestorPopCount = 3;

 private:
  enum NextNodeRelationship {
    // Next node is a child of this node. (This node is a parent.)
    kChild,
    // Next node is sibling to this node. (This node is a leaf.)
    kSibling,
    // Next node is sibling to an ancestor. (This node is a leaf.)
    kSiblingToAncestor,
  };
  enum TreeUpdateOperation {
    kAddChild,
    kRemoveNode,
    kTextChange,
    kNoOperation
  };

  void RecursiveGenerateUpdate(const ui::AXNode* node,
                               ui::AXTreeUpdate& tree_update,
                               FuzzerData& fuzz_data,
                               std::set<ui::AXNodeID>& updated_nodes);
  // TODO(janewman): Many of these can be made static.
  ui::AXNodeData CreateChildNodeData(ui::AXNodeData& parent,
                                     ui::AXNodeID new_node_id);
  NextNodeRelationship DetermineNextNodeRelationship(ax::mojom::Role role,
                                                     unsigned char byte);
  TreeUpdateOperation DetermineTreeUpdateOperation(const ui::AXNode* node,
                                                   unsigned char byte);
  void AddRoleSpecificProperties(FuzzerData& fuzz_data,
                                 ui::AXNodeData& node,
                                 const std::string& parentName,
                                 size_t extra_data_size);
  ax::mojom::Role GetInterestingRole(unsigned char byte,
                                     ax::mojom::Role parent_role);
  bool CanHaveChildren(ax::mojom::Role role);
  bool CanHaveText(ax::mojom::Role role);
  std::u16string GenerateInterestingText(const unsigned char* data,
                                         size_t size);
  ui::AXNodeID max_assigned_node_id_;
  ui::TestSingleAXTreeManager tree_manager_;
};
#endif  //  UI_ACCESSIBILITY_AX_TREE_FUZZER_UTIL_H_
