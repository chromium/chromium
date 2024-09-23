// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/accessibility/ax_tree_fuzzer_util.h"

#include <vector>

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
                                                  int node_count) {
  max_assigned_node_id_ = 1;
  ui::AXTreeUpdate initial_state;
  initial_state.root_id = max_assigned_node_id_++;

  initial_state.has_tree_data = true;
  initial_state.tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();

  ui::AXNodeData root;
  root.id = initial_state.root_id;
  root.role = ax::mojom::Role::kRootWebArea;

  std::stack<size_t> parent_index_stack;
  parent_index_stack.push(initial_state.nodes.size());
  initial_state.nodes.push_back(root);

  // As we give out ids sequentially, starting at 1, the
  // ...max_assigned_node_id_... is equivalent to the node count.
  while (fuzz_data.RemainingBytes() >= kMinimumNewNodeFuzzDataSize &&
         max_assigned_node_id_ < node_count) {
    size_t extra_data_size =
        fuzz_data.RemainingBytes() - kMinimumNewNodeFuzzDataSize;

    ui::AXNodeData& parent = initial_state.nodes[parent_index_stack.top()];

    // Create a node.
    ui::AXNodeData node = CreateChildNodeData(parent, max_assigned_node_id_++);

    // Determine role.
    node.role = GetInterestingRole(fuzz_data.NextByte(), parent.role);

    // Add role-specific properties.
    AddRoleSpecificProperties(
        fuzz_data, node,
        parent.GetStringAttribute(ax::mojom::StringAttribute::kName),
        extra_data_size);

    // Determine the relationship of the next node from fuzz data. See
    // implementation of `DetermineNextNodeRelationship` for details.
    size_t ancestor_pop_count;
    switch (DetermineNextNodeRelationship(node.role, fuzz_data.NextByte())) {
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

// Pre-order depth first walk of tree. Skip over deleted subtrees.
void AXTreeFuzzerGenerator::RecursiveGenerateUpdate(
    const ui::AXNode* node,
    ui::AXTreeUpdate& tree_update,
    FuzzerData& fuzz_data,
    std::set<ui::AXNodeID>& updated_nodes) {
  // Stop traversing if we run out of fuzz data.
  if (fuzz_data.RemainingBytes() <= kMinimumNewNodeFuzzDataSize)
    return;
  size_t extra_data_size =
      fuzz_data.RemainingBytes() - kMinimumNewNodeFuzzDataSize;

  AXTreeFuzzerGenerator::TreeUpdateOperation operation = kNoOperation;
  if (!updated_nodes.count(node->id()))
    operation = DetermineTreeUpdateOperation(node, fuzz_data.NextByte());

  switch (operation) {
    case kAddChild: {
      // Determine where to insert the node.
      // Create node and attach to parent.
      ui::AXNodeData parent = node->data();
      ui::AXNodeData child =
          CreateChildNodeData(parent, max_assigned_node_id_++);

      // Determine role.
      child.role = GetInterestingRole(fuzz_data.NextByte(), node->GetRole());

      // Add role-specific properties.
      AddRoleSpecificProperties(
          fuzz_data, child,
          node->GetStringAttribute(ax::mojom::StringAttribute::kName),
          extra_data_size);
      // Also add inline text child if we can.
      ui::AXNodeData inline_text_data;
      if (ui::CanHaveInlineTextBoxChildren(child.role)) {
        inline_text_data = CreateChildNodeData(child, max_assigned_node_id_++);
        inline_text_data.role = ax::mojom::Role::kInlineTextBox;
        inline_text_data.SetName(
            child.GetStringAttribute(ax::mojom::StringAttribute::kName));
      }
      // Add both the current node (parent) and the child to the tree update.
      tree_update.nodes.push_back(parent);
      tree_update.nodes.push_back(child);
      updated_nodes.emplace(parent.id);
      updated_nodes.emplace(child.id);
      if (inline_text_data.id != ui::kInvalidAXNodeID) {
        tree_update.nodes.push_back(inline_text_data);
        updated_nodes.emplace(inline_text_data.id);
      }
      break;
    }
    case kRemoveNode: {
      const ui::AXNode* parent = node->GetParent();
      if (updated_nodes.count(parent->id()))
        break;
      // Determine what node to delete.
      // To delete a node, just find the parent and update the child list to
      // no longer include this node.
      ui::AXNodeData parent_update = parent->data();
      std::erase(parent_update.child_ids, node->id());
      tree_update.nodes.push_back(parent_update);
      updated_nodes.emplace(parent_update.id);

      // This node was deleted, don't traverse to the subtree.
      return;
    }
    case kTextChange: {
      // Modify the text.
      const ui::AXNode* child_inline_text = node->GetFirstChild();
      if (!child_inline_text ||
          child_inline_text->GetRole() != ax::mojom::Role::kInlineTextBox) {
        break;
      }
      ui::AXNodeData static_text_data = node->data();
      ui::AXNodeData inline_text_data = child_inline_text->data();
      size_t text_size =
          kMinTextFuzzDataSize + fuzz_data.NextByte() % kMaxTextFuzzDataSize;
      if (text_size > extra_data_size)
        text_size = extra_data_size;
      extra_data_size -= text_size;
      inline_text_data.SetName(
          GenerateInterestingText(fuzz_data.NextBytes(text_size), text_size));
      static_text_data.SetName(inline_text_data.GetStringAttribute(
          ax::mojom::StringAttribute::kName));
      tree_update.nodes.push_back(static_text_data);
      tree_update.nodes.push_back(inline_text_data);
      updated_nodes.emplace(static_text_data.id);
      updated_nodes.emplace(inline_text_data.id);
      break;
    }
    case kNoOperation:
      break;
  }

  // Visit subtree.
  for (auto iter = node->AllChildrenBegin(); iter != node->AllChildrenEnd();
       ++iter) {
    RecursiveGenerateUpdate(iter.get(), tree_update, fuzz_data, updated_nodes);
  }
}

// When building a tree update, we must take care to not create an
// unserializable tree. If the tree does not serialize, things like
// TestAXTreeObserver will not be able to handle the incorrectly serialized
// tree. This will require us to abort the fuzz run.
bool AXTreeFuzzerGenerator::GenerateTreeUpdate(FuzzerData& fuzz_data,
                                               size_t node_count) {
  ui::AXTreeUpdate tree_update;
  std::set<ui::AXNodeID> updated_nodes;
  RecursiveGenerateUpdate(tree_manager_.GetRoot(), tree_update, fuzz_data,
                          updated_nodes);
  return GetTree()->Unserialize(tree_update);
}

ui::AXNodeID AXTreeFuzzerGenerator::GetMaxAssignedID() const {
  return max_assigned_node_id_;
}

ui::AXNodeData AXTreeFuzzerGenerator::CreateChildNodeData(
    ui::AXNodeData& parent,
    ui::AXNodeID new_node_id) {
  ui::AXNodeData node;
  node.id = new_node_id;
  // Connect parent to this node.
  parent.child_ids.push_back(node.id);
  return node;
}

// Determine the relationship of the next node from fuzz data.
AXTreeFuzzerGenerator::NextNodeRelationship
AXTreeFuzzerGenerator::DetermineNextNodeRelationship(ax::mojom::Role role,
                                                     unsigned char byte) {
  // Force this to have a inline text child if it can.
  if (ui::CanHaveInlineTextBoxChildren(role))
    return NextNodeRelationship::kChild;

  // Don't allow inline text boxes to have children or siblings.
  if (role == ax::mojom::Role::kInlineTextBox)
    return NextNodeRelationship::kSiblingToAncestor;

  // Determine next node using fuzz data.
  NextNodeRelationship relationship =
      static_cast<NextNodeRelationship>(byte % 3);

  // Check to ensure we can have children.
  if (relationship == NextNodeRelationship::kChild && !CanHaveChildren(role)) {
    return NextNodeRelationship::kSibling;
  }
  return relationship;
}

AXTreeFuzzerGenerator::TreeUpdateOperation
AXTreeFuzzerGenerator::DetermineTreeUpdateOperation(const ui::AXNode* node,
                                                    unsigned char byte) {
  switch (byte % 4) {
    case 0:
      // Don't delete the following nodes:
      // 1) The root. TODO(janewman): implement root changes in an update.
      // 2) Inline text. We don't want to leave Static text nodes without inline
      // text children.
      if (ax::mojom::Role::kRootWebArea != node->GetRole())
        return kRemoveNode;
      ABSL_FALLTHROUGH_INTENDED;
    case 1:
      // Check to ensure this node can have children. Also consider that we
      // shouldn't add children to static text, as these nodes only expect to
      // have a inline text single child.
      if (CanHaveChildren(node->GetRole()) && !ui::IsText(node->GetRole()))
        return kAddChild;
      ABSL_FALLTHROUGH_INTENDED;
    case 2:
      if (ax::mojom::Role::kStaticText == node->GetRole())
        return kTextChange;
      ABSL_FALLTHROUGH_INTENDED;
    default:
      return kNoOperation;
  }
}

void AXTreeFuzzerGenerator::AddRoleSpecificProperties(
    FuzzerData& fuzz_data,
    ui::AXNodeData& node,
    const std::string& parentName,
    size_t extra_data_size) {
  // TODO(janewman): Add ignored state.
  // Add role-specific properties.
  if (node.role == ax::mojom::Role::kInlineTextBox) {
    node.SetName(parentName);
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
