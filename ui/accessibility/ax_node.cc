// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node.h"

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_computed_node_data.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_hypertext.h"
#include "ui/accessibility/ax_language_detection.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_table_info.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/transform.h"

namespace ui {

// Definition of static class members.
constexpr char AXNode::kEmbeddedObjectCharacterUTF8[];
constexpr char16_t AXNode::kEmbeddedObjectCharacterUTF16[];
constexpr int AXNode::kEmbeddedObjectCharacterLengthUTF8;
constexpr int AXNode::kEmbeddedObjectCharacterLengthUTF16;

AXNode::AXNode(AXTree* tree,
               AXNode* parent,
               AXNodeID id,
               size_t index_in_parent,
               size_t unignored_index_in_parent)
    : tree_(tree),
      index_in_parent_(index_in_parent),
      unignored_index_in_parent_(unignored_index_in_parent),
      parent_(parent) {
  // TODO(accessibility): Change to CHECK(tree_) after https://crbug.com/1511053
  // is fixed.
  DCHECK(tree_);
  data_.id = id;
}

AXNode::~AXNode() = default;

AXNodeData&& AXNode::TakeData() {
  return std::move(data_);
}

const std::vector<raw_ptr<AXNode, VectorExperimental>>& AXNode::GetAllChildren()
    const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return children_;
}

size_t AXNode::GetChildCount() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return children_.size();
}

#if DCHECK_IS_ON()
size_t AXNode::GetSubtreeCount() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  size_t count = 1;  // |this| counts as one.
  for (AXNode* child : children_) {
    count += child->GetSubtreeCount();
  }
  return count;
}
#endif  // DCHECK_IS_ON()

size_t AXNode::GetChildCountCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());

  const AXTreeManager* child_tree_manager = AXTreeManager::ForChildTree(*this);
  if (child_tree_manager)
    return 1u;

  return GetChildCount();
}

size_t AXNode::GetUnignoredChildCount() const {
  DCHECK(!IsIgnored()) << "Called unignored method on ignored node: " << *this;
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return unignored_child_count_;
}

size_t AXNode::GetUnignoredChildCountCrossingTreeBoundary() const {
  // TODO(nektar): Should DCHECK that this node is not ignored.
  DCHECK(!tree_->GetTreeUpdateInProgressState());

  const AXTreeManager* child_tree_manager = AXTreeManager::ForChildTree(*this);
  if (child_tree_manager) {
    DCHECK_EQ(unignored_child_count_, 0u)
        << "A node cannot be hosting both a child tree and other nodes as "
           "children.";
    return 1u;  // A child tree is never ignored.
  }

  return unignored_child_count_;
}

AXNode* AXNode::GetChildAtIndex(size_t index) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (index >= GetChildCount())
    return nullptr;
  return children_[index];
}

AXNode* AXNode::GetChildAtIndexCrossingTreeBoundary(size_t index) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());

  const AXTreeManager* child_tree_manager = AXTreeManager::ForChildTree(*this);
  if (child_tree_manager) {
    DCHECK_EQ(index, 0u)
        << "A node cannot be hosting both a child tree and other nodes as "
           "children.";
    return child_tree_manager->GetRoot();
  }

  return GetChildAtIndex(index);
}

AXNode* AXNode::GetUnignoredChildAtIndex(size_t index) const {
  // TODO(nektar): Should DCHECK that this node is not ignored.
  DCHECK(!tree_->GetTreeUpdateInProgressState());

  for (auto it = UnignoredChildrenBegin(); it != UnignoredChildrenEnd(); ++it) {
    if (index == 0)
      return it.get();
    --index;
  }

  return nullptr;
}

AXNode* AXNode::GetUnignoredChildAtIndexCrossingTreeBoundary(
    size_t index) const {
  // TODO(nektar): Should DCHECK that this node is not ignored.
  DCHECK(!tree_->GetTreeUpdateInProgressState());

  const AXTreeManager* child_tree_manager = AXTreeManager::ForChildTree(*this);
  if (child_tree_manager) {
    DCHECK_EQ(index, 0u)
        << "A node cannot be hosting both a child tree and other nodes as "
           "children.";
    // A child tree is never ignored.
    return child_tree_manager->GetRoot();
  }

  return GetUnignoredChildAtIndex(index);
}

AXNode* AXNode::GetParent() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return parent_;
}

AXNode* AXNode::GetParentCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (parent_)
    return parent_;
  const AXTreeManager* manager = GetManager();
  if (manager)
    return manager->GetParentNodeFromParentTree();
  return nullptr;
}

AXNode* AXNode::GetUnignoredParent() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  AXNode* unignored_parent = GetParent();
  while (unignored_parent && unignored_parent->IsIgnored())
    unignored_parent = unignored_parent->GetParent();
  return unignored_parent;
}

AXNode* AXNode::GetUnignoredParentCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  AXNode* unignored_parent = GetUnignoredParent();
  if (!unignored_parent) {
    const AXTreeManager* manager = GetManager();
    if (manager)
      unignored_parent = manager->GetParentNodeFromParentTree();
  }
  return unignored_parent;
}

base::queue<AXNode*> AXNode::GetAncestorsCrossingTreeBoundaryAsQueue() const {
  base::queue<AXNode*> ancestors;
  AXNode* ancestor = const_cast<AXNode*>(this);
  while (ancestor) {
    ancestors.push(ancestor);
    ancestor = ancestor->GetParentCrossingTreeBoundary();
  }
  return ancestors;
}

base::stack<AXNode*> AXNode::GetAncestorsCrossingTreeBoundaryAsStack() const {
  base::stack<AXNode*> ancestors;
  AXNode* ancestor = const_cast<AXNode*>(this);
  while (ancestor) {
    ancestors.push(ancestor);
    ancestor = ancestor->GetParentCrossingTreeBoundary();
  }
  return ancestors;
}

size_t AXNode::GetIndexInParent() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return index_in_parent_;
}

size_t AXNode::GetUnignoredIndexInParent() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return unignored_index_in_parent_;
}

AXNode* AXNode::GetFirstChild() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return GetChildAtIndex(0);
}

AXNode* AXNode::GetFirstChildCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return GetChildAtIndexCrossingTreeBoundary(0);
}

AXNode* AXNode::GetFirstUnignoredChild() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return ComputeFirstUnignoredChildRecursive();
}

AXNode* AXNode::GetFirstUnignoredChildCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());

  const AXTreeManager* child_tree_manager = AXTreeManager::ForChildTree(*this);
  if (child_tree_manager)
    return child_tree_manager->GetRoot();

  return ComputeFirstUnignoredChildRecursive();
}

AXNode* AXNode::GetLastChild() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  size_t n = GetChildCount();
  if (n == 0)
    return nullptr;
  return GetChildAtIndex(n - 1);
}

AXNode* AXNode::GetLastChildCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  size_t n = GetChildCountCrossingTreeBoundary();
  if (n == 0)
    return nullptr;
  return GetChildAtIndexCrossingTreeBoundary(n - 1);
}

AXNode* AXNode::GetLastUnignoredChild() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return ComputeLastUnignoredChildRecursive();
}

AXNode* AXNode::GetLastUnignoredChildCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());

  const AXTreeManager* child_tree_manager = AXTreeManager::ForChildTree(*this);
  if (child_tree_manager)
    return child_tree_manager->GetRoot();

  return ComputeLastUnignoredChildRecursive();
}

AXNode* AXNode::GetDeepestFirstDescendant() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (!GetChildCount())
    return nullptr;

  AXNode* deepest_descendant = GetFirstChild();
  DCHECK(deepest_descendant);
  while (deepest_descendant->GetChildCount()) {
    deepest_descendant = deepest_descendant->GetFirstChild();
    DCHECK(deepest_descendant);
  }

  return deepest_descendant;
}

AXNode* AXNode::GetDeepestFirstDescendantCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (!GetChildCountCrossingTreeBoundary())
    return nullptr;

  AXNode* deepest_descendant = GetFirstChildCrossingTreeBoundary();
  DCHECK(deepest_descendant);
  while (deepest_descendant->GetChildCountCrossingTreeBoundary()) {
    deepest_descendant =
        deepest_descendant->GetFirstChildCrossingTreeBoundary();
    DCHECK(deepest_descendant);
  }

  return deepest_descendant;
}

AXNode* AXNode::GetDeepestFirstUnignoredDescendant() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  DCHECK(!IsIgnored()) << "Called unignored method on ignored node: " << *this;
  if (!GetUnignoredChildCount())
    return nullptr;

  AXNode* deepest_descendant = GetFirstUnignoredChild();
  DCHECK(deepest_descendant);
  while (deepest_descendant->GetUnignoredChildCount()) {
    deepest_descendant = deepest_descendant->GetFirstUnignoredChild();
    DCHECK(deepest_descendant);
  }

  return deepest_descendant;
}

AXNode* AXNode::GetDeepestFirstUnignoredDescendantCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  DCHECK(!IsIgnored()) << "Called unignored method on ignored node: " << *this;
  if (!GetUnignoredChildCountCrossingTreeBoundary())
    return nullptr;

  AXNode* deepest_descendant = GetFirstUnignoredChildCrossingTreeBoundary();
  DCHECK(deepest_descendant);
  while (deepest_descendant->GetUnignoredChildCountCrossingTreeBoundary()) {
    deepest_descendant =
        deepest_descendant->GetFirstUnignoredChildCrossingTreeBoundary();
    DCHECK(deepest_descendant);
  }

  return deepest_descendant;
}

AXNode* AXNode::GetDeepestLastDescendant() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (!GetChildCount())
    return nullptr;

  AXNode* deepest_descendant = GetLastChild();
  DCHECK(deepest_descendant);
  while (deepest_descendant->GetChildCount()) {
    deepest_descendant = deepest_descendant->GetLastChild();
    DCHECK(deepest_descendant);
  }

  return deepest_descendant;
}

AXNode* AXNode::GetDeepestLastDescendantCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  DCHECK(!IsIgnored()) << "Called unignored method on ignored node: " << *this;
  if (!GetChildCountCrossingTreeBoundary())
    return nullptr;

  AXNode* deepest_descendant = GetLastChildCrossingTreeBoundary();
  DCHECK(deepest_descendant);
  while (deepest_descendant->GetChildCountCrossingTreeBoundary()) {
    deepest_descendant = deepest_descendant->GetLastChildCrossingTreeBoundary();
    DCHECK(deepest_descendant);
  }

  return deepest_descendant;
}

AXNode* AXNode::GetDeepestLastUnignoredDescendant() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  DCHECK(!IsIgnored()) << "Called unignored method on ignored node: " << *this;
  if (!GetUnignoredChildCount())
    return nullptr;

  AXNode* deepest_descendant = GetLastUnignoredChild();
  DCHECK(deepest_descendant);
  while (deepest_descendant->GetUnignoredChildCount()) {
    deepest_descendant = deepest_descendant->GetLastUnignoredChild();
    DCHECK(deepest_descendant);
  }

  return deepest_descendant;
}

AXNode* AXNode::GetDeepestLastUnignoredDescendantCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  DCHECK(!IsIgnored()) << "Called unignored method on ignored node: " << *this;
  if (!GetUnignoredChildCountCrossingTreeBoundary())
    return nullptr;

  AXNode* deepest_descendant = GetLastUnignoredChildCrossingTreeBoundary();
  DCHECK(deepest_descendant);
  while (deepest_descendant->GetUnignoredChildCountCrossingTreeBoundary()) {
    deepest_descendant =
        deepest_descendant->GetLastUnignoredChildCrossingTreeBoundary();
    DCHECK(deepest_descendant);
  }

  return deepest_descendant;
}

AXNode* AXNode::GetNextSibling() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  AXNode* parent = GetParent();
  if (!parent)
    return nullptr;
  DCHECK(parent || !GetIndexInParent())
      << "Root nodes lack a parent. Their index_in_parent should be 0.";
  size_t nextIndex = GetIndexInParent() + 1;
  if (nextIndex >= parent->GetChildCount())
    return nullptr;
  return parent->GetChildAtIndex(nextIndex);
}

// Search for the next sibling of this node, skipping over any ignored nodes
// encountered.
//
// In our search:
//   If we find an ignored sibling, we consider its children as our siblings.
//   If we run out of siblings, we consider an ignored parent's siblings as our
//     own siblings.
//
// Note: this behaviour of 'skipping over' an ignored node makes this subtly
// different to finding the next (direct) sibling which is unignored.
//
// Consider a tree, where (i) marks a node as ignored:
//
//   1
//   ├── 2
//   ├── 3(i)
//   │   └── 5
//   └── 4
//
// The next sibling of node 2 is node 3, which is ignored.
// The next unignored sibling of node 2 could be either:
//  1) node 4 - next unignored sibling in the literal tree, or
//  2) node 5 - next unignored sibling in the logical document.
//
// There is no next sibling of node 5.
// The next unignored sibling of node 5 could be either:
//  1) null   - no next sibling in the literal tree, or
//  2) node 4 - next unignored sibling in the logical document.
//
// In both cases, this method implements approach (2).
//
// TODO(chrishall): Can we remove this non-reflexive case by forbidding
//   GetNextUnignoredSibling calls on an ignored started node?
// Note: this means that Next/Previous-UnignoredSibling are not reflexive if
// either of the nodes in question are ignored. From above we get an example:
//   NextUnignoredSibling(3)     is 4, but
//   PreviousUnignoredSibling(4) is 5.
//
// The view of unignored siblings for node 3 includes both node 2 and node 4:
//    2 <-- [3(i)] --> 4
//
// Whereas nodes 2, 5, and 4 do not consider node 3 to be an unignored sibling:
// null <-- [2] --> 5
//    2 <-- [5] --> 4
//    5 <-- [4] --> null
AXNode* AXNode::GetNextUnignoredSibling() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXNode* current = this;

  // If there are children of the |current| node still to consider.
  bool considerChildren = false;

  while (current) {
    // A |candidate| sibling to consider.
    // If it is unignored then we have found our result.
    // Otherwise promote it to |current| and consider its children.
    AXNode* candidate;

    if (considerChildren && (candidate = current->GetFirstChild())) {
      if (!candidate->IsIgnored())
        return candidate;
      current = candidate;

    } else if ((candidate = current->GetNextSibling())) {
      if (!candidate->IsIgnored())
        return candidate;
      current = candidate;
      // Look through the ignored candidate node to consider their children as
      // though they were siblings.
      considerChildren = true;

    } else {
      // Continue our search through a parent iff they are ignored.
      //
      // If |current| has an ignored parent, then we consider the parent's
      // siblings as though they were siblings of |current|.
      //
      // Given a tree:
      //   1
      //   ├── 2(?)
      //   │   └── [4]
      //   └── 3
      //
      // Node 4's view of siblings:
      //   literal tree:   null <-- [4] --> null
      //
      // If node 2 is not ignored, then node 4's view doesn't change, and we
      // have no more nodes to consider:
      //   unignored tree: null <-- [4] --> null
      //
      // If instead node 2 is ignored, then node 4's view of siblings grows to
      // include node 3, and we have more nodes to consider:
      //   unignored tree: null <-- [4] --> 3
      current = current->GetParent();
      if (!current || !current->IsIgnored())
        return nullptr;

      // We have already considered all relevant descendants of |current|.
      considerChildren = false;
    }
  }

  return nullptr;
}

AXNode* AXNode::GetPreviousSibling() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  DCHECK(GetParent() || !GetIndexInParent())
      << "Root nodes lack a parent. Their index_in_parent should be 0.";
  size_t index = GetIndexInParent();
  if (index == 0)
    return nullptr;
  return GetParent()->GetChildAtIndex(index - 1);
}

// Search for the previous sibling of this node, skipping over any ignored nodes
// encountered.
//
// In our search for a sibling:
//   If we find an ignored sibling, we may consider its children as siblings.
//   If we run out of siblings, we may consider an ignored parent's siblings as
//     our own.
//
// See the documentation for |GetNextUnignoredSibling| for more details.
AXNode* AXNode::GetPreviousUnignoredSibling() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXNode* current = this;

  // If there are children of the |current| node still to consider.
  bool considerChildren = false;

  while (current) {
    // A |candidate| sibling to consider.
    // If it is unignored then we have found our result.
    // Otherwise promote it to |current| and consider its children.
    AXNode* candidate;

    if (considerChildren && (candidate = current->GetLastChild())) {
      if (!candidate->IsIgnored())
        return candidate;
      current = candidate;

    } else if ((candidate = current->GetPreviousSibling())) {
      if (!candidate->IsIgnored())
        return candidate;
      current = candidate;
      // Look through the ignored candidate node to consider their children as
      // though they were siblings.
      considerChildren = true;

    } else {
      // Continue our search through a parent iff they are ignored.
      //
      // If |current| has an ignored parent, then we consider the parent's
      // siblings as though they were siblings of |current|.
      //
      // Given a tree:
      //   1
      //   ├── 2
      //   └── 3(?)
      //       └── [4]
      //
      // Node 4's view of siblings:
      //   literal tree:   null <-- [4] --> null
      //
      // If node 3 is not ignored, then node 4's view doesn't change, and we
      // have no more nodes to consider:
      //   unignored tree: null <-- [4] --> null
      //
      // If instead node 3 is ignored, then node 4's view of siblings grows to
      // include node 2, and we have more nodes to consider:
      //   unignored tree:    2 <-- [4] --> null
      current = current->GetParent();
      if (!current || !current->IsIgnored())
        return nullptr;

      // We have already considered all relevant descendants of |current|.
      considerChildren = false;
    }
  }

  return nullptr;
}

AXNode* AXNode::GetNextUnignoredInTreeOrder() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (GetUnignoredChildCount())
    return GetFirstUnignoredChild();

  const AXNode* node = this;
  while (node) {
    AXNode* sibling = node->GetNextUnignoredSibling();
    if (sibling)
      return sibling;

    node = node->GetUnignoredParent();
  }

  return nullptr;
}

AXNode* AXNode::GetPreviousUnignoredInTreeOrder() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  AXNode* sibling = GetPreviousUnignoredSibling();
  if (!sibling)
    return GetUnignoredParent();

  if (sibling->GetUnignoredChildCount())
    return sibling->GetDeepestLastUnignoredDescendant();

  return sibling;
}

AXNode::AllChildIterator AXNode::AllChildrenBegin() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return AllChildIterator(this, GetFirstChild());
}

AXNode::AllChildIterator AXNode::AllChildrenEnd() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return AllChildIterator(this, nullptr);
}

AXNode::AllChildCrossingTreeBoundaryIterator
AXNode::AllChildrenCrossingTreeBoundaryBegin() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return AllChildCrossingTreeBoundaryIterator(
      this, GetFirstChildCrossingTreeBoundary());
}

AXNode::AllChildCrossingTreeBoundaryIterator
AXNode::AllChildrenCrossingTreeBoundaryEnd() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return AllChildCrossingTreeBoundaryIterator(this, nullptr);
}

AXNode::UnignoredChildIterator AXNode::UnignoredChildrenBegin() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return UnignoredChildIterator(this, GetFirstUnignoredChild());
}

AXNode::UnignoredChildIterator AXNode::UnignoredChildrenEnd() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return UnignoredChildIterator(this, nullptr);
}

AXNode::UnignoredChildCrossingTreeBoundaryIterator
AXNode::UnignoredChildrenCrossingTreeBoundaryBegin() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return UnignoredChildCrossingTreeBoundaryIterator(
      this, GetFirstUnignoredChildCrossingTreeBoundary());
}

AXNode::UnignoredChildCrossingTreeBoundaryIterator
AXNode::UnignoredChildrenCrossingTreeBoundaryEnd() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return UnignoredChildCrossingTreeBoundaryIterator(this, nullptr);
}

bool AXNode::CanFireEvents() const {
  // TODO(nektar): Cache the `IsChildOfLeaf` state in `AXComputedNodeData`.
  return !IsChildOfLeaf();
}

AXNode* AXNode::GetLowestCommonAncestor(const AXNode& other) {
  if (this == &other)
    return this;

  AXNode* common_ancestor = nullptr;
  base::stack<AXNode*> our_ancestors =
      GetAncestorsCrossingTreeBoundaryAsStack();
  base::stack<AXNode*> other_ancestors =
      other.GetAncestorsCrossingTreeBoundaryAsStack();
  while (!our_ancestors.empty() && !other_ancestors.empty() &&
         our_ancestors.top() == other_ancestors.top()) {
    common_ancestor = our_ancestors.top();
    our_ancestors.pop();
    other_ancestors.pop();
  }
  return common_ancestor;
}

std::optional<int> AXNode::CompareTo(const AXNode& other) const {
  if (this == &other)
    return 0;

  AXNode* common_ancestor = nullptr;
  base::stack<AXNode*> our_ancestors =
      GetAncestorsCrossingTreeBoundaryAsStack();
  base::stack<AXNode*> other_ancestors =
      other.GetAncestorsCrossingTreeBoundaryAsStack();
  while (!our_ancestors.empty() && !other_ancestors.empty() &&
         our_ancestors.top() == other_ancestors.top()) {
    common_ancestor = our_ancestors.top();
    our_ancestors.pop();
    other_ancestors.pop();
  }

  if (!common_ancestor)
    return std::nullopt;
  if (common_ancestor == this)
    return -1;
  if (common_ancestor == &other)
    return 1;

  if (our_ancestors.empty() || other_ancestors.empty()) {
    NOTREACHED_IN_MIGRATION()
        << "The common ancestor should be followed by two uncommon "
           "children in the two corresponding lists of ancestors.";
    return std::nullopt;
  }

  size_t this_uncommon_ancestor_index = our_ancestors.top()->GetIndexInParent();
  size_t other_uncommon_ancestor_index =
      other_ancestors.top()->GetIndexInParent();
  DCHECK_NE(this_uncommon_ancestor_index, other_uncommon_ancestor_index)
      << "Deepest uncommon ancestors should truly be uncommon, i.e. not be the "
         "same node.";
  return this_uncommon_ancestor_index - other_uncommon_ancestor_index;
}

bool AXNode::IsText() const {
  // Regular list markers only expose their alternative text, but do not expose
  // their descendants; and the descendants should be ignored. This is because
  // the alternative text depends on the counter style and can be different from
  // the actual (visual) marker text, and hence, inconsistent with the
  // descendants. We treat a list marker as non-text only if it still has
  // non-ignored descendants, which happens only when:
  // - The list marker itself is ignored but the descendants are not
  // - Or the list marker contains images
  if (GetRole() == ax::mojom::Role::kListMarker)
    return !IsIgnored() && !GetUnignoredChildCount();
  return ui::IsText(GetRole());
}

bool AXNode::IsLineBreak() const {
  // The last condition captures inline text nodes whose only content is an '\n'
  // character.
  return GetRole() == ax::mojom::Role::kLineBreak ||
         (GetRole() == ax::mojom::Role::kInlineTextBox &&
          GetBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject));
}

void AXNode::SetData(const AXNodeData& src) {
  data_ = src;
}

void AXNode::SetLocation(AXNodeID offset_container_id,
                         const gfx::RectF& location,
                         gfx::Transform* transform) {
  data_.relative_bounds.offset_container_id = offset_container_id;
  data_.relative_bounds.bounds = location;
  if (transform) {
    data_.relative_bounds.transform =
        std::make_unique<gfx::Transform>(*transform);
  } else {
    data_.relative_bounds.transform.reset();
  }
}

void AXNode::SetScrollInfo(const int& scroll_x, const int& scroll_y) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kScrollX, scroll_x);
  data_.AddIntAttribute(ax::mojom::IntAttribute::kScrollY, scroll_y);
}

void AXNode::GetScrollInfo(int* scroll_x, int* scroll_y) const {
  *scroll_x = GetIntAttribute(ax::mojom::IntAttribute::kScrollX);
  *scroll_y = GetIntAttribute(ax::mojom::IntAttribute::kScrollY);
}

void AXNode::SetIndexInParent(size_t index_in_parent) {
  index_in_parent_ = index_in_parent;
}

void AXNode::UpdateUnignoredCachedValues() {
  computed_node_data_.reset();
  if (!IsIgnored())
    UpdateUnignoredCachedValuesRecursive(0);
}

void AXNode::SwapChildren(
    std::vector<raw_ptr<AXNode, VectorExperimental>>* children) {
  children->swap(children_);
}

bool AXNode::IsDescendantOf(const AXNode* ancestor) const {
  if (!ancestor)
    return false;
  if (this == ancestor)
    return true;
  if (const AXNode* parent = GetParent())
    return parent->IsDescendantOf(ancestor);
  return false;
}

bool AXNode::IsDescendantOfCrossingTreeBoundary(const AXNode* ancestor) const {
  if (!ancestor)
    return false;
  if (this == ancestor)
    return true;
  if (const AXNode* parent = GetParentCrossingTreeBoundary())
    return parent->IsDescendantOfCrossingTreeBoundary(ancestor);
  return false;
}

SkColor AXNode::ComputeColor() const {
  return ComputeColorAttribute(ax::mojom::IntAttribute::kColor);
}

SkColor AXNode::ComputeBackgroundColor() const {
  return ComputeColorAttribute(ax::mojom::IntAttribute::kBackgroundColor);
}

SkColor AXNode::ComputeColorAttribute(ax::mojom::IntAttribute attr) const {
  SkColor color = GetIntAttribute(attr);
  AXNode* ancestor = GetParent();

  // If the color has some transparency, keep blending with background
  // colors until we get an opaque color or reach the root.
  while (ancestor && SkColorGetA(color) != SK_AlphaOPAQUE) {
    SkColor background_color = ancestor->GetIntAttribute(attr);
    color = color_utils::GetResultingPaintColor(color, background_color);
    ancestor = ancestor->GetParent();
  }

  return color;
}

AXTreeManager* AXNode::GetManager() const {
  return AXTreeManager::FromID(tree_->GetAXTreeID());
}

bool AXNode::HasVisibleCaretOrSelection() const {
  const AXSelection selection = GetSelection();
  const AXNode* focus = tree()->GetFromId(selection.focus_object_id);
  if (!focus || !focus->IsDescendantOf(this))
    return false;

  // A selection or the caret will be visible in a focused text field (including
  // a content editable).
  const AXNode* text_field = GetTextFieldAncestor();
  if (text_field)
    return true;

  // The selection will be visible in non-editable content only if it is not
  // collapsed.
  return !selection.IsCollapsed();
}

AXSelection AXNode::GetSelection() const {
  DCHECK(tree()) << "Cannot retrieve the current selection if the node is not "
                    "attached to an accessibility tree.\n"
                 << *this;
  return tree()->GetSelection();
}

AXSelection AXNode::GetUnignoredSelection() const {
  DCHECK(tree()) << "Cannot retrieve the current selection if the node is not "
                    "attached to an accessibility tree.\n"
                 << *this;
  AXSelection selection = tree()->GetUnignoredSelection();

  // "selection.anchor_offset" and "selection.focus_ofset" might need to be
  // adjusted if the anchor or the focus nodes include ignored children.
  //
  // TODO(nektar): Move this logic into its own "AXSelection" class and cache
  // the result for faster reuse.
  const AXNode* anchor = tree()->GetFromId(selection.anchor_object_id);
  if (anchor && !anchor->IsLeaf()) {
    DCHECK_GE(selection.anchor_offset, 0);
    if (static_cast<size_t>(selection.anchor_offset) <
        anchor->GetChildCount()) {
      const AXNode* anchor_child =
          anchor->GetChildAtIndex(selection.anchor_offset);
      DCHECK(anchor_child);
      selection.anchor_offset =
          static_cast<int>(anchor_child->GetUnignoredIndexInParent());
    } else {
      selection.anchor_offset =
          static_cast<int>(anchor->GetUnignoredChildCount());
    }
  }

  const AXNode* focus = tree()->GetFromId(selection.focus_object_id);
  if (focus && !focus->IsLeaf()) {
    DCHECK_GE(selection.focus_offset, 0);
    if (static_cast<size_t>(selection.focus_offset) < focus->GetChildCount()) {
      const AXNode* focus_child =
          focus->GetChildAtIndex(selection.focus_offset);
      DCHECK(focus_child);
      selection.focus_offset =
          static_cast<int>(focus_child->GetUnignoredIndexInParent());
    } else {
      selection.focus_offset =
          static_cast<int>(focus->GetUnignoredChildCount());
    }
  }
  return selection;
}

bool AXNode::HasIntAttribute(ax::mojom::IntAttribute attribute) const {
  if (data().HasIntAttribute(attribute)) {
    return true;
  }
  return CanComputeIntAttribute(attribute);
}

bool AXNode::CanComputeIntAttribute(ax::mojom::IntAttribute attribute) const {
  // NOTE: This method must be kept strictly in sync with parent deferral logic
  // in AXInlineTextBox::(next|previous)OnLine.
  if (attribute != ax::mojom::IntAttribute::kNextOnLineId &&
      attribute != ax::mojom::IntAttribute::kPreviousOnLineId) {
    return false;
  }

  if (!::features::IsAccessibilityPruneRedundantInlineConnectivityEnabled()) {
    return false;
  }

  // Inline text boxes share the same next- or previous-on-line ID with the
  // parent when traversing across the parent's boundary. Determination of the
  // next- or previous-on-line IDs for this type of connectivity is expensive
  // during the serialization process. Unnecessary to duplicate the effort.
  if (data().role != ax::mojom::Role::kInlineTextBox) {
    return false;
  }

  if (!GetParent()) {
    return false;
  }

  if (this == GetParent()->GetFirstChild() &&
      attribute == ax::mojom::IntAttribute::kPreviousOnLineId) {
    return GetParent()->data().HasIntAttribute(attribute);
  }

  if (this == GetParent()->GetLastChild() &&
      attribute == ax::mojom::IntAttribute::kNextOnLineId) {
    return GetParent()->data().HasIntAttribute(attribute);
  }

  return false;
}

int AXNode::GetIntAttribute(ax::mojom::IntAttribute attribute) const {
  int value = data().GetIntAttribute(attribute);
  if (value != kDefaultIntValue || data().HasIntAttribute(attribute)) {
    return value;
  }
  if (CanComputeIntAttribute(attribute)) {
    return GetParent()->data().GetIntAttribute(attribute);
  }
  return kDefaultIntValue;
}

bool AXNode::HasStringAttribute(ax::mojom::StringAttribute attribute) const {
  if (data().HasStringAttribute(attribute)) {
    return true;
  }
  return CanComputeStringAttribute(attribute);
}

bool AXNode::CanComputeStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  switch (attribute) {
    case ax::mojom::StringAttribute::kValue:
      // The value attribute could be computed on the browser for content
      // editables and ARIA text/search boxes.
      return data().IsNonAtomicTextField();

    case ax::mojom::StringAttribute::kName:
      // The name may be suppressed when serializing an AXInlineTextBox if it
      // can be inferred from the parent.
      return ::features::IsAccessibilityPruneRedundantInlineTextEnabled() &&
             data().role == ax::mojom::Role::kInlineTextBox &&
             data().GetNameFrom() == ax::mojom::NameFrom::kContents &&
             GetParent() &&
             GetParent()->data().GetNameFrom() ==
                 ax::mojom::NameFrom::kContents &&
             GetParent()->data().HasStringAttribute(
                 ax::mojom::StringAttribute::kName);

    default:
      return false;
  }
}

const std::string& AXNode::GetStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  if (data().HasStringAttribute(attribute)) {
    return data().GetStringAttribute(attribute);
  }
  if (CanComputeStringAttribute(attribute)) {
    // Computed string attributes are cached.
    return GetComputedNodeData().ComputeAttributeUTF8(attribute);
  }
  return base::EmptyString();
}

std::u16string AXNode::GetString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  // String values in AXNodeData are in utf8 format. The getter for UTF16 does
  // an implicit conversion.
  if (data().HasStringAttribute(attribute)) {
    const std::string& value_utf8 = data().GetStringAttribute(attribute);
    return base::UTF8ToUTF16(value_utf8);
  }

  if (CanComputeStringAttribute(attribute)) {
    return GetComputedNodeData().ComputeAttributeUTF16(attribute);
  }
  return std::u16string();
}

bool AXNode::HasInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  for (const AXNode* current_node = this; current_node;
       current_node = current_node->GetParent()) {
    if (current_node->HasStringAttribute(attribute))
      return true;
  }
  return false;
}

const std::string& AXNode::GetInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  for (const AXNode* current_node = this; current_node;
       current_node = current_node->GetParent()) {
    if (current_node->HasStringAttribute(attribute))
      return current_node->GetStringAttribute(attribute);
  }
  return base::EmptyString();
}

std::u16string AXNode::GetInheritedString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  return base::UTF8ToUTF16(GetInheritedStringAttribute(attribute));
}

bool AXNode::HasIntListAttribute(ax::mojom::IntListAttribute attribute) const {
  if (data().HasIntListAttribute(attribute)) {
    return true;
  }
  return CanComputeIntListAttribute(attribute);
}

bool AXNode::CanComputeIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  switch (attribute) {
    case ax::mojom::IntListAttribute::kLineStarts:
    case ax::mojom::IntListAttribute::kLineEnds:
    case ax::mojom::IntListAttribute::kSentenceStarts:
    case ax::mojom::IntListAttribute::kSentenceEnds:
    case ax::mojom::IntListAttribute::kWordStarts:
    case ax::mojom::IntListAttribute::kWordEnds:
      return true;

    default:
      return false;
  }
}

const std::vector<int32_t>& AXNode::GetIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  if (data().HasIntListAttribute(attribute)) {
    return data().GetIntListAttribute(attribute);
  }
  if (CanComputeIntListAttribute(attribute)) {
    return GetComputedNodeData().ComputeAttribute(attribute);
  }
  return data().GetIntListAttribute(ax::mojom::IntListAttribute::kNone);
}

AXLanguageInfo* AXNode::GetLanguageInfo() const {
  return language_info_.get();
}

void AXNode::SetLanguageInfo(std::unique_ptr<AXLanguageInfo> lang_info) {
  language_info_ = std::move(lang_info);
}

void AXNode::ClearLanguageInfo() {
  language_info_.reset();
}

const AXComputedNodeData& AXNode::GetComputedNodeData() const {
  if (!computed_node_data_)
    computed_node_data_ = std::make_unique<AXComputedNodeData>(*this);
  return *computed_node_data_;
}

void AXNode::ClearComputedNodeData() {
  computed_node_data_.reset();
}

const std::string& AXNode::GetNameUTF8() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return this->GetStringAttribute(ax::mojom::StringAttribute::kName);
}

std::u16string AXNode::GetNameUTF16() const {
  // Storing a copy of the name in UTF16 would probably not be helpful because
  // it could potentially double the memory usage of AXTree.
  return base::UTF8ToUTF16(GetNameUTF8());
}

const std::u16string& AXNode::GetHypertext() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  // TODO(nektar): Introduce proper caching of hypertext via
  // `AXHypertext::needs_update`.
  hypertext_ = AXHypertext();

  // Hypertext is not exposed for descendants of leaf nodes. For such nodes,
  // their text content is equivalent to their hypertext. Otherwise, we would
  // never be able to compute equivalent ancestor positions in atomic text
  // fields given an AXPosition on an inline text box descendant, because there
  // is often an ignored generic container between the text descendants and the
  // text field node.
  //
  // For example, look at the following accessibility tree and the text
  // positions indicated using "<>" symbols in the text content of every node,
  // and then imagine what would happen if the generic container was represented
  // by an "embedded object replacement character" in the text of its text field
  // parent.
  // ++kTextField "Hell<o>" IsLeaf=true
  // ++++kGenericContainer "Hell<o>" ignored IsChildOfLeaf=true
  // ++++++kStaticText "Hell<o>" IsChildOfLeaf=true
  // ++++++++kInlineTextBox "Hell<o>" IsChildOfLeaf=true

  if (IsLeaf() || IsChildOfLeaf()) {
    hypertext_.hypertext = GetTextContentUTF16();
  } else {
    // Construct the hypertext for this node, which contains the concatenation
    // of the text content of this node's textual children, and an "object
    // replacement character" for all the other children.
    //
    // Note that the word "hypertext" comes from the IAccessible2 Standard and
    // has nothing to do with HTML.
    static const base::NoDestructor<std::u16string> embedded_character_str(
        AXNode::kEmbeddedObjectCharacterUTF16);
    auto first = UnignoredChildrenCrossingTreeBoundaryBegin();
    for (auto iter = first; iter != UnignoredChildrenCrossingTreeBoundaryEnd();
         ++iter) {
      // Similar to Firefox, we don't expose text nodes in IAccessible2 and ATK
      // hypertext with the embedded object character. We copy all of their text
      // instead.
      if (iter->IsText()) {
        hypertext_.hypertext += iter->GetTextContentUTF16();
      } else {
        int character_offset = static_cast<int>(hypertext_.hypertext.size());
        auto inserted =
            hypertext_.hypertext_offset_to_hyperlink_child_index.emplace(
                character_offset, static_cast<int>(std::distance(first, iter)));
        DCHECK(inserted.second) << "An embedded object at " << character_offset
                                << " has already been encountered.";
        hypertext_.hypertext += *embedded_character_str;
      }
    }
  }

  hypertext_.needs_update = false;
  return hypertext_.hypertext;
}

const std::map<int, int>& AXNode::GetHypertextOffsetToHyperlinkChildIndex()
    const {
  // TODO(nektar): Introduce proper caching of hypertext via
  // `AXHypertext::needs_update`.
  GetHypertext();  // Update `hypertext_` if not up-to-date.
  return hypertext_.hypertext_offset_to_hyperlink_child_index;
}

const std::string& AXNode::GetTextContentUTF8() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return GetComputedNodeData().GetOrComputeTextContentUTF8();
}

const std::u16string& AXNode::GetTextContentUTF16() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return GetComputedNodeData().GetOrComputeTextContentUTF16();
}

int AXNode::GetTextContentLengthUTF8() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return GetComputedNodeData().GetOrComputeTextContentLengthUTF8();
}

int AXNode::GetTextContentLengthUTF16() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return GetComputedNodeData().GetOrComputeTextContentLengthUTF16();
}

gfx::RectF AXNode::GetTextContentRangeBoundsUTF16(int start_offset,
                                                  int end_offset) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  DCHECK_LE(start_offset, end_offset)
      << "Invalid `start_offset` and `end_offset`.\n"
      << start_offset << ' ' << end_offset << "\nin\n"
      << *this;

  int text_content_length = GetTextContentLengthUTF16();
  // Since we DCHECK that `start_offset` <= `end_offset`, there is no need to
  // check whether `start_offset` is also in range.
  if (end_offset > text_content_length) {
    return gfx::RectF();
  }

  const std::vector<int32_t>& character_offsets =
      GetIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets);
  int character_offsets_length =
      base::checked_cast<int>(character_offsets.size());
  // Character offsets are always based on the UTF-16 representation of the
  // text.
  if (character_offsets_length < GetTextContentLengthUTF16()) {
    // Blink might not return pixel offsets for all characters. Clamp the
    // character range to be within the number of provided pixels. Note that the
    // first character always starts at pixel 0, so an offset for that character
    // is not provided.
    //
    // TODO(accessibility): We need to fix this bug in Blink.
    start_offset = std::min(start_offset, character_offsets_length);
    end_offset = std::min(end_offset, character_offsets_length);
  }

  // TODO(nektar): Remove all this code and fix up the character offsets vector
  // itself.
  int start_pixel_offset =
      start_offset > 0
          ? character_offsets[base::checked_cast<size_t>(start_offset - 1)]
          : 0;
  int end_pixel_offset =
      end_offset > 0
          ? character_offsets[base::checked_cast<size_t>(end_offset - 1)]
          : 0;
  int max_pixel_offset = character_offsets_length > 0
                             ? character_offsets[character_offsets_length - 1]
                             : 0;
  const gfx::RectF& node_bounds = data().relative_bounds.bounds;

  gfx::RectF out_bounds;
  switch (static_cast<ax::mojom::WritingDirection>(
      GetIntAttribute(ax::mojom::IntAttribute::kTextDirection))) {
    case ax::mojom::WritingDirection::kNone:
    case ax::mojom::WritingDirection::kLtr:
      out_bounds = gfx::RectF(start_pixel_offset, 0,
                              end_pixel_offset - start_pixel_offset,
                              node_bounds.height());
      break;
    case ax::mojom::WritingDirection::kRtl: {
      int left = max_pixel_offset - end_pixel_offset;
      int right = max_pixel_offset - start_pixel_offset;
      out_bounds = gfx::RectF(left, 0, right - left, node_bounds.height());
      break;
    }
    case ax::mojom::WritingDirection::kTtb:
      out_bounds = gfx::RectF(0, start_pixel_offset, node_bounds.width(),
                              end_pixel_offset - start_pixel_offset);
      break;
    case ax::mojom::WritingDirection::kBtt: {
      int top = max_pixel_offset - end_pixel_offset;
      int bottom = max_pixel_offset - start_pixel_offset;
      out_bounds = gfx::RectF(0, top, node_bounds.width(), bottom - top);
      break;
    }
  }
  return out_bounds;
}

std::string AXNode::GetLanguage() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  // Walk up tree considering both detected and author declared languages.
  for (const AXNode* cur = this; cur; cur = cur->GetParent()) {
    // If language detection has assigned a language then we prefer that.
    const AXLanguageInfo* lang_info = cur->GetLanguageInfo();
    if (lang_info && !lang_info->language.empty())
      return lang_info->language;

    // If the page author has declared a language attribute we fallback to that.
    if (cur->HasStringAttribute(ax::mojom::StringAttribute::kLanguage))
      return cur->GetStringAttribute(ax::mojom::StringAttribute::kLanguage);
  }

  return std::string();
}

std::string AXNode::GetValueForControl() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (data().IsTextField()) {
    // Returns the value of a text field. If necessary, computes the value from
    // the field's internal representation in the accessibility tree, in order
    // to minimize cross-process communication between the renderer and the
    // browser processes.
    return GetStringAttribute(ax::mojom::StringAttribute::kValue);
  }

  if (data().IsRangeValueSupported())
    return GetTextForRangeValue();
  if (GetRole() == ax::mojom::Role::kColorWell)
    return GetValueForColorWell();
  if (!IsControl(GetRole()))
    return std::string();

  return GetStringAttribute(ax::mojom::StringAttribute::kValue);
}

std::ostream& operator<<(std::ostream& stream, const AXNode& node) {
  stream << node.data().ToString(/*verbose*/ false);
  if (node.tree()->GetTreeUpdateInProgressState()) {
    // Prevent calling node traversal methods when it's illegal to do so.
    return stream;
  }
  if (node.GetUnignoredChildCountCrossingTreeBoundary()) {
    stream << " unignored_child_ids=";
    bool needs_comma = false;
    for (auto it = node.UnignoredChildrenBegin();
         it != node.UnignoredChildrenEnd(); ++it) {
      if (needs_comma) {
        stream << ",";
      } else {
        needs_comma = true;
      }
      stream << it.get()->data().id;
    }
  }
  if (node.IsLeaf()) {
    stream << " is_leaf";
  }
  if (node.IsChildOfLeaf()) {
    stream << " is_child_of_leaf";
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const AXNode* node) {
  if (!node) {
    return stream << "null";
  }

  return stream << *node;
}

bool AXNode::IsTable() const {
  return IsTableLike(GetRole());
}

std::optional<int> AXNode::GetTableColCount() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::nullopt;
  return static_cast<int>(table_info->col_count);
}

std::optional<int> AXNode::GetTableRowCount() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::nullopt;
  return static_cast<int>(table_info->row_count);
}

std::optional<int> AXNode::GetTableAriaColCount() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::nullopt;
  return std::make_optional(table_info->aria_col_count);
}

std::optional<int> AXNode::GetTableAriaRowCount() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::nullopt;
  return std::make_optional(table_info->aria_row_count);
}

std::optional<int> AXNode::GetTableCellCount() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::nullopt;

  return static_cast<int>(table_info->unique_cell_ids.size());
}

AXNode* AXNode::GetTableCellFromIndex(int index) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return nullptr;

  // There is a table but there is no cell with the given index.
  if (index < 0 ||
      static_cast<size_t>(index) >= table_info->unique_cell_ids.size()) {
    return nullptr;
  }

  return tree_->GetFromId(
      table_info->unique_cell_ids[static_cast<size_t>(index)]);
}

AXNode* AXNode::GetTableCaption() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return nullptr;

  return tree_->GetFromId(table_info->caption_id);
}

AXNode* AXNode::GetTableCellFromCoords(int row_index, int col_index) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return nullptr;

  // There is a table but the given coordinates are outside the table.
  if (row_index < 0 ||
      static_cast<size_t>(row_index) >= table_info->row_count ||
      col_index < 0 ||
      static_cast<size_t>(col_index) >= table_info->col_count) {
    return nullptr;
  }

  return tree_->GetFromId(table_info->cell_ids[static_cast<size_t>(row_index)]
                                              [static_cast<size_t>(col_index)]);
}

AXNode* AXNode::GetTableCellFromAriaCoords(int aria_row_index,
                                           int aria_col_index) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info) {
    return nullptr;
  }

  if (aria_row_index < 1 || aria_row_index > table_info->aria_row_count ||
      aria_col_index < 1 || aria_col_index > table_info->aria_col_count) {
    return nullptr;
  }

  // Aria rows/columns are not guaranteed to be contiguous, and can also
  // span multiple "rows" or "columns".
  // So while we do need to check many of the internal rows/columns, we can do
  // some skipping around, and don't need to continue to search if we are past
  // the specified row/column.
  for (size_t row = 0; row < table_info->row_count; ++row) {
    for (size_t col = 0; col < table_info->col_count; ++col) {
      AXNode* node = tree_->GetFromId(table_info->cell_ids[row][col]);
      CHECK(node);

      std::optional<int> current_aria_row = node->GetTableCellAriaRowIndex();
      std::optional<int> current_aria_col = node->GetTableCellAriaColIndex();
      if (!current_aria_row || *current_aria_row < aria_row_index) {
        break;
      } else if (*current_aria_row > aria_row_index) {
        return nullptr;
      }
      if (!current_aria_col || *current_aria_col < aria_col_index) {
        continue;
      } else if (*current_aria_col > aria_col_index) {
        return nullptr;
      }
      DCHECK(*current_aria_row == aria_row_index &&
             *current_aria_col == aria_col_index);
      return node;
    }
  }
  return nullptr;
}

std::vector<AXNodeID> AXNode::GetTableColHeaderNodeIds() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::vector<AXNodeID>();

  std::vector<AXNodeID> col_header_ids;
  // Flatten and add column header ids of each column to |col_header_ids|.
  for (std::vector<AXNodeID> col_headers_at_index : table_info->col_headers) {
    col_header_ids.insert(col_header_ids.end(), col_headers_at_index.begin(),
                          col_headers_at_index.end());
  }

  return col_header_ids;
}

std::vector<AXNodeID> AXNode::GetTableColHeaderNodeIds(int col_index) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::vector<AXNodeID>();

  if (col_index < 0 || static_cast<size_t>(col_index) >= table_info->col_count)
    return std::vector<AXNodeID>();

  return std::vector<AXNodeID>(
      table_info->col_headers[static_cast<size_t>(col_index)]);
}

std::vector<AXNodeID> AXNode::GetTableRowHeaderNodeIds(int row_index) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::vector<AXNodeID>();

  if (row_index < 0 || static_cast<size_t>(row_index) >= table_info->row_count)
    return std::vector<AXNodeID>();

  return std::vector<AXNodeID>(
      table_info->row_headers[static_cast<size_t>(row_index)]);
}

std::vector<AXNodeID> AXNode::GetTableUniqueCellIds() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::vector<AXNodeID>();

  return std::vector<AXNodeID>(table_info->unique_cell_ids);
}

const std::vector<raw_ptr<AXNode, VectorExperimental>>*
AXNode::GetExtraMacNodes() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  // Should only be available on the table node itself, not any of its children.
  const AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return nullptr;

  return &table_info->extra_mac_nodes;
}

bool AXNode::IsGenerated() const {
  bool is_generated_node = id() < 0 && id() > kInitialEmptyDocumentRootNodeID;
#if DCHECK_IS_ON()
  // Currently, the only generated nodes are columns and table header
  // containers, and when those roles occur, they are always extra mac nodes.
  // This could change in the future.
  bool is_extra_mac_node_role =
      GetRole() == ax::mojom::Role::kColumn ||
      GetRole() == ax::mojom::Role::kTableHeaderContainer;
  DCHECK_EQ(is_generated_node, is_extra_mac_node_role);
#endif
  return is_generated_node;
}

//
// Table row-like nodes.
//

bool AXNode::IsTableRow() const {
  return ui::IsTableRow(GetRole());
}

std::optional<int> AXNode::GetTableRowRowIndex() const {
  if (!IsTableRow())
    return std::nullopt;

  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::nullopt;

  const auto& iter = table_info->row_id_to_index.find(id());
  if (iter == table_info->row_id_to_index.end())
    return std::nullopt;
  return static_cast<int>(iter->second);
}

std::vector<AXNodeID> AXNode::GetTableRowNodeIds() const {
  std::vector<AXNodeID> row_node_ids;
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return row_node_ids;

  for (AXNode* node : table_info->row_nodes)
    row_node_ids.push_back(node->id());

  return row_node_ids;
}

#if BUILDFLAG(IS_APPLE)

//
// Table column-like nodes. These nodes are only present on macOS.
//

bool AXNode::IsTableColumn() const {
  return ui::IsTableColumn(GetRole());
}

std::optional<int> AXNode::GetTableColColIndex() const {
  if (!IsTableColumn())
    return std::nullopt;

  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::nullopt;

  int index = 0;
  for (const AXNode* node : table_info->extra_mac_nodes) {
    if (node == this)
      break;
    index++;
  }
  return index;
}

#endif  // BUILDFLAG(IS_APPLE)

//
// Table cell-like nodes.
//

bool AXNode::IsTableCellOrHeader() const {
  return IsCellOrTableHeader(GetRole());
}

std::optional<int> AXNode::GetTableCellIndex() const {
  if (!IsTableCellOrHeader())
    return std::nullopt;

  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::nullopt;

  const auto& iter = table_info->cell_id_to_index.find(id());
  if (iter != table_info->cell_id_to_index.end())
    return static_cast<int>(iter->second);
  return std::nullopt;
}

std::optional<int> AXNode::GetTableCellColIndex() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::nullopt;

  std::optional<int> index = GetTableCellIndex();
  if (!index)
    return std::nullopt;

  return static_cast<int>(table_info->cell_data_vector[*index].col_index);
}

std::optional<int> AXNode::GetTableCellRowIndex() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::nullopt;

  // If it's a table row, use the first cell within.
  if (IsTableRow()) {
    if (const AXNode* first_cell = table_info->GetFirstCellInRow(this)) {
      return first_cell->GetTableCellRowIndex();
    }
    return std::nullopt;
  }

  std::optional<int> index = GetTableCellIndex();
  if (!index)
    return std::nullopt;

  return static_cast<int>(table_info->cell_data_vector[*index].row_index);
}

std::optional<int> AXNode::GetTableCellColSpan() const {
  // If it's not a table cell, don't return a col span.
  if (!IsTableCellOrHeader())
    return std::nullopt;

  // Otherwise, try to return a colspan, with 1 as the default if it's not
  // specified.
  int col_span = GetIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan);
  if (col_span ||
      HasIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan)) {
    return col_span;
  }
  return 1;
}

std::optional<int> AXNode::GetTableCellRowSpan() const {
  // If it's not a table cell, don't return a row span.
  if (!IsTableCellOrHeader())
    return std::nullopt;

  // Otherwise, try to return a row span, with 1 as the default if it's not
  // specified.
  int row_span = GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan);
  if (row_span || HasIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan)) {
    return row_span;
  }
  return 1;
}

std::optional<int> AXNode::GetTableCellAriaColIndex() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::nullopt;

  std::optional<int> index = GetTableCellIndex();
  if (!index)
    return std::nullopt;

  int aria_col_index =
      static_cast<int>(table_info->cell_data_vector[*index].aria_col_index);
  // |aria-colindex| attribute is one-based, value less than 1 is invalid.
  // https://www.w3.org/TR/wai-aria-1.2/#aria-colindex
  return (aria_col_index > 0) ? std::optional<int>(aria_col_index)
                              : std::nullopt;
}

std::optional<int> AXNode::GetTableCellAriaRowIndex() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::nullopt;

  // If it's a table row, use the first cell within.
  if (IsTableRow()) {
    if (const AXNode* first_cell = table_info->GetFirstCellInRow(this)) {
      return first_cell->GetTableCellAriaRowIndex();
    }
    return std::nullopt;
  }

  std::optional<int> index = GetTableCellIndex();
  if (!index) {
    return std::nullopt;
  }

  int aria_row_index =
      static_cast<int>(table_info->cell_data_vector[*index].aria_row_index);
  // |aria-rowindex| attribute is one-based, value less than 1 is invalid.
  // https://www.w3.org/TR/wai-aria-1.2/#aria-rowindex
  return (aria_row_index > 0) ? std::optional<int>(aria_row_index)
                              : std::nullopt;
}

std::vector<AXNodeID> AXNode::GetTableCellColHeaderNodeIds() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info || table_info->col_count <= 0)
    return std::vector<AXNodeID>();

  // If this node is not a cell, then return the headers for the first column.
  int col_index = GetTableCellColIndex().value_or(0);

  return std::vector<AXNodeID>(table_info->col_headers[col_index]);
}

void AXNode::GetTableCellColHeaders(std::vector<AXNode*>* col_headers) const {
  DCHECK(col_headers);

  std::vector<AXNodeID> col_header_ids = GetTableCellColHeaderNodeIds();
  IdVectorToNodeVector(col_header_ids, col_headers);
}

std::vector<AXNodeID> AXNode::GetTableCellRowHeaderNodeIds() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info || table_info->row_count <= 0)
    return std::vector<AXNodeID>();

  // If this node is not a cell, then return the headers for the first row.
  int row_index = GetTableCellRowIndex().value_or(0);

  return std::vector<AXNodeID>(table_info->row_headers[row_index]);
}

void AXNode::GetTableCellRowHeaders(std::vector<AXNode*>* row_headers) const {
  DCHECK(row_headers);

  std::vector<AXNodeID> row_header_ids = GetTableCellRowHeaderNodeIds();
  IdVectorToNodeVector(row_header_ids, row_headers);
}

bool AXNode::IsCellOrHeaderOfAriaGrid() const {
  if (!IsTableCellOrHeader())
    return false;

  const AXNode* node = this;
  while (node && !node->IsTable())
    node = node->GetParent();
  if (!node)
    return false;

  return node->GetRole() == ax::mojom::Role::kGrid ||
         node->GetRole() == ax::mojom::Role::kTreeGrid;
}

AXTableInfo* AXNode::GetAncestorTableInfo() const {
  const AXNode* node = this;
  while (node && !node->IsTable())
    node = node->GetParent();
  if (node)
    return tree_->GetTableInfo(node);
  return nullptr;
}

void AXNode::IdVectorToNodeVector(const std::vector<AXNodeID>& ids,
                                  std::vector<AXNode*>* nodes) const {
  for (AXNodeID id : ids) {
    AXNode* node = tree_->GetFromId(id);
    if (node)
      nodes->push_back(node);
  }
}

std::optional<int> AXNode::GetHierarchicalLevel() const {
  int hierarchical_level =
      GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);

  // According to the WAI_ARIA spec, a defined hierarchical level value is
  // greater than 0.
  // https://www.w3.org/TR/wai-aria-1.1/#aria-level
  if (hierarchical_level > 0)
    return hierarchical_level;

  return std::nullopt;
}

bool AXNode::IsOrderedSetItem() const {
  // Tree grid rows should be treated as ordered set items. Since we don't have
  // a separate row role for tree grid rows, we can't just add the Role::kRow to
  // IsItemLike. We need to validate that the row is indeed part of a tree grid.
  if (IsRowInTreeGrid(GetOrderedSet()))
    return true;

  return ui::IsItemLike(GetRole());
}

bool AXNode::IsOrderedSet() const {
  // Tree grid rows should be considered like ordered set items and a tree grid
  // like an ordered set. Continuing that logic, in order to compute the right
  // PosInSet and SetSize, row groups inside of a tree grid should also be
  // ordered sets.
  if (IsRowGroupInTreeGrid())
    return true;

  return ui::IsSetLike(GetRole());
}

// Uses AXTree's cache to calculate node's PosInSet.
std::optional<int> AXNode::GetPosInSet() const {
  return tree_->GetPosInSet(*this);
}

// Uses AXTree's cache to calculate node's SetSize.
std::optional<int> AXNode::GetSetSize() const {
  return tree_->GetSetSize(*this);
}

// Returns true if the role of ordered set matches the role of item.
// Returns false otherwise.
bool AXNode::SetRoleMatchesItemRole(const AXNode* ordered_set) const {
  ax::mojom::Role item_role = GetRole();

  // Tree grid rows and grouped disclosure triangles should be treated as
  // ordered set items.
  if (IsRowInTreeGrid(ordered_set) ||
      item_role == ax::mojom::Role::kDisclosureTriangle ||
      item_role == ax::mojom::Role::kDisclosureTriangleGrouped) {
    return true;
  }

  // Switch on role of ordered set
  switch (ordered_set->GetRole()) {
    case ax::mojom::Role::kFeed:
      return item_role == ax::mojom::Role::kArticle;
    case ax::mojom::Role::kList:
      return item_role == ax::mojom::Role::kListItem;
    case ax::mojom::Role::kGroup:
      return item_role == ax::mojom::Role::kComment ||
             item_role == ax::mojom::Role::kListItem ||
             item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kMenuItemCheckBox ||
             item_role == ax::mojom::Role::kListBoxOption ||
             item_role == ax::mojom::Role::kTreeItem;
    case ax::mojom::Role::kMenu:
      return item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kMenuItemCheckBox;
    case ax::mojom::Role::kMenuBar:
      return item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kMenuItemCheckBox;
    case ax::mojom::Role::kTabList:
      return item_role == ax::mojom::Role::kTab;
    case ax::mojom::Role::kTree:
    case ax::mojom::Role::kTreeItem:
      return item_role == ax::mojom::Role::kTreeItem;
    case ax::mojom::Role::kListBox:
      return item_role == ax::mojom::Role::kListBoxOption;
    case ax::mojom::Role::kMenuListPopup:
      return item_role == ax::mojom::Role::kMenuListOption ||
             item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kMenuItemCheckBox;
    case ax::mojom::Role::kRadioGroup:
      return item_role == ax::mojom::Role::kRadioButton;
    case ax::mojom::Role::kDescriptionList:
      // Only the term for each description list entry should receive posinset
      // and setsize.
      return item_role == ax::mojom::Role::kTerm;
    case ax::mojom::Role::kComboBoxSelect:
      // kComboBoxSelect wraps a kMenuListPopUp.
      return item_role == ax::mojom::Role::kMenuListPopup;
    default:
      return false;
  }
}

bool AXNode::IsIgnoredContainerForOrderedSet() const {
  return IsIgnored() || IsEmbeddedGroup() ||
         GetRole() == ax::mojom::Role::kDetails ||
         GetRole() == ax::mojom::Role::kLabelText ||
         GetRole() == ax::mojom::Role::kListItem ||
         GetRole() == ax::mojom::Role::kGenericContainer ||
         GetRole() == ax::mojom::Role::kScrollView ||
         GetRole() == ax::mojom::Role::kUnknown;
}

bool AXNode::IsRowInTreeGrid(const AXNode* ordered_set) const {
  // Tree grid rows have the requirement of being focusable, so we use it to
  // avoid iterating over rows that clearly aren't part of a tree grid.
  if (GetRole() != ax::mojom::Role::kRow || !ordered_set || !IsFocusable())
    return false;

  if (ordered_set->GetRole() == ax::mojom::Role::kTreeGrid)
    return true;

  return ordered_set->IsRowGroupInTreeGrid();
}

bool AXNode::IsRowGroupInTreeGrid() const {
  // To the best of our understanding, row groups can't be nested.
  //
  // According to https://www.w3.org/TR/wai-aria-1.1/#rowgroup, a row group is a
  // "structural equivalent to the thead, tfoot, and tbody elements in an HTML
  // table". It is specified in the spec of the thead, tfoot and tbody elements
  // that they need to be children of a table element, meaning that there can
  // only be one level of such elements. We assume the same for row groups.
  if (GetRole() != ax::mojom::Role::kRowGroup)
    return false;

  AXNode* ordered_set = GetOrderedSet();
  return ordered_set && ordered_set->GetRole() == ax::mojom::Role::kTreeGrid;
}

int AXNode::UpdateUnignoredCachedValuesRecursive(int startIndex) {
  int count = 0;
  for (AXNode* child : children()) {
    if (child->IsIgnored()) {
      child->unignored_index_in_parent_ = 0;
      count += child->UpdateUnignoredCachedValuesRecursive(startIndex + count);
    } else {
      child->unignored_index_in_parent_ = startIndex + count++;
    }
  }
  unignored_child_count_ = count;
  return count;
}

// Finds ordered set that contains node.
// Is not required for set's role to match node's role.
AXNode* AXNode::GetOrderedSet() const {
  AXNode* result = GetParent();
  // Continue walking up while parent is invalid, ignored, a generic container,
  // unknown, or embedded group.
  while (result && result->IsIgnoredContainerForOrderedSet()) {
    result = result->GetParent();
  }

  return result;
}

bool AXNode::IsReadOnlySupported() const {
  // Grid cells and headers can't be derived solely from the role (need to check
  // the ancestor chain) so check this first.
  if (IsCellOrHeaderOfAriaGrid())
    return true;

  return ui::IsReadOnlySupported(GetRole());
}

bool AXNode::IsReadOnlyOrDisabled() const {
  switch (data().GetRestriction()) {
    case ax::mojom::Restriction::kReadOnly:
    case ax::mojom::Restriction::kDisabled:
      return true;
    case ax::mojom::Restriction::kNone: {
      if (HasState(ax::mojom::State::kEditable) ||
          HasState(ax::mojom::State::kRichlyEditable)) {
        return false;
      }

      if (ShouldHaveReadonlyStateByDefault(GetRole()))
        return true;

      // When readonly is not supported, we assume that the node is always
      // read-only and mark it as such since this is the default behavior.
      return !IsReadOnlySupported();
    }
  }
}

bool AXNode::IsView() const {
  const AXTreeManager* manager = GetManager();
  if (!manager) {
    return false;
  }
  return manager->IsView();
}

AXNode* AXNode::ComputeLastUnignoredChildRecursive() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (children().empty())
    return nullptr;

  for (int i = static_cast<int>(children().size()) - 1; i >= 0; --i) {
    AXNode* child = children_[i];
    if (!child->IsIgnored())
      return child;

    AXNode* descendant = child->ComputeLastUnignoredChildRecursive();
    if (descendant)
      return descendant;
  }
  return nullptr;
}

AXNode* AXNode::ComputeFirstUnignoredChildRecursive() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  for (size_t i = 0; i < children().size(); i++) {
    AXNode* child = children_[i];
    if (!child->IsIgnored())
      return child;

    AXNode* descendant = child->ComputeFirstUnignoredChildRecursive();
    if (descendant)
      return descendant;
  }
  return nullptr;
}

std::string AXNode::GetTextForRangeValue() const {
  DCHECK(data().IsRangeValueSupported());
  std::string range_value =
      GetStringAttribute(ax::mojom::StringAttribute::kValue);
  if (range_value.empty()) {
    float numeric_value =
        GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange);
    if (numeric_value != AXNode::kDefaultFloatValue ||
        HasFloatAttribute(ax::mojom::FloatAttribute::kValueForRange)) {
      // This method of number to string conversion creates a localized string
      // and avoids padding with extra zeros after the decimal point.
      // For example, 3.5 is converted to "3.5" rather than "3.50000".
      return base::StringPrintf("%g", numeric_value);
    }
  }
  return range_value;
}

std::string AXNode::GetValueForColorWell() const {
  DCHECK_EQ(GetRole(), ax::mojom::Role::kColorWell);
  // static cast because SkColor is a 4-byte unsigned int
  unsigned int color = static_cast<unsigned int>(
      GetIntAttribute(ax::mojom::IntAttribute::kColorValue));

  unsigned int red = SkColorGetR(color);
  unsigned int green = SkColorGetG(color);
  unsigned int blue = SkColorGetB(color);
  return base::StringPrintf("%d%% red %d%% green %d%% blue", red * 100 / 255,
                            green * 100 / 255, blue * 100 / 255);
}

bool AXNode::IsIgnored() const {
  // If the focus has moved, then it could make a previously ignored node
  // unignored or vice versa. We never ignore focused nodes otherwise users of
  // assistive software might be unable to interact with the webpage.
  return AXTree::ComputeNodeIsIgnored(&tree_->data(), data());
}

bool AXNode::IsIgnoredForTextNavigation() const {
  // Splitters do not contribute anything to the tree's text representation, so
  // stopping on a splitter would erroniously appear to a screen reader user
  // that the cursor has stopped on the next unignored object.
  if (GetRole() == ax::mojom::Role::kSplitter)
    return true;

  // A generic container without any unignored children that is not editable
  // should not be used for text-based navigation. Such nodes don't make sense
  // for screen readers to land on, since no role / text will be announced and
  // no action is possible.
  if (GetRole() == ax::mojom::Role::kGenericContainer &&
      !GetUnignoredChildCount() && !HasState(ax::mojom::State::kEditable)) {
    return true;
  }

  return false;
}

bool AXNode::IsInvisibleOrIgnored() const {
  return id() != tree_->data().focus_id && (IsIgnored() || data_.IsInvisible());
}

bool AXNode::IsChildOfLeaf() const {
  // TODO(nektar): Cache this state in `AXComputedNodeData`.
  for (const AXNode* ancestor = GetUnignoredParent(); ancestor;
       ancestor = ancestor->GetUnignoredParent()) {
    if (ancestor->IsLeaf()) {
      return true;
    }
  }
  return false;
}

bool AXNode::IsEmptyLeaf() const {
  if (!IsLeaf())
    return false;
  if (GetUnignoredChildCountCrossingTreeBoundary())
    return !GetTextContentLengthUTF8();
  // Text exposed by ignored leaf (text) nodes is not exposed to the platforms'
  // accessibility layer, hence such leaf nodes are in effect empty.
  return IsIgnored() || !GetTextContentLengthUTF8();
}

bool AXNode::IsLeaf() const {
  // A node is a leaf if it has no descendants, i.e. if it is at the bottom of
  // the tree, regardless whether it is ignored or not.
  if (!GetChildCountCrossingTreeBoundary())
    return true;

  // Ignored nodes with any kind of descendants, (ignored or unignored), cannot
  // be leaves because: A) If some of their descendants are unignored then those
  // descendants need to be exposed to the platform layer, and B) If all of
  // their descendants are ignored they cannot be at the bottom of the platform
  // tree since that tree does not expose any ignored objects.
  if (IsIgnored())
    return false;

  // An unignored node is a leaf if all of its descendants are ignored.
  int child_count = GetUnignoredChildCountCrossingTreeBoundary();
  if (!child_count)
    return true;

#if BUILDFLAG(IS_WIN)
  // On Windows, we want to hide the subtree of a collapsed <select> element.
  // Otherwise, ATs are always going to announce its options whether it's
  // collapsed or expanded. In the AXTree, this element corresponds to a node
  // with role ax::mojom::Role::kComboBoxSelect that is the parent of a node
  // with // role ax::mojom::Role::kMenuListPopup.
  if (IsCollapsedMenuListSelect())
    return true;
#endif  // BUILDFLAG(IS_WIN)

  // These types of objects may have children that we use as internal
  // implementation details, but we want to expose them as leaves to platform
  // accessibility APIs because screen readers might be confused if they find
  // any children.
  if (data().IsAtomicTextField() || IsText())
    return true;

  // Non atomic text fields may have children that we want to expose.
  // For example, a <div contenteditable> may have child elements such as
  // more <div>s that we want to expose.
  if (data().IsNonAtomicTextField())
    return false;

  // Roles whose children are only presentational according to the ARIA and
  // HTML5 Specs should be hidden from screen readers.
  switch (GetRole()) {
    // According to the ARIA and Core-AAM specs:
    // https://w3c.github.io/aria/#button,
    // https://www.w3.org/TR/core-aam-1.1/#exclude_elements
    // buttons' children are presentational only and should be hidden from
    // screen readers. However, we cannot enforce the leafiness of buttons
    // because they may contain many rich, interactive descendants such as a day
    // in a calendar, and screen readers will need to interact with these
    // contents. See https://crbug.com/689204.
    // So we decided to not enforce the leafiness of buttons and expose all
    // children.
    case ax::mojom::Role::kButton:
      return false;
    case ax::mojom::Role::kImage: {
      // HTML images (i.e. <img> elements) are not leaves when they are image
      // maps. Therefore, do not truncate descendants except in the case where
      // ARIA role=img or role=image because that's how we want to treat
      // ARIA-based images.
      const std::string role =
          GetStringAttribute(ax::mojom::StringAttribute::kRole);
      return role == "img" || role == "image";
    }
    case ax::mojom::Role::kDocCover:
    case ax::mojom::Role::kGraphicsSymbol:
    case ax::mojom::Role::kMeter:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSpinButton:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kProgressIndicator:
      return true;
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kListBoxOption:
    // role="math" is flat. But always return false for kMathMLMath since the
    // children of a <math> tag should be exposed to make MathML accessible.
    case ax::mojom::Role::kMath:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kPopUpButton:
    case ax::mojom::Role::kToggleButton:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTab: {
      // For historical reasons, truncate the children of these roles when they
      // have a single text child and are not editable.
      // TODO(accessibility) Consider removing this in the future, and exposing
      // all descendants, as it seems ATs do a good job of avoiding redundant
      // speech even if they have a text child. Removing this rule would allow
      // AT users to select any text visible in the page, and ensure that all
      // text is available to ATs that use the position of objects on the
      // screen. This has been manually tested in JAWS, NVDA, VoiceOver, Orca
      // and ChromeVox.
      // Note that the ARIA spec says, "User agents SHOULD NOT expose
      // descendants of this element through the platform accessibility API. If
      // user agents do not hide the descendant nodes, some information may be
      // read twice." However, this is not a MUST, and in non-simple cases
      // Chrome and Firefox already expose descendants, without causing issues.
      // Allow up to 2 text nodes so that list items with bullets are leaves.
      if (child_count > 2 || HasState(ax::mojom::State::kEditable))
        return false;
      const AXNode* child1 = GetFirstUnignoredChildCrossingTreeBoundary();
      if (!child1 || !child1->IsText())
        return false;
      const AXNode* child2 = child1->GetNextSibling();
      return !child2 || child2->IsText();
    }
    default:
      return false;
  }
}

bool AXNode::IsFocusable() const {
  return HasState(ax::mojom::State::kFocusable) ||
         IsLikelyARIAActiveDescendant();
}

bool AXNode::IsLikelyARIAActiveDescendant() const {
  // Should be menu item, option, etc.
  if (!ui::IsLikelyActiveDescendantRole(GetRole()))
    return false;

  // False if invisible, ignored or disabled.
  if (IsInvisibleOrIgnored() ||
      GetIntAttribute(ax::mojom::IntAttribute::kRestriction) ==
          static_cast<int>(ax::mojom::Restriction::kDisabled)) {
    return false;
  }

  // False if no ARIA role -- not a perfect rule, but a reasonable heuristic.
  if (!HasStringAttribute(ax::mojom::StringAttribute::kRole))
    return false;

  // False if no id attribute -- nothing to point to.
  // This requirement may need to be removed if ARIA element reflection is
  // implemented. HTML attribute serialization must currently be turned on in
  // order to pass this requirement.
  if (!HasStringAttribute(ax::mojom::StringAttribute::kHtmlId)) {
    return false;
  }

  // Finally, check for the required ancestor.
  for (AXNode* ancestor_node = GetUnignoredParent(); ancestor_node;
       ancestor_node = ancestor_node->GetUnignoredParent()) {
    // Check for an ancestor with aria-activedescendant.
    if (ancestor_node->HasIntAttribute(
            ax::mojom::IntAttribute::kActivedescendantId)) {
      return true;
    }
    // Check for an ancestor listbox/tree/grid/treegrid/dialog that is
    // controlled by a textfield combobox that also has an
    // aria-activedescendant. Note: blink will map aria-owns to aria-controls in
    // the textfield combobox case as it was the older technique, but treating
    // as an actual aria-owns makes no sense as a textfield cannot have
    // children.
    if (ui::IsComboBoxContainer(ancestor_node->GetRole())) {
      std::set<AXNodeID> nodes_that_control_this_list =
          tree()->GetReverseRelations(ax::mojom::IntListAttribute::kControlsIds,
                                      ancestor_node->id());
      for (AXNodeID id : nodes_that_control_this_list) {
        if (AXNode* node = tree()->GetFromId(id)) {
          if (ui::IsTextField(node->GetRole())) {
            return node->HasIntAttribute(
                ax::mojom::IntAttribute::kActivedescendantId);
          }
        }
      }
    }
    // TODO(aleventhal) Re-add this once Google Slides no longer needs
    // special hack where the aria-activedescendant is on a containing
    // contenteditable, which is currently done in the slides thumb strip for
    // copy/paste reasons. See matching code in AXPlatformNode win which clears
    // IA2_STATE_EDITABLE for this case, but requires the descendant tree items
    // to have the FOCUSABLE state. See also the related dump tree test
    // aria-focusable-subwidget-not-editable.html.
    // (IsContainerWithSelectableChildren(ancestor_node->GetRole())) {
    //   // No need to check more ancestors.
    //   break;
    // }
  }

  return false;
}

bool AXNode::IsInListMarker() const {
  if (GetRole() == ax::mojom::Role::kListMarker)
    return true;

  // The children of a list marker node can only be text nodes.
  if (!IsText())
    return false;

  // There is no need to iterate over all the ancestors of the current node
  // since a list marker has descendants that are only 2 levels deep, i.e.:
  // AXLayoutObject role=kListMarker
  // ++StaticText
  // ++++InlineTextBox
  AXNode* parent_node = GetUnignoredParent();
  if (!parent_node)
    return false;

  if (parent_node->GetRole() == ax::mojom::Role::kListMarker)
    return true;

  AXNode* grandparent_node = parent_node->GetUnignoredParent();
  return grandparent_node &&
         grandparent_node->GetRole() == ax::mojom::Role::kListMarker;
}

bool AXNode::IsCollapsedMenuListSelect() const {
  return HasState(ax::mojom::State::kCollapsed) &&
         GetRole() == ax::mojom::Role::kComboBoxSelect;
}

bool AXNode::IsRootWebAreaForPresentationalIframe() const {
  if (!ui::IsPlatformDocument(GetRole()))
    return false;
  const AXNode* parent = GetUnignoredParentCrossingTreeBoundary();
  if (!parent)
    return false;
  return parent->GetRole() == ax::mojom::Role::kIframePresentational;
}

AXNode* AXNode::GetCollapsedMenuListSelectAncestor() const {
  AXNode* node = GetOrderedSet();

  if (!node)
    return nullptr;

  // The ordered set returned is either the popup element child of the select
  // combobox or the select combobox itself. We need |node| to point to the
  // select combobox.
  if (node->GetRole() != ax::mojom::Role::kComboBoxSelect) {
    node = node->GetParent();
    if (!node)
      return nullptr;
  }

  return node->IsCollapsedMenuListSelect() ? node : nullptr;
}

bool AXNode::IsEmbeddedGroup() const {
  if (GetRole() != ax::mojom::Role::kGroup || !GetUnignoredParent()) {
    return false;
  }

  return ui::IsSetLike(GetUnignoredParent()->GetRole());
}

AXNode* AXNode::GetLowestPlatformAncestor() const {
  AXNode* current_node = const_cast<AXNode*>(this);
  AXNode* lowest_unignored_node = current_node;
  for (; lowest_unignored_node && lowest_unignored_node->IsIgnored();
       lowest_unignored_node = lowest_unignored_node->GetParent()) {
  }

  // `highest_leaf_node` could be nullptr.
  AXNode* highest_leaf_node = lowest_unignored_node;
  // For the purposes of this method, a leaf node does not include leaves in the
  // internal accessibility tree, only in the platform exposed tree.
  for (AXNode* ancestor_node = lowest_unignored_node; ancestor_node;
       ancestor_node = ancestor_node->GetUnignoredParent()) {
    if (ancestor_node->IsLeaf())
      highest_leaf_node = ancestor_node;
  }
  if (highest_leaf_node)
    return highest_leaf_node;

  if (lowest_unignored_node)
    return lowest_unignored_node;
  return current_node;
}

AXNode* AXNode::GetTextFieldAncestor() const {
  // The descendants of a text field usually have State::kEditable, however in
  // the case of Role::kSearchBox or Role::kSpinButton being the text field
  // ancestor, its immediate descendant can have Role::kGenericContainer without
  // State::kEditable. Same with inline text boxes and placeholder text.
  // TODO(nektar): Fix all such inconsistencies in Blink.
  //
  // Also, ARIA text and search boxes may not have the contenteditable attribute
  // set, but they should still be treated the same as all other text fields.
  // (See `AXNodeData::IsAtomicTextField()` for more details.)
  for (AXNode* ancestor = const_cast<AXNode*>(this); ancestor;
       ancestor = ancestor->GetUnignoredParent()) {
    if (ancestor->data().IsTextField())
      return ancestor;
  }
  return nullptr;
}

AXNode* AXNode::GetTextFieldInnerEditorElement() const {
  if (!data().IsAtomicTextField() || !GetUnignoredChildCount())
    return nullptr;

  // Text fields wrap their static text and inline text boxes in generic
  // containers, and some, like <input type="search">, wrap the wrapper as well.
  // There are several incarnations of this structure.
  // 1. An empty atomic text field:
  // -- Generic container <-- there can be any number of these in a chain.
  //    However, some empty text fields have the below structure, with empty
  //    text boxes.
  // 2. A single line, an atomic text field with some text in it:
  // -- Generic container <-- there can be any number of these in a chain.
  // ---- Static text
  // ------ Inline text box children (zero or more)
  // ---- Line Break (optional,  a placeholder break element if the text data
  //                    ends with '\n' or '\r')
  // 3. A multiline textarea with some text in it:
  //    Similar to #2, but can repeat the static text, line break children
  //    multiple times.

  AXNode* text_container = GetDeepestFirstUnignoredDescendant();
  DCHECK(text_container) << "Unable to retrieve deepest unignored child on\n"
                         << *this;
  // Non-empty text fields expose a set of static text objects with one or more
  // inline text boxes each. On some platforms, such as Android, we don't enable
  // inline text boxes, and only the static text objects are exposed.
  if (text_container->GetRole() == ax::mojom::Role::kInlineTextBox)
    text_container = text_container->GetUnignoredParent();

  // Get the parent of the static text or the line break, if any; a line break
  // is possible when the field contains a line break as its first character.
  if (text_container->GetRole() == ax::mojom::Role::kStaticText ||
      text_container->GetRole() == ax::mojom::Role::kLineBreak) {
    text_container = text_container->GetUnignoredParent();
  }

  DCHECK(text_container) << "Unexpected unignored parent while computing text "
                            "field inner editor element on\n"
                         << *this;
  if (text_container->GetRole() == ax::mojom::Role::kGenericContainer)
    return text_container;
  return nullptr;
}

AXNode* AXNode::GetSelectionContainer() const {
  // Avoid walking ancestors if the role cannot support the selectable state.
  if (!IsSelectSupported(GetRole()))
    return nullptr;
  if (IsInvisibleOrIgnored() ||
      GetIntAttribute(ax::mojom::IntAttribute::kRestriction) ==
          static_cast<int>(ax::mojom::Restriction::kDisabled)) {
    return nullptr;
  }
  for (AXNode* ancestor = const_cast<AXNode*>(this); ancestor;
       ancestor = ancestor->GetUnignoredParent()) {
    if (ui::IsContainerWithSelectableChildren(ancestor->GetRole()))
      return ancestor;
  }
  return nullptr;
}

AXNode* AXNode::GetTableAncestor() const {
  for (AXNode* ancestor = const_cast<AXNode*>(this); ancestor;
       ancestor = ancestor->GetUnignoredParent()) {
    if (ancestor->IsTable())
      return ancestor;
  }
  return nullptr;
}

bool AXNode::IsDescendantOfAtomicTextField() const {
  AXNode* text_field_node = GetTextFieldAncestor();
  return text_field_node && text_field_node->data().IsAtomicTextField();
}

}  // namespace ui
