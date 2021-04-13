// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_H_

#include <string>

#include "base/strings/string_number_conversions.h"
#include "ui/accessibility/ax_export.h"

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
template <std::string (*GetDOMId)(const gfx::NativeViewAccessible),
          typename ChildrenContainer,
          ChildrenContainer (*GetChildren)(const gfx::NativeViewAccessible),
          typename Compare = std::less<gfx::NativeViewAccessible>>
class AX_EXPORT AXTreeIndexer final {
 public:
  explicit AXTreeIndexer(const gfx::NativeViewAccessible node) {
    int counter = 0;
    Build(node, &counter);
  }
  virtual ~AXTreeIndexer() {}

  // Returns a line index in the formatted tree the node is placed at.
  std::string IndexBy(const gfx::NativeViewAccessible node) const {
    std::string line_index = ":unknown";
    auto iter = node_to_identifier_.find(node);
    if (iter != node_to_identifier_.end()) {
      line_index = iter->second.line_index;
    }
    return line_index;
  }

  // Finds a first match either by a line number in :LINE_NUM format or by DOM
  // id.
  gfx::NativeViewAccessible NodeBy(const std::string& identifier) const {
    for (auto& item : node_to_identifier_) {
      if (item.second.line_index == identifier ||
          item.second.DOMid == identifier) {
        return item.first;
      }
    }
    return nullptr;
  }

 private:
  void Build(const gfx::NativeViewAccessible node, int* counter) {
    const std::string line_index =
        std::string(1, ':') + base::NumberToString(++(*counter));
    const std::string domid = GetDOMId(node);

    node_to_identifier_.insert({node, {line_index, domid}});

    auto children = GetChildren(node);
    for (gfx::NativeViewAccessible child in children) {
      Build(child, counter);
    }
  }

  struct NodeIdentifier {
    std::string line_index;
    std::string DOMid;
  };

  // Map between accessible objects and their identificators which can be a line
  // index the object is placed at in an accessible tree or its DOM id
  // attribute.
  std::map<const gfx::NativeViewAccessible, NodeIdentifier, Compare>
      node_to_identifier_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_H_
