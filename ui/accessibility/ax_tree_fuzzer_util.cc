// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_fuzzer_util.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"

using TestPositionType =
    std::unique_ptr<ui::AXPosition<ui::AXNodePosition, ui::AXNode>>;
using TestPositionRange =
    ui::AXRange<ui::AXPosition<ui::AXNodePosition, ui::AXNode>>;

FuzzerData::FuzzerData(const unsigned char* data, size_t size)
    : data_(data), data_size_(size), data_index_(0) {}

size_t FuzzerData::RemainingBytes() {
  return data_size_ - data_index_;
}

unsigned char FuzzerData::NextByte() {
  CHECK(RemainingBytes());
  return data_[data_index_++];
}

const unsigned char* FuzzerData::NextBytes(size_t amount) {
  CHECK(RemainingBytes() >= amount);
  const unsigned char* current_position = &data_[data_index_];
  data_index_ += amount;
  return current_position;
}

ui::AXTree* AXTreeFuzzerGenerator::GetTree() {
  return tree_manager_.GetTree();
}

void AXTreeFuzzerGenerator::GenerateInitialUpdate(FuzzerData& fuzz_data,
                                                  int node_count,
                                                  ui::AXNodeID& max_node_id) {
  max_node_id = 1;
  ui::AXTreeUpdate initial_state;
  initial_state.root_id = max_node_id++;

  initial_state.has_tree_data = true;
  initial_state.tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();

  ui::AXNodeData root;
  root.id = initial_state.root_id;
  root.role = ax::mojom::Role::kRootWebArea;

  std::stack<size_t> parent_index_stack;
  parent_index_stack.push(initial_state.nodes.size());
  initial_state.nodes.push_back(root);

  // As we give out ids sequentially, starting at 1, the max_node_id is
  // equivalent to the node count.
  while (fuzz_data.RemainingBytes() >= kMinimumNewNodeFuzzDataSize &&
         max_node_id < node_count) {
    size_t extra_data_size =
        fuzz_data.RemainingBytes() - kMinimumNewNodeFuzzDataSize;

    // Create a node.
    ui::AXNodeData node;
    node.id = max_node_id++;
    // Connect parent to this node.
    ui::AXNodeData& parent = initial_state.nodes[parent_index_stack.top()];
    parent.child_ids.push_back(node.id);

    // Determine role.
    node.role = GetInterestingRole(fuzz_data.NextByte(), parent.role);

    // Add role-specific properties.
    if (node.role == ax::mojom::Role::kInlineTextBox) {
      node.SetName(
          parent.GetStringAttribute(ax::mojom::StringAttribute::kName));
    } else if (node.role == ax::mojom::Role::kLineBreak) {
      node.SetName("\n");
    } else if (ui::IsText(node.role)) {
      size_t text_size =
          kMinTextFuzzDataSize + fuzz_data.NextByte() % kMaxTextFuzzDataSize;
      if (text_size > extra_data_size)
        text_size = extra_data_size;
      extra_data_size -= text_size;
      node.SetName(
          GenerateInterestingText(fuzz_data.NextBytes(text_size), text_size));
    }

    enum NextNodeRelationship {
      // Next node is a child of this node. (This node is a parent.)
      kChild,
      // Next node is sibling to this node. (This node is a leaf.)
      kSibling,
      // Next node is sibling to an ancestor. (This node is a leaf.)
      kSiblingToAncestor
    };

    NextNodeRelationship next_node_relationship;
    // Determine the relationship of the next node from fuzz data.
    if (ui::CanHaveInlineTextBoxChildren(node.role)) {
      // Force this to have a inline text child.
      next_node_relationship = NextNodeRelationship::kChild;
    } else if (node.role == ax::mojom::Role::kInlineTextBox) {
      // Force next node to be a sibling to an ancestor.
      next_node_relationship = NextNodeRelationship::kSiblingToAncestor;
    } else {
      // Determine next node using fuzz data.
      switch (fuzz_data.NextByte() % 3) {
        case 0:
          if (CanHaveChildren(node.role)) {
            next_node_relationship = NextNodeRelationship::kChild;
            break;
          }
          ABSL_FALLTHROUGH_INTENDED;
        case 1:
          next_node_relationship = NextNodeRelationship::kSibling;
          break;
        case 2:
          next_node_relationship = NextNodeRelationship::kSiblingToAncestor;
          break;
      }
    }

    size_t ancestor_pop_count;
    switch (next_node_relationship) {
      case kChild:
        CHECK(CanHaveChildren(node.role));
        parent_index_stack.push(initial_state.nodes.size());
        break;
      case kSibling:
        initial_state.nodes.push_back(node);
        break;
      case kSiblingToAncestor:
        ancestor_pop_count = 1 + fuzz_data.NextByte() % kMaxAncestorPopCount;
        for (size_t i = 0;
             i < ancestor_pop_count && parent_index_stack.size() > 1; ++i) {
          parent_index_stack.pop();
        }
        break;
    }

    initial_state.nodes.push_back(node);
  }
  // Run with --v=1 to aid in debugging a specific crash.
  VLOG(1) << "Input accessibility tree:\n" << initial_state.ToString();
  tree_manager_.SetTree(std::make_unique<ui::AXTree>(initial_state));
}

ax::mojom::Role AXTreeFuzzerGenerator::GetInterestingRole(
    unsigned char byte,
    ax::mojom::Role parent_role) {
  if (ui::CanHaveInlineTextBoxChildren(parent_role))
    return ax::mojom::Role::kInlineTextBox;

  // Bias towards creating text nodes so we end up with more text in the tree.
  switch (byte % 7) {
    default:
    case 0:
    case 1:
    case 2:
      return ax::mojom::Role::kStaticText;
    case 3:
      return ax::mojom::Role::kLineBreak;
    case 4:
      return ax::mojom::Role::kParagraph;
    case 5:
      return ax::mojom::Role::kGenericContainer;
    case 6:
      return ax::mojom::Role::kGroup;
  }
}

bool AXTreeFuzzerGenerator::CanHaveChildren(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kInlineTextBox:
      return false;
    default:
      return true;
  }
}

std::u16string AXTreeFuzzerGenerator::GenerateInterestingText(
    const unsigned char* data,
    size_t size) {
  std::u16string wide_str;
  for (size_t i = 0; i + 1 < size; i += 2) {
    char16_t char_16 = data[i] << 8;
    char_16 |= data[i + 1];
    // Don't insert a null character.
    if (char_16)
      wide_str.push_back(char_16);
  }
  return wide_str;
}
