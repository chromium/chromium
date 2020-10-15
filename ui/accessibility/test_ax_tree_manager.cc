// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/test_ax_tree_manager.h"

#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_manager_map.h"

namespace ui {

TestAXTreeManager::TestAXTreeManager() = default;

TestAXTreeManager::TestAXTreeManager(std::unique_ptr<AXTree> tree)
    : tree_(std::move(tree)) {
  AXTreeManagerMap::GetInstance().AddTreeManager(GetTreeID(), this);
}

TestAXTreeManager::~TestAXTreeManager() {
  if (tree_)
    AXTreeManagerMap::GetInstance().RemoveTreeManager(GetTreeID());
}

void TestAXTreeManager::DestroyTree() {
  if (!tree_)
    return;

  AXTreeManagerMap::GetInstance().RemoveTreeManager(GetTreeID());
  tree_.reset();
}

AXTree* TestAXTreeManager::GetTree() const {
  DCHECK(tree_) << "Did you forget to call SetTree?";
  return tree_.get();
}

void TestAXTreeManager::SetTree(std::unique_ptr<AXTree> tree) {
  if (tree_)
    AXTreeManagerMap::GetInstance().RemoveTreeManager(GetTreeID());

  tree_ = std::move(tree);
  AXTreeManagerMap::GetInstance().AddTreeManager(GetTreeID(), this);
}

AXNode* TestAXTreeManager::GetNodeFromTree(const AXTreeID tree_id,
                                           const AXNode::AXID node_id) const {
  return (tree_ && GetTreeID() == tree_id) ? tree_->GetFromId(node_id)
                                           : nullptr;
}

AXNode* TestAXTreeManager::GetNodeFromTree(const AXNode::AXID node_id) const {
  return tree_ ? tree_->GetFromId(node_id) : nullptr;
}

void TestAXTreeManager::AddObserver(AXTreeObserver* observer) {
  if (tree_)
    tree_->AddObserver(observer);
}

void TestAXTreeManager::RemoveObserver(AXTreeObserver* observer) {
  if (tree_)
    tree_->RemoveObserver(observer);
}

AXTreeID TestAXTreeManager::GetTreeID() const {
  return tree_ ? tree_->data().tree_id : AXTreeIDUnknown();
}

AXTreeID TestAXTreeManager::GetParentTreeID() const {
  return tree_ ? tree_->data().parent_tree_id : AXTreeIDUnknown();
}

AXNode* TestAXTreeManager::GetRootAsAXNode() const {
  return tree_ ? tree_->root() : nullptr;
}

AXNode* TestAXTreeManager::GetParentNodeFromParentTreeAsAXNode() const {
  ui::AXTreeID parent_tree_id = GetParentTreeID();
  TestAXTreeManager* parent_manager = static_cast<TestAXTreeManager*>(
      ui::AXTreeManagerMap::GetInstance().GetManager(parent_tree_id));
  if (!parent_manager)
    return nullptr;

  std::set<int32_t> host_node_ids =
      parent_manager->GetTree()->GetNodeIdsForChildTreeId(GetTreeID());

  for (int32_t host_node_id : host_node_ids) {
    ui::AXNode* parent_node =
        parent_manager->GetNodeFromTree(parent_tree_id, host_node_id);
    if (parent_node)
      return parent_node;
  }

  return nullptr;
}

}  // namespace ui
