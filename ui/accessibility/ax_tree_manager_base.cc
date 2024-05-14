// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_manager_base.h"

#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "ui/accessibility/ax_node.h"

namespace ui {

// static
AXTreeManagerBase* AXTreeManagerBase::GetManager(const AXTreeID& tree_id) {
  if (tree_id.type() == ax::mojom::AXTreeIDType::kUnknown)
    return nullptr;
  auto iter = GetTreeManagerMapInstance().find(tree_id);
  if (iter == GetTreeManagerMapInstance().end())
    return nullptr;
  return iter->second;
}

// static
std::unordered_map<AXTreeID, AXTreeManagerBase*, AXTreeIDHash>&
AXTreeManagerBase::GetTreeManagerMapInstance() {
  static base::NoDestructor<
      std::unordered_map<AXTreeID, AXTreeManagerBase*, AXTreeIDHash>>
      map_instance;
  return *map_instance;
}

AXTreeManagerBase::AXTreeManagerBase() = default;

AXTreeManagerBase::AXTreeManagerBase(std::unique_ptr<AXTree> tree) {
  if (!tree)
    return;

  const AXTreeID& tree_id = tree->GetAXTreeID();
  if (tree_id.type() == ax::mojom::AXTreeIDType::kUnknown) {
    NOTREACHED_IN_MIGRATION() << "Invalid tree ID.\n" << tree->ToString();
    return;
  }

  tree_ = std::move(tree);
  GetTreeManagerMapInstance()[tree_id] = this;
}

AXTreeManagerBase::AXTreeManagerBase(const AXTreeUpdate& initial_state)
    : AXTreeManagerBase(std::make_unique<AXTree>(initial_state)) {}

AXTreeManagerBase::~AXTreeManagerBase() {
  if (!tree_)
    return;

  DCHECK_NE(GetTreeID().type(), ax::mojom::AXTreeIDType::kUnknown);
  tree_->NotifyTreeManagerWillBeRemoved(GetTreeID());
  GetTreeManagerMapInstance().erase(GetTreeID());
}

AXTreeManagerBase::AXTreeManagerBase(AXTreeManagerBase&& manager) {
  if (!manager.tree_) {
    ReleaseTree();
    return;
  }

  manager.tree_->NotifyTreeManagerWillBeRemoved(manager.GetTreeID());
  GetTreeManagerMapInstance().erase(manager.GetTreeID());
  SetTree(std::move(manager.tree_));
}

AXTreeManagerBase& AXTreeManagerBase::operator=(AXTreeManagerBase&& manager) {
  if (this == &manager)
    return *this;

  if (manager.tree_) {
    manager.tree_->NotifyTreeManagerWillBeRemoved(manager.GetTreeID());
    GetTreeManagerMapInstance().erase(manager.GetTreeID());
    SetTree(std::move(manager.tree_));
  } else {
    ReleaseTree();
  }

  return *this;
}

AXTree* AXTreeManagerBase::GetTree() const {
  return tree_.get();
}

std::unique_ptr<AXTree> AXTreeManagerBase::SetTree(
    std::unique_ptr<AXTree> tree) {
  if (!tree) {
    NOTREACHED_IN_MIGRATION()
        << "Attempting to set a new tree, but no tree has been provided.";
    return {};
  }

  if (tree->GetAXTreeID().type() == ax::mojom::AXTreeIDType::kUnknown) {
    NOTREACHED_IN_MIGRATION() << "Invalid tree ID.\n" << tree->ToString();
    return {};
  }

  if (tree_) {
    tree_->NotifyTreeManagerWillBeRemoved(GetTreeID());
    GetTreeManagerMapInstance().erase(GetTreeID());
  }

  std::swap(tree_, tree);
  GetTreeManagerMapInstance()[GetTreeID()] = this;
  return tree;
}

std::unique_ptr<AXTree> AXTreeManagerBase::SetTree(
    const AXTreeUpdate& initial_state) {
  return SetTree(std::make_unique<AXTree>(initial_state));
}

std::unique_ptr<AXTree> AXTreeManagerBase::ReleaseTree() {
  if (!tree_)
    return {};

  tree_->NotifyTreeManagerWillBeRemoved(GetTreeID());
  GetTreeManagerMapInstance().erase(GetTreeID());
  return std::move(tree_);
}

AXTreeUpdate AXTreeManagerBase::SnapshotTree() const {
  NOTIMPLEMENTED();
  return {};
}

bool AXTreeManagerBase::ApplyTreeUpdate(const AXTreeUpdate& update) {
  if (tree_)
    return tree_->Unserialize(update);
  return false;
}

const AXTreeID& AXTreeManagerBase::GetTreeID() const {
  if (tree_)
    return tree_->GetAXTreeID();
  return AXTreeIDUnknown();
}

const AXTreeID& AXTreeManagerBase::GetParentTreeID() const {
  if (tree_)
    return GetTreeData().parent_tree_id;
  return AXTreeIDUnknown();
}

const AXTreeData& AXTreeManagerBase::GetTreeData() const {
  static const base::NoDestructor<AXTreeData> empty_tree_data;
  if (tree_)
    return tree_->data();
  return *empty_tree_data;
}

// static
AXNode* AXTreeManagerBase::GetNodeFromTree(const AXTreeID& tree_id,
                                           const AXNodeID& node_id) {
  const AXTreeManagerBase* manager = GetManager(tree_id);
  if (manager)
    return manager->GetNode(node_id);
  return nullptr;
}

AXNode* AXTreeManagerBase::GetNode(const AXNodeID& node_id) const {
  if (tree_)
    return tree_->GetFromId(node_id);
  return nullptr;
}

AXNode* AXTreeManagerBase::GetRoot() const {
  if (!tree_)
    return nullptr;
  // tree_->root() can be nullptr during `AXTreeObserver` callbacks.
  return tree_->root();
}

AXNode* AXTreeManagerBase::GetRootOfChildTree(
    const AXNodeID& host_node_id) const {
  const AXNode* host_node = GetNode(host_node_id);
  if (host_node)
    return GetRootOfChildTree(*host_node);
  return nullptr;
}

AXNode* AXTreeManagerBase::GetRootOfChildTree(const AXNode& host_node) const {
  const AXTreeID& child_tree_id = AXTreeID::FromString(
      host_node.GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId));
  const AXTreeManagerBase* child_manager = GetManager(child_tree_id);
  if (!child_manager || !child_manager->GetTree())
    return nullptr;
  // `AXTree::root()` can be nullptr during `AXTreeObserver` callbacks.
  return child_manager->GetTree()->root();
}

AXNode* AXTreeManagerBase::GetHostNode() const {
  const AXTreeID& parent_tree_id = GetParentTreeID();
  const AXTreeManagerBase* parent_manager = GetManager(parent_tree_id);
  if (!parent_manager || !parent_manager->GetTree())
    return nullptr;  // The parent tree is not present or is empty.

  const std::set<AXNodeID>& host_node_ids =
      parent_manager->GetTree()->GetNodeIdsForChildTreeId(GetTreeID());
  if (host_node_ids.empty()) {
    // The parent tree is present but is still not connected. A connection will
    // be established when it updates one of its nodes, turning it into a host
    // node pointing to this child tree.
    return nullptr;
  }

  DCHECK_EQ(host_node_ids.size(), 1u)
      << "Multiple nodes claim the same child tree ID.";
  const AXNodeID& host_node_id = *host_node_ids.begin();
  AXNode* parent_node = parent_manager->GetNode(host_node_id);
  DCHECK(parent_node);
  DCHECK_EQ(GetTreeID(), AXTreeID::FromString(parent_node->GetStringAttribute(
                             ax::mojom::StringAttribute::kChildTreeId)))
      << "A node that hosts a child tree should expose its tree ID in its "
         "`kChildTreeId` attribute.";
  return parent_node;
}

bool AXTreeManagerBase::AttachChildTree(const AXNodeID& host_node_id,
                                        AXTreeManagerBase& child_manager) {
  AXNode* host_node = GetNode(host_node_id);
  if (host_node)
    return AttachChildTree(*host_node, child_manager);
  return false;
}

bool AXTreeManagerBase::AttachChildTree(AXNode& host_node,
                                        AXTreeManagerBase& child_manager) {
  if (!tree_ || !child_manager.GetTree())
    return false;
  if (child_manager.GetParentTreeID().type() !=
      ax::mojom::AXTreeIDType::kUnknown) {
    return false;  // Child manager already attached to another host node.
  }

  DCHECK_EQ(GetNode(host_node.id()), &host_node)
      << "`host_node` should belong to this manager.";
  DCHECK(host_node.tree())
      << "A node should always be attached to its owning tree.";
  DCHECK_NE(GetTreeID().type(), ax::mojom::AXTreeIDType::kUnknown);
  DCHECK_NE(child_manager.GetTreeID().type(),
            ax::mojom::AXTreeIDType::kUnknown);
  if (!host_node.IsLeaf()) {
    // For now, child trees can only be attached on leaf nodes, otherwise
    // behavior would be ambiguous.
    return false;
  }

  {
    AXNodeData host_node_data = host_node.data();
    DCHECK(
        !host_node.HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId))
        << "`AXNode::IsLeaf()` should mark all nodes with child tree IDs as "
           "leaves.\n"
        << host_node;
    host_node_data.AddChildTreeId(child_manager.GetTreeID());
    AXTreeUpdate update;
    update.nodes = {host_node_data};
    CHECK(ApplyTreeUpdate(update)) << GetTree()->error();
  }

  {
    AXTreeData tree_data = child_manager.GetTreeData();
    tree_data.parent_tree_id = GetTreeID();
    AXTreeUpdate update;
    update.has_tree_data = true;
    update.tree_data = tree_data;
    CHECK(child_manager.ApplyTreeUpdate(update))
        << child_manager.GetTree()->error();
  }

  return true;
}

std::optional<AXTreeManagerBase> AXTreeManagerBase::AttachChildTree(
    const AXNodeID& host_node_id,
    const AXTreeUpdate& initial_state) {
  AXNode* host_node = GetNode(host_node_id);
  if (host_node)
    return AttachChildTree(*host_node, initial_state);
  return std::nullopt;
}

std::optional<AXTreeManagerBase> AXTreeManagerBase::AttachChildTree(
    AXNode& host_node,
    const AXTreeUpdate& initial_state) {
  AXTreeManagerBase child_manager(initial_state);
  if (AttachChildTree(host_node, child_manager))
    return child_manager;
  return std::nullopt;
}

AXTreeManagerBase* AXTreeManagerBase::DetachChildTree(
    const AXNodeID& host_node_id) {
  AXNode* host_node = GetNode(host_node_id);
  if (host_node)
    return DetachChildTree(*host_node);
  return nullptr;
}

AXTreeManagerBase* AXTreeManagerBase::DetachChildTree(AXNode& host_node) {
  if (!tree_)
    return nullptr;

  DCHECK_EQ(GetNode(host_node.id()), &host_node)
      << "`host_node` should belong to this manager.";
  DCHECK(host_node.tree())
      << "A node should always be attached to its owning tree.";
  DCHECK_NE(GetTreeID().type(), ax::mojom::AXTreeIDType::kUnknown);
  if (!host_node.IsLeaf()) {
    // For now, child trees are only allowed to be attached on leaf nodes,
    // otherwise behavior would be ambiguous.
    return nullptr;
  }

  const AXTreeID& child_tree_id = AXTreeID::FromString(
      host_node.GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId));
  AXTreeManagerBase* child_manager = GetManager(child_tree_id);
  if (!child_manager || !child_manager->GetTree())
    return nullptr;

  DCHECK_NE(child_manager->GetTreeID().type(),
            ax::mojom::AXTreeIDType::kUnknown);

  {
    AXNodeData host_node_data = host_node.data();
    DCHECK_NE(child_manager->GetTreeData().parent_tree_id.type(),
              ax::mojom::AXTreeIDType::kUnknown)
        << "Child tree should be attached to its host node.\n"
        << host_node;
    host_node_data.RemoveStringAttribute(
        ax::mojom::StringAttribute::kChildTreeId);
    AXTreeUpdate update;
    update.nodes = {host_node_data};
    CHECK(ApplyTreeUpdate(update)) << GetTree()->error();
  }

  {
    AXTreeData tree_data = child_manager->GetTreeData();
    tree_data.parent_tree_id = AXTreeIDUnknown();
    AXTreeUpdate update;
    update.has_tree_data = true;
    update.tree_data = tree_data;
    CHECK(child_manager->ApplyTreeUpdate(update))
        << child_manager->GetTree()->error();
  }

  return child_manager;
}

}  // namespace ui
