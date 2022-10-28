// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/test_ax_tree_manager.h"

#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/test_ax_tree_update.h"

namespace ui {

TestAXTreeManager::TestAXTreeManager() = default;

TestAXTreeManager::TestAXTreeManager(std::unique_ptr<AXTree> tree)
    : AXTreeManager(std::move(tree)) {}

TestAXTreeManager::TestAXTreeManager(TestAXTreeManager&& manager)
    : AXTreeManager(std::move(manager.ax_tree_)) {
  if (ax_tree_) {
    GetMap().RemoveTreeManager(GetTreeID());
    GetMap().AddTreeManager(GetTreeID(), this);
  }
}

TestAXTreeManager& TestAXTreeManager::operator=(TestAXTreeManager&& manager) {
  if (this == &manager)
    return *this;
  if (manager.ax_tree_)
    GetMap().RemoveTreeManager(manager.GetTreeID());
  // std::move(nullptr) == nullptr, so no need to check if `manager.tree_` is
  // assigned.
  SetTree(std::move(manager.ax_tree_));
  return *this;
}

TestAXTreeManager::~TestAXTreeManager() = default;

void TestAXTreeManager::DestroyTree() {
  if (!ax_tree_)
    return;

  GetMap().RemoveTreeManager(GetTreeID());
  ax_tree_.reset();
}

AXTree* TestAXTreeManager::GetTree() const {
  DCHECK(ax_tree_) << "Did you forget to call SetTree?";
  return ax_tree_.get();
}

void TestAXTreeManager::SetTree(std::unique_ptr<AXTree> tree) {
  if (ax_tree_)
    GetMap().RemoveTreeManager(GetTreeID());

  ax_tree_ = std::move(tree);
  ax_tree_id_ = GetTreeID();
  if (ax_tree_)
    GetMap().AddTreeManager(GetTreeID(), this);
}

AXTree* TestAXTreeManager::Init(AXTreeUpdate tree_update) {
  tree_update.has_tree_data = true;
  if (tree_update.tree_data.tree_id == AXTreeIDUnknown())
    tree_update.tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  SetTree(std::make_unique<AXTree>(tree_update));
  return ax_tree_.get();
}

AXTree* TestAXTreeManager::Init(const TestAXTreeUpdateNode& tree_update_root) {
  return Init(TestAXTreeUpdate(tree_update_root));
}

AXTree* TestAXTreeManager::Init(
    const ui::AXNodeData& node1,
    const ui::AXNodeData& node2 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node3 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node4 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node5 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node6 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node7 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node8 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node9 /* = AXNodeData() */,
    const ui::AXNodeData& node10 /* = AXNodeData() */,
    const ui::AXNodeData& node11 /* = AXNodeData() */,
    const ui::AXNodeData& node12 /* = AXNodeData() */) {
  AXTreeUpdate update;
  update.root_id = node1.id;
  update.tree_data.title = "Dialog title";
  update.nodes.push_back(node1);
  if (node2.id != kInvalidAXNodeID)
    update.nodes.push_back(node2);
  if (node3.id != kInvalidAXNodeID)
    update.nodes.push_back(node3);
  if (node4.id != kInvalidAXNodeID)
    update.nodes.push_back(node4);
  if (node5.id != kInvalidAXNodeID)
    update.nodes.push_back(node5);
  if (node6.id != kInvalidAXNodeID)
    update.nodes.push_back(node6);
  if (node7.id != kInvalidAXNodeID)
    update.nodes.push_back(node7);
  if (node8.id != kInvalidAXNodeID)
    update.nodes.push_back(node8);
  if (node9.id != kInvalidAXNodeID)
    update.nodes.push_back(node9);
  if (node10.id != kInvalidAXNodeID)
    update.nodes.push_back(node10);
  if (node11.id != kInvalidAXNodeID)
    update.nodes.push_back(node11);
  if (node12.id != kInvalidAXNodeID)
    update.nodes.push_back(node12);
  return Init(update);
}

AXNodePosition::AXPositionInstance TestAXTreeManager::CreateTreePosition(
    const AXNode& anchor,
    int child_index) const {
  return AXNodePosition::CreateTreePosition(anchor, child_index);
}

AXNodePosition::AXPositionInstance TestAXTreeManager::CreateTreePosition(
    const AXTree* tree,
    const AXNodeData& anchor_data,
    int child_index) const {
  const AXNode* anchor = tree->GetFromId(anchor_data.id);
  return CreateTreePosition(*anchor, child_index);
}

AXNodePosition::AXPositionInstance TestAXTreeManager::CreateTreePosition(
    const AXNodeData& anchor_data,
    int child_index) const {
  return CreateTreePosition(ax_tree(), anchor_data, child_index);
}

AXNodePosition::AXPositionInstance TestAXTreeManager::CreateTextPosition(
    const AXNode& anchor,
    int text_offset,
    ax::mojom::TextAffinity affinity) const {
  return AXNodePosition::CreateTextPosition(anchor, text_offset, affinity);
}

AXNodePosition::AXPositionInstance TestAXTreeManager::CreateTextPosition(
    const AXTree* tree,
    const AXNodeData& anchor_data,
    int text_offset,
    ax::mojom::TextAffinity affinity) const {
  const AXNode* anchor = tree->GetFromId(anchor_data.id);
  return CreateTextPosition(*anchor, text_offset, affinity);
}

AXNodePosition::AXPositionInstance TestAXTreeManager::CreateTextPosition(
    const AXNodeData& anchor_data,
    int text_offset,
    ax::mojom::TextAffinity affinity) const {
  return CreateTextPosition(ax_tree(), anchor_data, text_offset, affinity);
}

AXNodePosition::AXPositionInstance TestAXTreeManager::CreateTextPosition(
    const AXNodeID& anchor_id,
    int text_offset,
    ax::mojom::TextAffinity affinity) const {
  const AXNode* anchor = ax_tree()->GetFromId(anchor_id);
  return CreateTextPosition(*anchor, text_offset, affinity);
}

AXNode* TestAXTreeManager::GetParentNodeFromParentTree() const {
  AXTreeID parent_tree_id = GetParentTreeID();
  TestAXTreeManager* parent_manager =
      static_cast<TestAXTreeManager*>(AXTreeManager::FromID(parent_tree_id));
  if (!parent_manager)
    return nullptr;

  std::set<AXNodeID> host_node_ids =
      parent_manager->GetTree()->GetNodeIdsForChildTreeId(GetTreeID());

  for (AXNodeID host_node_id : host_node_ids) {
    AXNode* parent_node =
        parent_manager->GetNodeFromTree(parent_tree_id, host_node_id);
    if (parent_node)
      return parent_node;
  }

  return nullptr;
}

}  // namespace ui
