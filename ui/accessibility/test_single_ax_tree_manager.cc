// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/test_single_ax_tree_manager.h"

#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_manager_map.h"

namespace ui {

TestSingleAXTreeManager::TestSingleAXTreeManager() = default;

TestSingleAXTreeManager::TestSingleAXTreeManager(std::unique_ptr<AXTree> tree)
    : AXTreeManager(std::move(tree)) {}

TestSingleAXTreeManager::TestSingleAXTreeManager(
    TestSingleAXTreeManager&& manager)
    : AXTreeManager(std::move(manager.ax_tree_)) {
  if (ax_tree_) {
    GetMap().RemoveTreeManager(GetTreeID());
    GetMap().AddTreeManager(GetTreeID(), this);
  }
}

TestSingleAXTreeManager& TestSingleAXTreeManager::operator=(
    TestSingleAXTreeManager&& manager) {
  if (this == &manager) {
    return *this;
  }
  if (manager.HasValidTreeID()) {
    GetMap().RemoveTreeManager(manager.GetTreeID());
  }
  // std::move(nullptr) == nullptr, so no need to check if `manager.tree_` is
  // assigned.
  SetTree(std::move(manager.ax_tree_));
  return *this;
}

TestSingleAXTreeManager::~TestSingleAXTreeManager() = default;

void TestSingleAXTreeManager::DestroyTree() {
  if (HasValidTreeID()) {
    GetMap().RemoveTreeManager(GetTreeID());
  }
  ax_tree_.reset();
}

AXTree* TestSingleAXTreeManager::GetTree() const {
  DCHECK(ax_tree_) << "Did you forget to call SetTree?";
  return ax_tree_.get();
}

void TestSingleAXTreeManager::SetTree(std::unique_ptr<AXTree> tree) {
  if (HasValidTreeID()) {
    GetMap().RemoveTreeManager(GetTreeID());
  }

  ax_tree_ = std::move(tree);
  if (HasValidTreeID()) {
    GetMap().AddTreeManager(GetTreeID(), this);
  }
}

AXTree* TestSingleAXTreeManager::Init(AXTreeUpdate tree_update) {
  tree_update.has_tree_data = true;
  if (tree_update.tree_data.tree_id == AXTreeIDUnknown()) {
    tree_update.tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  }
  SetTree(std::make_unique<AXTree>(tree_update));
  return ax_tree_.get();
}

AXTree* TestSingleAXTreeManager::Init(
    const AXNodeData& node1,
    const AXNodeData& node2 /* = AXNodeData() */,
    const AXNodeData& node3 /* = AXNodeData() */,
    const AXNodeData& node4 /* = AXNodeData() */,
    const AXNodeData& node5 /* = AXNodeData() */,
    const AXNodeData& node6 /* = AXNodeData() */,
    const AXNodeData& node7 /* = AXNodeData() */,
    const AXNodeData& node8 /* = AXNodeData() */,
    const AXNodeData& node9 /* = AXNodeData() */,
    const AXNodeData& node10 /* = AXNodeData() */,
    const AXNodeData& node11 /* = AXNodeData() */,
    const AXNodeData& node12 /* = AXNodeData() */) {
  AXTreeUpdate update;
  update.root_id = node1.id;
  update.tree_data.title = "Dialog title";
  update.nodes.push_back(node1);
  if (node2.id != kInvalidAXNodeID) {
    update.nodes.push_back(node2);
  }
  if (node3.id != kInvalidAXNodeID) {
    update.nodes.push_back(node3);
  }
  if (node4.id != kInvalidAXNodeID) {
    update.nodes.push_back(node4);
  }
  if (node5.id != kInvalidAXNodeID) {
    update.nodes.push_back(node5);
  }
  if (node6.id != kInvalidAXNodeID) {
    update.nodes.push_back(node6);
  }
  if (node7.id != kInvalidAXNodeID) {
    update.nodes.push_back(node7);
  }
  if (node8.id != kInvalidAXNodeID) {
    update.nodes.push_back(node8);
  }
  if (node9.id != kInvalidAXNodeID) {
    update.nodes.push_back(node9);
  }
  if (node10.id != kInvalidAXNodeID) {
    update.nodes.push_back(node10);
  }
  if (node11.id != kInvalidAXNodeID) {
    update.nodes.push_back(node11);
  }
  if (node12.id != kInvalidAXNodeID) {
    update.nodes.push_back(node12);
  }
  return Init(update);
}

AXNodePosition::AXPositionInstance TestSingleAXTreeManager::CreateTreePosition(
    const AXNode& anchor,
    int child_index) const {
  return AXNodePosition::CreateTreePosition(anchor, child_index);
}

AXNodePosition::AXPositionInstance TestSingleAXTreeManager::CreateTreePosition(
    const AXTree* tree,
    const AXNodeData& anchor_data,
    int child_index) const {
  const AXNode* anchor = tree->GetFromId(anchor_data.id);
  return CreateTreePosition(*anchor, child_index);
}

AXNodePosition::AXPositionInstance TestSingleAXTreeManager::CreateTreePosition(
    const AXNodeData& anchor_data,
    int child_index) const {
  return CreateTreePosition(ax_tree(), anchor_data, child_index);
}

AXNodePosition::AXPositionInstance TestSingleAXTreeManager::CreateTextPosition(
    const AXNode& anchor,
    int text_offset,
    ax::mojom::TextAffinity affinity) const {
  return AXNodePosition::CreateTextPosition(anchor, text_offset, affinity);
}

AXNodePosition::AXPositionInstance TestSingleAXTreeManager::CreateTextPosition(
    const AXTree* tree,
    const AXNodeData& anchor_data,
    int text_offset,
    ax::mojom::TextAffinity affinity) const {
  const AXNode* anchor = tree->GetFromId(anchor_data.id);
  return CreateTextPosition(*anchor, text_offset, affinity);
}

AXNodePosition::AXPositionInstance TestSingleAXTreeManager::CreateTextPosition(
    const AXNodeData& anchor_data,
    int text_offset,
    ax::mojom::TextAffinity affinity) const {
  return CreateTextPosition(ax_tree(), anchor_data, text_offset, affinity);
}

AXNodePosition::AXPositionInstance TestSingleAXTreeManager::CreateTextPosition(
    const AXNodeID& anchor_id,
    int text_offset,
    ax::mojom::TextAffinity affinity) const {
  const AXNode* anchor = ax_tree()->GetFromId(anchor_id);
  return CreateTextPosition(*anchor, text_offset, affinity);
}

AXNode* TestSingleAXTreeManager::GetParentNodeFromParentTree() const {
  AXTreeID parent_tree_id = GetParentTreeID();
  TestSingleAXTreeManager* parent_manager =
      static_cast<TestSingleAXTreeManager*>(
          AXTreeManager::FromID(parent_tree_id));
  if (!parent_manager) {
    return nullptr;
  }

  std::set<AXNodeID> host_node_ids =
      parent_manager->GetTree()->GetNodeIdsForChildTreeId(GetTreeID());

  for (AXNodeID host_node_id : host_node_ids) {
    AXNode* parent_node =
        parent_manager->GetNodeFromTree(parent_tree_id, host_node_id);
    if (parent_node) {
      return parent_node;
    }
  }

  return nullptr;
}

}  // namespace ui
