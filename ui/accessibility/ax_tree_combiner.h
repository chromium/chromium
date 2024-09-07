// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_COMBINER_H_
#define UI_ACCESSIBILITY_AX_TREE_COMBINER_H_

#include <vector>

#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

// This helper class takes multiple accessibility trees that reference each
// other via tree IDs, and combines them into a single accessibility tree
// that spans all of them.
//
// Since node IDs are relative to each ID, it has to renumber all of the IDs
// and update all of the attributes that reference IDs of other nodes to
// ensure they point to the right node.
//
// It also makes sure the final combined tree points to the correct focused
// node across all of the trees based on the focused tree ID of the root tree.
class AX_EXPORT AXTreeCombiner {
 public:
  AXTreeCombiner();
  ~AXTreeCombiner();

  void AddTree(AXTreeUpdate& tree, bool is_root);
  bool Combine();

  std::optional<AXTreeUpdate> combined() { return combined_; }

 private:
  AXNodeID MapId(AXTreeID tree_id, AXNodeID node_id);

  void ProcessTree(AXTreeUpdate* tree,
                   const std::map<AXTreeID, AXTreeUpdate*>& tree_id_map);

  std::vector<AXTreeUpdate> trees_;
  AXTreeID root_tree_id_;
  AXNodeID next_id_ = 1;
  std::map<std::pair<AXTreeID, AXNodeID>, AXNodeID> tree_id_node_id_map_;
  std::optional<AXTreeUpdate> combined_;
};


}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_COMBINER_H_
