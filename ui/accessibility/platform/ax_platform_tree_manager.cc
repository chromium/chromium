// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_tree_manager.h"

#include <utility>

#include "base/check_op.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_util.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/ax_platform_node.h"

namespace ui {

AXPlatformTreeManager::AXPlatformTreeManager(const AXTreeID& tree_id,
                                             std::unique_ptr<AXTree> tree)
    : AXTreeManager(tree_id, std::move(tree)) {}

AXPlatformTreeManager::~AXPlatformTreeManager() = default;

AXPlatformNode* AXPlatformTreeManager::GetPlatformNode(
    const AXNodeID& node_id) const {
  AXPlatformNodeDelegate* delegate = GetPlatformNodeDelegate(node_id);
  if (!delegate)
    return nullptr;
  return delegate->GetAXPlatformNode();
}

ui::AXPlatformNode* AXPlatformTreeManager::GetPlatformNode(
    const ui::AXNode& node) const {
  return GetPlatformNode(node.id());
}

AXPlatformNodeDelegate* AXPlatformTreeManager::GetPlatformNodeDelegate(
    const AXNodeID& node_id) const {
  // TODO(nektar): The initial tree has a rootnode with an invalid ID. Fix this
  // bug and re-enable this DCHECK.
  // DCHECK_NE(node_id, kInvalidAXNodeID);
  if (!ax_tree())
    return nullptr;

  const auto iter = id_wrapper_map_.find(node_id);
  if (iter == id_wrapper_map_.end()) {
    DCHECK(!ax_tree()->GetFromId(node_id))
        << "Unable to retrieve the delegate for an `AXNode` even though the "
           "node is still in its owning tree.\n"
        << *ax_tree()->GetFromId(node_id);
    return nullptr;
  }

  DCHECK(iter->second);
  return iter->second.get();
}

AXPlatformNodeDelegate* AXPlatformTreeManager::GetPlatformNodeDelegate(
    const AXNode& node) const {
  if (!ax_tree())
    return nullptr;

  DCHECK_EQ(node.tree(), ax_tree())
      << "Should not try to retrieve a delegate for a node that is in a "
         "different "
         "accessibility tree from the one that is being managed.\n"
      << node;
  return GetPlatformNodeDelegate(node.id());
}

AXPlatformNodeDelegate* AXPlatformTreeManager::GetPlatformNodeDelegateForRoot()
    const {
  const AXNode* root = GetRoot();
  if (root)
    return GetPlatformNodeDelegate(*root);
  return nullptr;
}

void AXPlatformTreeManager::SetPlatformNodeDelegate(
    const AXNode& node,
    std::unique_ptr<AXPlatformNodeDelegate> delegate) {
  DCHECK(node.IsDataValid());
  DCHECK(delegate);
  if (!ax_tree()) {
    DCHECK(id_wrapper_map_.empty());
    return;
  }

  DCHECK(node.tree()) << "Should not try to set a delegate for a node that is "
                         "not yet owned by an accessibility tree, or that has "
                         "been deleted from its owning tree.\n"
                      << node << '\n'
                      << *delegate;
  DCHECK_EQ(node.tree(), ax_tree())
      << "Should not try to set a delegate for a node that is in a different "
         "accessibility tree from the one that is being managed.\n"
      << node << '\n'
      << *delegate;
  id_wrapper_map_[node.id()] = std::move(delegate);
  // TODO(nektar): When a tree's root is replaced, OnNodeDeleted is not called.
  // Fix and re-enable this DCHECK.
  // DCHECK_GE(static_cast<size_t>(node.tree()->size()), id_wrapper_map_.size())
  // << "The number of delegate objects should not be more than the number of "
  // "`AXNode`s they are associated with in the managed accessibility "
  // "tree because the node's lifetime should determine the delegate's "
  // "lifetime.\n"
  // << node << '\n'
  // << *delegate;
}

std::unique_ptr<AXPlatformNodeDelegate>
AXPlatformTreeManager::UnsetPlatformNodeDelegate(const AXNodeID& node_id) {
  DCHECK_NE(node_id, kInvalidAXNodeID);
  if (!ax_tree()) {
    DCHECK(id_wrapper_map_.empty());
    return nullptr;
  }

  auto delegate_handle = id_wrapper_map_.extract(node_id);
  if (delegate_handle)
    return std::move(delegate_handle.mapped());
  return nullptr;
}

std::unique_ptr<AXPlatformNodeDelegate>
AXPlatformTreeManager::UnsetPlatformNodeDelegate(const AXNode& node) {
  DCHECK(node.IsDataValid());
  DCHECK_EQ(node.tree(), ax_tree())
      << "Should not try to unset a delegate for a node that is in a different "
         "accessibility tree from the one that is being managed.\n"
      << node;
  return UnsetPlatformNodeDelegate(node.id());
}

}  // namespace ui
