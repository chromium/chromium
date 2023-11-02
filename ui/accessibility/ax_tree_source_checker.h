// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_SOURCE_CHECKER_H_
#define UI_ACCESSIBILITY_AX_TREE_SOURCE_CHECKER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/accessibility/ax_tree_source.h"

namespace ui {

template <typename AXSourceNode>
class AXTreeSourceChecker {
 public:
  explicit AXTreeSourceChecker(AXTreeSource<AXSourceNode>* tree);

  AXTreeSourceChecker(const AXTreeSourceChecker&) = delete;
  AXTreeSourceChecker& operator=(const AXTreeSourceChecker&) = delete;

  ~AXTreeSourceChecker();

  // Returns true if everything reachable from the root of the tree is
  // consistent in its parent/child connections, and returns the error
  // as a string.
  bool CheckAndGetErrorString(std::string* error_string);

 private:
  bool Check(AXSourceNode node, std::string indent, std::string* output);
  std::string NodeToString(AXSourceNode node);

  raw_ptr<AXTreeSource<AXSourceNode>> tree_;

  std::map<AXNodeID, AXNodeID> node_id_to_parent_id_map_;
};

template <typename AXSourceNode>
AXTreeSourceChecker<AXSourceNode>::AXTreeSourceChecker(
    AXTreeSource<AXSourceNode>* tree)
    : tree_(tree) {}

template <typename AXSourceNode>
AXTreeSourceChecker<AXSourceNode>::~AXTreeSourceChecker() = default;

template <typename AXSourceNode>
bool AXTreeSourceChecker<AXSourceNode>::CheckAndGetErrorString(
    std::string* error_string) {
  node_id_to_parent_id_map_.clear();

  AXSourceNode root = tree_->GetRoot();
  if (!tree_->IsValid(root)) {
    *error_string = "Root is not valid.";
    return false;
  }

  AXNodeID root_id = tree_->GetId(root);
  node_id_to_parent_id_map_[root_id] = kInvalidAXNodeID;

  return Check(root, "", error_string);
}

template <typename AXSourceNode>
std::string AXTreeSourceChecker<AXSourceNode>::NodeToString(AXSourceNode node) {
  AXNodeData node_data;
  tree_->SerializeNode(node, &node_data);

  std::vector<AXSourceNode> children;
  tree_->GetChildren(node, &children);
  std::string children_str;
  if (children.empty()) {
    children_str = "(no children)";
  } else {
    for (size_t i = 0; i < children.size(); i++) {
      auto& child = children[i];
      AXNodeID child_id =
          tree_->IsValid(child) ? tree_->GetId(child) : kInvalidAXNodeID;
      if (i == 0)
        children_str += "child_ids=" + base::NumberToString(child_id);
      else
        children_str += "," + base::NumberToString(child_id);
    }
  }

  AXNodeID parent_id = tree_->IsValid(tree_->GetParent(node))
                           ? tree_->GetId(tree_->GetParent(node))
                           : kInvalidAXNodeID;

  return base::StringPrintf("%s %s parent_id=%d", node_data.ToString().c_str(),
                            children_str.c_str(), parent_id);
}

template <typename AXSourceNode>
bool AXTreeSourceChecker<AXSourceNode>::Check(AXSourceNode node,
                                              std::string indent,
                                              std::string* output) {
  *output += indent + NodeToString(node);

  AXNodeID node_id = tree_->GetId(node);
  if (node_id <= kInvalidAXNodeID) {
    std::string msg = base::StringPrintf(
        "Got a node with id %d, but all node IDs should be >= 1:\n%s\n",
        node_id, NodeToString(node).c_str());
    *output = msg + *output;
    return false;
  }

  // Check parent.
  AXNodeID expected_parent_id = node_id_to_parent_id_map_[node_id];
  AXSourceNode parent = tree_->GetParent(node);
  if (expected_parent_id == kInvalidAXNodeID) {
    if (tree_->IsValid(parent)) {
      std::string msg = base::StringPrintf(
          "Node %d is the root, so its parent should be invalid, but we "
          "got a node with id %d.\n"
          "Node: %s\n"
          "Parent: %s\n",
          node_id, tree_->GetId(parent), NodeToString(node).c_str(),
          NodeToString(parent).c_str());
      *output = msg + *output;
      return false;
    }
  } else {
    if (!tree_->IsValid(parent)) {
      std::string msg = base::StringPrintf(
          "Node %d is not the root, but its parent was invalid:\n%s\n", node_id,
          NodeToString(node).c_str());
      *output = msg + *output;
      return false;
    }
    AXNodeID parent_id = tree_->GetId(parent);
    if (parent_id != expected_parent_id) {
      AXSourceNode expected_parent = tree_->GetFromId(expected_parent_id);
      std::string msg = base::StringPrintf(
          "Expected node %d to have a parent of %d, but found a parent of %d.\n"
          "Node: %s\n"
          "Parent: %s\n"
          "Expected parent: %s\n",
          node_id, expected_parent_id, parent_id, NodeToString(node).c_str(),
          NodeToString(parent).c_str(), NodeToString(expected_parent).c_str());
      *output = msg + *output;
      return false;
    }
  }

  // Check children.
  std::vector<AXSourceNode> children;
  tree_->GetChildren(node, &children);

  for (size_t i = 0; i < children.size(); i++) {
    auto& child = children[i];
    if (!tree_->IsValid(child)) {
      std::string msg = base::StringPrintf(
          "Node %d has an invalid child (index %d): %s\n", node_id,
          static_cast<int>(i), NodeToString(node).c_str());
      *output = msg + *output;
      return false;
    }

    AXNodeID child_id = tree_->GetId(child);
    if (node_id_to_parent_id_map_.find(child_id) !=
        node_id_to_parent_id_map_.end()) {
      *output += "\n" + indent + "  ";
      AXNodeData child_data;
      tree_->SerializeNode(child, &child_data);
      *output += child_data.ToString() + "\n";

      std::string msg = base::StringPrintf(
          "Node %d has a child with ID %d, but we've previously seen a node "
          "with that ID, with a parent of %d.\n"
          "Node: %s",
          node_id, child_id, node_id_to_parent_id_map_[child_id],
          NodeToString(node).c_str());
      *output = msg + *output;
      return false;
    }

    node_id_to_parent_id_map_[child_id] = node_id;
  }

  *output += "\n";

  for (auto& child : children) {
    if (!Check(child, indent + "  ", output))
      return false;
  }

  return true;
}

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_SOURCE_CHECKER_H_
