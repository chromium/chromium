// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_H_

#include <map>
#include <string>

#include "base/component_export.h"
#include "base/strings/string_number_conversions.h"

namespace ui {

/**
 * Indexes an accessible tree: associates each accessible node of the tree
 * with its DOM id (if any) and to a line index in the formatted accessible
 * tree the node is placed at.
 *
 * GetDOMId returns DOM id by an accessible node;
 * ChildrenContainer returns accessible children for an accessible node;
 * Compare is the Compare named requirements, used to compare two nodes.
 */
template <typename AccessibilityObject,
          std::string (*GetDOMId)(const AccessibilityObject),
          typename ChildrenContainer,
          ChildrenContainer (*GetChildren)(const AccessibilityObject),
          typename Compare = std::less<AccessibilityObject>>
class COMPONENT_EXPORT(AX_PLATFORM) AXTreeIndexer {
 public:
  explicit AXTreeIndexer(const AccessibilityObject node) {
    int counter = 0;
    Build(node, &counter);
  }
  virtual ~AXTreeIndexer() {}

  // Returns a line index in the formatted tree the node is placed at.
  virtual std::string IndexBy(const AccessibilityObject node) const {
    std::string line_index = ":unknown";
    auto iter = node_to_identifier_.find(node);
    if (iter != node_to_identifier_.end()) {
      line_index = iter->second.line_index;
    }
    return line_index;
  }

  // Finds a first match either by a line number in :LINE_NUM format or by DOM
  // id.
  AccessibilityObject NodeBy(const std::string& identifier) const {
    for (auto& item : node_to_identifier_) {
      if (item.second.line_index == identifier ||
          item.second.id == identifier) {
        return item.first;
      }
    }
    return nullptr;
  }

 private:
  void Build(const AccessibilityObject node, int* counter) {
    const std::string id = *counter == 0 ? "document" : GetDOMId(node);
    const std::string line_index =
        std::string(1, ':') + base::NumberToString(++(*counter));

    node_to_identifier_.insert({node, {line_index, id}});

    auto children = GetChildren(node);
    for (auto child : children) {
      Build(child, counter);
    }
  }

  struct NodeIdentifier {
    // A line index of a node in the formatted tree.
    std::string line_index;

    // The ID that a node can be identified by, for example DOM id, or
    // the "document" keyword pointing to a root node.
    std::string id;
  };

  // Map between accessible objects and their identificators which can be a line
  // index the object is placed at in an accessible tree or its DOM id
  // attribute.
  std::map<const AccessibilityObject, NodeIdentifier, Compare>
      node_to_identifier_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_H_
