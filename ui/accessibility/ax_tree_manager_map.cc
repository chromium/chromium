// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_manager_map.h"

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ui {

AXTreeManagerMap::AXTreeManagerMap() = default;

AXTreeManagerMap::~AXTreeManagerMap() = default;

// static
AXTreeManagerMap& AXTreeManagerMap::GetInstance() {
  static base::NoDestructor<AXTreeManagerMap> instance;
  return *instance;
}

void AXTreeManagerMap::AddTreeManager(AXTreeID tree_id,
                                      AXTreeManager* manager) {
  if (tree_id != AXTreeIDUnknown())
    map_[tree_id] = manager;
}

void AXTreeManagerMap::RemoveTreeManager(AXTreeID tree_id) {
  if (auto* manager = GetManager(tree_id)) {
    manager->WillBeRemovedFromMap();
    map_.erase(tree_id);
  }
}

AXTreeManager* AXTreeManagerMap::GetManager(AXTreeID tree_id) {
  if (tree_id == AXTreeIDUnknown())
    return nullptr;
  auto iter = map_.find(tree_id);
  if (iter == map_.end())
    return nullptr;

  return iter->second;
}

AXTreeManager* AXTreeManagerMap::GetManagerForChildTree(
    const AXNode& parent_node) {
  if (!parent_node.HasStringAttribute(
          ax::mojom::StringAttribute::kChildTreeId)) {
    return nullptr;
  }

  AXTreeID child_tree_id = AXTreeID::FromString(
      parent_node.GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId));
  AXTreeManager* child_tree_manager =
      AXTreeManagerMap::GetInstance().GetManager(child_tree_id);

  // Some platforms do not use AXTreeManagers, so child trees don't exist in
  // the browser process.
  DCHECK(!child_tree_manager ||
         !child_tree_manager->GetParentNodeFromParentTreeAsAXNode() ||
         child_tree_manager->GetParentNodeFromParentTreeAsAXNode()->id() ==
             parent_node.id());
  return child_tree_manager;
}

}  // namespace ui
