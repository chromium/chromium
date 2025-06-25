// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_manager_map.h"

#include "base/containers/contains.h"

namespace ui {

AXTreeManagerMap::AXTreeManagerMap() = default;

AXTreeManagerMap::~AXTreeManagerMap() = default;

void AXTreeManagerMap::AddTreeManager(const AXTreeID& tree_id,
                                      AXTreeManager* manager) {
  if (tree_id != AXTreeIDUnknown())
    map_[tree_id] = manager;
}

void AXTreeManagerMap::RemoveTreeManager(const AXTreeID& tree_id) {
  if (auto iter = map_.find(tree_id); iter != map_.end()) {
    iter->second->WillBeRemovedFromMap();
    map_.erase(iter);
  }
}

AXTreeManager* AXTreeManagerMap::GetManager(const AXTreeID& tree_id) {
  if (tree_id == AXTreeIDUnknown())
    return nullptr;
  auto iter = map_.find(tree_id);
  if (iter == map_.end())
    return nullptr;

  return iter->second;
}

}  // namespace ui
