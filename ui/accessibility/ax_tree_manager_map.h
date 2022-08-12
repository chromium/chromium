// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_MANAGER_MAP_H_
#define UI_ACCESSIBILITY_AX_TREE_MANAGER_MAP_H_

#include "base/containers/flat_map.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"

namespace ui {

// This class manages AXTreeManager instances. It is a wrapper around a
// base::flat_map. AXTreeID's are used as the key for the map. Since AXTreeID's
// might refer to AXTreeIDUnknown, callers should not expect AXTreeIDUnknown to
// map to a particular AXTreeManager.
class AX_EXPORT AXTreeManagerMap {
 public:
  AXTreeManagerMap();
  ~AXTreeManagerMap();
  AXTreeManagerMap(const AXTreeManagerMap& map) = delete;
  AXTreeManagerMap& operator=(const AXTreeManagerMap& map) = delete;

  void AddTreeManager(const AXTreeID& tree_id, AXTreeManager* manager);
  void RemoveTreeManager(const AXTreeID& tree_id);
  AXTreeManager* GetManager(const AXTreeID& tree_id);

 private:
  base::flat_map<AXTreeID, AXTreeManager*> map_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_MANAGER_MAP_H_
