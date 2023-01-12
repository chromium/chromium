// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_TREE_NODE_ITERATOR_H_
#define UI_BASE_MODELS_TREE_NODE_ITERATOR_H_

#include "base/check.h"
#include "base/containers/stack.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"

namespace ui {

// Iterator that iterates over the descendants of a node. The iteration does
// not include the node itself, only the descendants. The following illustrates
// typical usage:
// while (iterator.has_next()) {
//   Node* node = iterator.Next();
//   // do something with node.
// }
template <class NodeType>
class TreeNodeIterator {
 public:
  typedef base::RepeatingCallback<bool(NodeType*)> PruneCallback;

  // This constructor accepts an optional filter function |prune| which could be
  // used to prune complete branches of the tree. The filter function will be
  // evaluated on each tree node and if it evaluates to true the node and all
  // its descendants will be skipped by the iterator.
  TreeNodeIterator(NodeType* node, const PruneCallback& prune)
      : prune_(prune) {
    // Move forward through the children list until the first non prunable node.
    // This is to satisfy the iterator invariant that the current index in the
    // Position at the top of the _positions list must point to a node the
    // iterator will be returning.
    const auto i =
        base::ranges::find_if(node->children(), [prune](const auto& child) {
          return prune.is_null() || !prune.Run(child.get());
        });
    if (i != node->children().cend())
      positions_.emplace(node, i - node->children().cbegin());
  }

  explicit TreeNodeIterator(NodeType* node) {
    if (!node->children().empty())
      positions_.emplace(node, 0);
  }

  TreeNodeIterator(const TreeNodeIterator&) = delete;
  TreeNodeIterator& operator=(const TreeNodeIterator&) = delete;

  // Returns true if there are more descendants.
  bool has_next() const { return !positions_.empty(); }

  // Returns the next descendant.
  NodeType* Next() {
    DCHECK(has_next());

    // There must always be a valid node in the current Position index.
    NodeType* result =
        positions_.top().node->children()[positions_.top().index].get();

    // Make sure we don't attempt to visit result again.
    ++positions_.top().index;

    // Iterate over result's children.
    positions_.emplace(result, 0);

    // Advance to next valid node by skipping over the pruned nodes and the
    // empty Positions. At the end of this loop two cases are possible:
    // - the current index of the top() Position points to a valid node
    // - the _position list is empty, the iterator has_next() will return false.
    while (!positions_.empty()) {
      auto& top = positions_.top();
      if (top.index >= top.node->children().size())
        positions_.pop(); // This Position is all processed, move to the next.
      else if (!prune_.is_null() &&
               prune_.Run(top.node->children()[top.index].get()))
        ++top.index;  // Prune the branch.
      else
        break;  // Now positioned at the next node to be returned.
    }

    return result;
  }

 private:
  template <class PositionNodeType>
  struct Position {
    Position(PositionNodeType* node, size_t index) : node(node), index(index) {}

    raw_ptr<PositionNodeType> node;
    size_t index;
  };

  base::stack<Position<NodeType>> positions_;
  PruneCallback prune_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_TREE_NODE_ITERATOR_H_
