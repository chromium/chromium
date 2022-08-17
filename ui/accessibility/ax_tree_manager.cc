// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_manager.h"

#include "base/no_destructor.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/ax_tree_observer.h"

namespace ui {

// static
AXTreeManagerMap& AXTreeManager::GetMap() {
  static base::NoDestructor<AXTreeManagerMap> map;
  return *map;
}

// static
AXTreeManager* AXTreeManager::FromID(AXTreeID ax_tree_id) {
  return ax_tree_id != AXTreeIDUnknown() ? GetMap().GetManager(ax_tree_id)
                                         : nullptr;
}

// static
AXTreeManager* AXTreeManager::ForChildTree(const AXNode& parent_node) {
  if (!parent_node.HasStringAttribute(
          ax::mojom::StringAttribute::kChildTreeId)) {
    return nullptr;
  }

  AXTreeID child_tree_id = AXTreeID::FromString(
      parent_node.GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId));
  AXTreeManager* child_tree_manager = GetMap().GetManager(child_tree_id);

  // Some platforms do not use AXTreeManagers, so child trees don't exist in
  // the browser process.
  DCHECK(!child_tree_manager ||
         !child_tree_manager->GetParentNodeFromParentTreeAsAXNode() ||
         child_tree_manager->GetParentNodeFromParentTreeAsAXNode()->id() ==
             parent_node.id());
  return child_tree_manager;
}

AXTreeManager::AXTreeManager()
    : ax_tree_id_(AXTreeIDUnknown()),
      ax_tree_(nullptr),
      event_generator_(ax_tree()) {}

AXTreeManager::AXTreeManager(std::unique_ptr<AXTree> tree)
    : ax_tree_id_(tree ? tree->data().tree_id : AXTreeIDUnknown()),
      ax_tree_(std::move(tree)),
      event_generator_(ax_tree()) {
  GetMap().AddTreeManager(ax_tree_id_, this);
}

AXTreeManager::AXTreeManager(const AXTreeID& tree_id,
                             std::unique_ptr<AXTree> tree)
    : ax_tree_id_(tree_id),
      ax_tree_(std::move(tree)),
      event_generator_(ax_tree()) {
  GetMap().AddTreeManager(ax_tree_id_, this);
  if (ax_tree())
    tree_observation_.Observe(ax_tree());
}

AXTreeID AXTreeManager::GetTreeID() const {
  return ax_tree_ ? ax_tree_->data().tree_id : AXTreeIDUnknown();
}

AXTreeID AXTreeManager::GetParentTreeID() const {
  return ax_tree_ ? ax_tree_->data().parent_tree_id : AXTreeIDUnknown();
}

AXNode* AXTreeManager::GetRootAsAXNode() const {
  return ax_tree_ ? ax_tree_->root() : nullptr;
}

void AXTreeManager::WillBeRemovedFromMap() {
  if (!ax_tree_)
    return;
  ax_tree_->NotifyTreeManagerWillBeRemoved(ax_tree_id_);
}

AXTreeManager::~AXTreeManager() {
  // Stop observing so we don't get a callback for every node being deleted.
  event_generator_.ReleaseTree();
  if (ax_tree_)
    GetMap().RemoveTreeManager(ax_tree_id_);
}

void AXTreeManager::OnTreeDataChanged(AXTree* tree,
                                      const AXTreeData& old_data,
                                      const AXTreeData& new_data) {
  GetMap().RemoveTreeManager(ax_tree_id_);
  ax_tree_id_ = new_data.tree_id;
  GetMap().AddTreeManager(ax_tree_id_, this);
}

void AXTreeManager::RemoveFromMap() {
  GetMap().RemoveTreeManager(ax_tree_id_);
}

}  // namespace ui
