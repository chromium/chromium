// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_combiner.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ui {

AXTreeCombiner::AXTreeCombiner() = default;

AXTreeCombiner::~AXTreeCombiner() = default;

void AXTreeCombiner::AddTree(AXTreeUpdate& tree, bool is_root) {
  if (tree.tree_data.tree_id == AXTreeIDUnknown()) {
    LOG(WARNING) << "Skipping AXTreeID because its tree ID is unknown";
    return;
  }

  if (is_root) {
    DCHECK_EQ(root_tree_id_, AXTreeIDUnknown());
    root_tree_id_ = tree.tree_data.tree_id;
  }

  trees_.emplace_back(std::move(tree));
}

bool AXTreeCombiner::Combine() {
  // `Combine` should only ever be called once.
  CHECK(!combined_.has_value());

  if (trees_.size() == 1) {
    // Nothing to combine -- only one tree.
    DCHECK(root_tree_id_ == trees_[0].tree_data.tree_id);
    combined_ = std::move(trees_[0]);
    return true;
  }

  combined_ = AXTreeUpdate();

  // First create a map from tree ID to tree update.
  std::map<AXTreeID, AXTreeUpdate*> tree_id_map;
  for (auto& tree : trees_) {
    AXTreeID tree_id = tree.tree_data.tree_id;
    if (tree_id_map.find(tree_id) != tree_id_map.end()) {
      return false;
    }
    tree_id_map[tree.tree_data.tree_id] = &tree;
  }

  // Make sure the root tree ID is in the map, otherwise fail.
  if (tree_id_map.find(root_tree_id_) == tree_id_map.end()) {
    return false;
  }

  // Process the nodes recursively, starting with the root tree.
  AXTreeUpdate* root = tree_id_map.find(root_tree_id_)->second;
  ProcessTree(root, tree_id_map);

  // Set the root id.
  combined_->root_id = combined_->nodes.size() > 0 ? combined_->nodes[0].id : 0;

  // Finally, handle the tree ID, taking into account which subtree might
  // have focus and mapping IDs from the tree data appropriately.
  combined_->has_tree_data = true;
  combined_->tree_data = root->tree_data;
  AXTreeID focused_tree_id = root->tree_data.focused_tree_id;
  const AXTreeUpdate* focused_tree = root;
  if (tree_id_map.find(focused_tree_id) != tree_id_map.end()) {
    focused_tree = tree_id_map[focused_tree_id];
  }
  combined_->tree_data.focus_id =
      MapId(focused_tree_id, focused_tree->tree_data.focus_id);
  combined_->tree_data.sel_is_backward =
      MapId(focused_tree_id, focused_tree->tree_data.sel_is_backward);
  combined_->tree_data.sel_anchor_object_id =
      MapId(focused_tree_id, focused_tree->tree_data.sel_anchor_object_id);
  combined_->tree_data.sel_focus_object_id =
      MapId(focused_tree_id, focused_tree->tree_data.sel_focus_object_id);
  combined_->tree_data.sel_anchor_offset =
      focused_tree->tree_data.sel_anchor_offset;
  combined_->tree_data.sel_focus_offset =
      focused_tree->tree_data.sel_focus_offset;

  // Debug-mode check that the resulting combined tree is valid.
  AXTree tree;
  DCHECK(tree.Unserialize(*combined_)) << combined_->ToString() << "\n"
                                       << tree.error();

  return true;
}

AXNodeID AXTreeCombiner::MapId(AXTreeID tree_id, AXNodeID node_id) {
  auto tree_id_node_id = std::make_pair(tree_id, node_id);
  if (tree_id_node_id_map_[tree_id_node_id] == 0)
    tree_id_node_id_map_[tree_id_node_id] = next_id_++;
  return tree_id_node_id_map_[tree_id_node_id];
}

void AXTreeCombiner::ProcessTree(
    AXTreeUpdate* tree,
    const std::map<AXTreeID, AXTreeUpdate*>& tree_id_map) {
  AXTreeID tree_id = tree->tree_data.tree_id;
  for (auto& node : tree->nodes) {
    AXTreeID child_tree_id = AXTreeID::FromString(
        node.GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId));

    // Map the node's ID.
    node.id = MapId(tree_id, node.id);

    // Map the node's child IDs.
    for (int& child_id : node.child_ids) {
      child_id = MapId(tree_id, child_id);
    }

    // Map the container id.
    if (node.relative_bounds.offset_container_id > 0) {
      node.relative_bounds.offset_container_id =
          MapId(tree_id, node.relative_bounds.offset_container_id);
    }

    // Map other int attributes that refer to node IDs.
    for (auto& attr : node.int_attributes) {
      if (IsNodeIdIntAttribute(attr.first)) {
        attr.second = MapId(tree_id, attr.second);
      }
    }

    // Map other int list attributes that refer to node IDs.
    for (auto& node_int_list : node.intlist_attributes) {
      if (IsNodeIdIntListAttribute(node_int_list.first)) {
        for (int& attr : node_int_list.second) {
          attr = MapId(tree_id, attr);
        }
      }
    }

    // Remove the ax::mojom::StringAttribute::kChildTreeId attribute.
    for (auto& attr : node.string_attributes) {
      if (attr.first == ax::mojom::StringAttribute::kChildTreeId) {
        attr.first = ax::mojom::StringAttribute::kNone;
        attr.second = "";
      }
    }

    // See if this node has a child tree. As a confidence check, make sure the
    // child tree lists this tree as its parent tree id.
    AXTreeUpdate* child_tree = nullptr;
    if (tree_id_map.find(child_tree_id) != tree_id_map.end()) {
      child_tree = tree_id_map.find(child_tree_id)->second;
      if (child_tree->tree_data.parent_tree_id != tree_id) {
        child_tree = nullptr;
      }
      if (child_tree && child_tree->nodes.empty()) {
        child_tree = nullptr;
      }
      if (child_tree) {
        node.child_ids.push_back(MapId(child_tree_id, child_tree->nodes[0].id));
      }
    }

    // Put the rewritten AXNodeData into the output data structure.
    combined_->nodes.emplace_back(std::move(node));

    // Recurse into the child tree now, if any.
    if (child_tree) {
      ProcessTree(child_tree, tree_id_map);
    }
  }
}

}  // namespace ui
