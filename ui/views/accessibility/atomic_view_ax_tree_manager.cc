// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/atomic_view_ax_tree_manager.h"

#include <memory>
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/ax_tree_update.h"

namespace views {

// static
std::unique_ptr<AtomicViewAXTreeManager> AtomicViewAXTreeManager::Create(
    ViewAXPlatformNodeDelegate* delegate,
    ui::AXNodeData node_data) {
  auto view_tree_manager = base::WrapUnique<AtomicViewAXTreeManager>(
      new AtomicViewAXTreeManager(delegate, node_data));
  if (view_tree_manager->ax_tree() == nullptr) {
    return nullptr;
  }
  return view_tree_manager;
}

AtomicViewAXTreeManager::AtomicViewAXTreeManager(
    ViewAXPlatformNodeDelegate* delegate,
    ui::AXNodeData node_data)
    : AXPlatformTreeManager(nullptr), delegate_(delegate) {
  DCHECK(delegate);
  if (!ui::IsText(node_data.role) && !node_data.IsAtomicTextField()) {
    return;
  }

  ui::AXTreeData tree_data = ui::AXTreeData();
  tree_data.tree_id = GetTreeID();
  tree_data.focused_tree_id = tree_data.tree_id;

  ui::AXTreeUpdate initial_state;
  initial_state.tree_data = tree_data;
  initial_state.tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  initial_state.has_tree_data = true;
  initial_state.root_id = node_data.id;
  initial_state.nodes = {node_data};
  ax_tree_ = std::make_unique<ui::AXTree>(initial_state);
  if (HasValidTreeID()) {
    GetMap().AddTreeManager(GetTreeID(), this);
  }
}

AtomicViewAXTreeManager::~AtomicViewAXTreeManager() = default;

bool AtomicViewAXTreeManager::IsView() const {
  return true;
}

ui::AXNode* AtomicViewAXTreeManager::GetNodeFromTree(
    const ui::AXTreeID& tree_id,
    const ui::AXNodeID node_id) const {
  return GetNode(node_id);
}

ui::AXNode* AtomicViewAXTreeManager::GetNode(const ui::AXNodeID node_id) const {
  // This here is the key to the whole thing. The AtomicViewAXTreeManager is
  // fetching and updating the AXNodeData from the View itself whenever this
  // function gets called.
  ax_tree_->root()->SetData(delegate_->GetData());
  DCHECK_EQ(node_id, ax_tree_->root()->id())
      << "The AtomicViewAXTreeManager should only allow callers to get the "
         "root node as it is the only managed node.";
  return ax_tree_->root();
}

ui::AXPlatformNode* AtomicViewAXTreeManager::GetPlatformNodeFromTree(
    const ui::AXNodeID node_id) const {
  return delegate_->GetFromNodeID(node_id);
}

ui::AXPlatformNode* AtomicViewAXTreeManager::GetPlatformNodeFromTree(
    const ui::AXNode& node) const {
  return GetPlatformNodeFromTree(node.id());
}

ui::AXPlatformNodeDelegate* AtomicViewAXTreeManager::RootDelegate() const {
  return delegate_;
}

ui::AXTreeID AtomicViewAXTreeManager::GetParentTreeID() const {
  return ui::AXTreeIDUnknown();
}

ui::AXNode* AtomicViewAXTreeManager::GetRoot() const {
  if (!ax_tree_) {
    return nullptr;
  }

  ax_tree_->root()->SetData(delegate_->GetData());
  return AXTreeManager::GetRoot();
}

ui::AXNode* AtomicViewAXTreeManager::GetParentNodeFromParentTree() const {
  return nullptr;
}

void AtomicViewAXTreeManager::ClearComputedRootData() {
  return ax_tree_->root()->ClearComputedNodeData();
}

}  // namespace views
