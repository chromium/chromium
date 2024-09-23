// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_SOURCE_H_
#define UI_ACCESSIBILITY_AX_TREE_SOURCE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/notreached.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_source_observer.h"

namespace ui {

// An AXTreeSource is an abstract interface for a serializable
// accessibility tree. The tree may be in some other format or
// may be computed dynamically, but maintains the properties that
// it's a strict tree, it has a unique id for each node, and all
// of the accessibility information about a node can be serialized
// as an AXNodeData. This is the primary interface to use when
// an accessibility tree will be sent over an IPC before being
// consumed.
template <typename AXNodeSource,
          typename AXTreeDataType,
          typename AXNodeDataType>
class AXTreeSource {
 public:
  virtual ~AXTreeSource() = default;

  // Get the tree data and returns true if there is any data to copy.
  virtual bool GetTreeData(AXTreeDataType data) const = 0;

  // Get the root of the tree.
  virtual AXNodeSource GetRoot() const = 0;

  // Get a node by its id. If no node by that id exists in the tree, return a
  // null node.
  virtual AXNodeSource GetFromId(AXNodeID id) const = 0;

  AXNodeSource EnsureGetFromId(AXNodeID id) const {
    AXNodeSource node = GetFromId(id);
    DCHECK(node);
    return node;
  }

  // Return the id of a node. All ids must be positive integers; 0 is not a
  // valid ID. IDs are unique only across the current tree source, not across
  // tree sources.
  virtual AXNodeID GetId(AXNodeSource node) const = 0;

  virtual void CacheChildrenIfNeeded(AXNodeSource) = 0;
  virtual size_t GetChildCount(AXNodeSource) const = 0;
  virtual AXNodeSource ChildAt(AXNodeSource, size_t) const = 0;
  virtual void ClearChildCache(AXNodeSource) = 0;

  // Get the parent of |node|.
  virtual AXNodeSource GetParent(AXNodeSource node) const = 0;

  // Returns true if |node| is an ignored node
  virtual bool IsIgnored(AXNodeSource node) const = 0;

  // Returns true if two nodes are equal.
  virtual bool IsEqual(AXNodeSource node1,
                       AXNodeSource node2) const = 0;

  // Return a AXNodeSource representing null.
  virtual AXNodeSource GetNull() const = 0;

  // Serialize one node in the tree.
  virtual void SerializeNode(AXNodeSource node,
                             AXNodeDataType* out_data) const = 0;

  // Return a string useful for debugging a node.
  virtual std::string GetDebugString(AXNodeSource node) const {
    AXNodeDataType node_data;
    SerializeNode(node, &node_data);
    return node_data.ToString();
  }

  // The following methods should be overridden in order to add or remove an
  // `AXTreeSourceObserver`, which is notified when nodes are added, removed or
  // updated in this tree source.

  virtual void AddObserver(
      AXTreeSourceObserver<AXNodeSource, AXTreeDataType, AXNodeDataType>*
          observer) {
    NOTIMPLEMENTED();
  }

  virtual void RemoveObserver(
      AXTreeSourceObserver<AXNodeSource, AXTreeDataType, AXNodeDataType>*
          observer) {
    NOTIMPLEMENTED();
  }

 protected:
  AXTreeSource() {}
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_SOURCE_H_
