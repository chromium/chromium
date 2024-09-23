// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_TREE_NODE_MODEL_H_
#define UI_BASE_MODELS_TREE_NODE_MODEL_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "ui/base/models/tree_model.h"

namespace ui {

// TreeNodeModel and TreeNodes provide an implementation of TreeModel around
// TreeNodes.
//
// TreeNodes own their children, so that deleting a node deletes all
// descendants.
//
// TreeNodes do NOT maintain a pointer back to the model. As such, if you
// are using TreeNodes with a TreeNodeModel you will need to notify the observer
// yourself any time you make any change directly to the TreeNodes. For example,
// if you directly invoke set_title on a node it does not notify the observer,
// you will need to do it yourself. This includes the following methods: Add,
// Remove and set_title. TreeNodeModel provides cover methods that mutate the
// TreeNodes and notify the observer. If you are using TreeNodes with a
// TreeNodeModel use the cover methods to save yourself the headache.
//
// The following example creates a TreeNode with two children and then
// creates a TreeNodeModel from it:
//
// std::unique_ptr<TreeNodeWithValue<int>> root =
//     std::make_unique<TreeNodeWithValue<int>>();
// root->Add(
//     std::make_unique<TreeNodeWithValue<int>>(u"child 1", 0));
// root->Add(
//     std::make_unique<TreeNodeWithValue<int>>(u"child 2", 1));
// TreeNodeModel<TreeNodeWithValue<int>> model(std::move(root));
//
// Two variants of TreeNode are provided here:
//
// . TreeNode itself is intended for subclassing. It has one type parameter
//   that corresponds to the type of the node. When subclassing use your class
//   name as the type parameter, e.g.:
//   class MyTreeNode : public TreeNode<MyTreeNode> .
// . TreeNodeWithValue is a trivial subclass of TreeNode that has one type
//   type parameter: a value type that is associated with the node.
//
// Which you use depends upon the situation. If you want to subclass and add
// methods, then use TreeNode. If you don't need any extra methods and just
// want to associate a value with each node, then use TreeNodeWithValue.
//
// Regardless of which TreeNode you use, if you are using the nodes with a
// TreeView take care to notify the observer when mutating the nodes.

// TreeNode -------------------------------------------------------------------

// See above for documentation. Example:
//
//   class MyNode : public ui::TreeNode<MyNode> {
//     ...<custom class logic>...
//   };
//   using MyModel = ui::TreeNodeModel<MyNode>;
template <class NodeType>
class TreeNode : public TreeModelNode {
 public:
  using TreeNodes = std::vector<std::unique_ptr<NodeType>>;

  TreeNode() : parent_(nullptr) {}

  explicit TreeNode(const std::u16string& title)
      : title_(title), parent_(nullptr) {}

  TreeNode(const TreeNode&) = delete;
  TreeNode& operator=(const TreeNode&) = delete;

  ~TreeNode() override {}

  // Adds |node| as a child of this node, at |index|. Returns a raw pointer to
  // the node.
  NodeType* Add(std::unique_ptr<NodeType> node, size_t index) {
    DCHECK(node);
    DCHECK_LE(index, children_.size());
    DCHECK(!node->parent_);
    node->parent_ = static_cast<NodeType*>(this);
    NodeType* node_ptr = node.get();
    children_.insert(children_.begin() + static_cast<ptrdiff_t>(index),
                     std::move(node));
    return node_ptr;
  }

  // Shorthand for "add at end".
  NodeType* Add(std::unique_ptr<NodeType> node) {
    return Add(std::move(node), children_.size());
  }

  // Removes the node at the given index. Returns the removed node.
  std::unique_ptr<NodeType> Remove(size_t index) {
    DCHECK_LT(index, children_.size());
    children_[index]->parent_ = nullptr;
    std::unique_ptr<NodeType> ptr = std::move(children_[index]);
    children_.erase(children_.begin() + static_cast<ptrdiff_t>(index));
    return ptr;
  }

  // Removes all the children from this node.
  void DeleteAll() { children_.clear(); }

  // Returns the parent node, or nullptr if this is the root node.
  const NodeType* parent() const { return parent_; }
  NodeType* parent() { return parent_; }

  // Returns true if this is the root node.
  bool is_root() const { return parent_ == nullptr; }

  const TreeNodes& children() const { return children_; }

  // Returns the number of all nodes in the subtree rooted at this node,
  // including this node.
  size_t GetTotalNodeCount() const {
    size_t count = 1;  // Start with one to include the node itself.
    for (const auto& child : children_)
      count += child->GetTotalNodeCount();
    return count;
  }

  // Returns the index of |node|, or nullopt if |node| is not a child of this.
  std::optional<size_t> GetIndexOf(const NodeType* node) const {
    DCHECK(node);
    const auto i =
        base::ranges::find(children_, node, &std::unique_ptr<NodeType>::get);
    return i != children_.end()
               ? std::make_optional(static_cast<size_t>(i - children_.begin()))
               : std::nullopt;
  }

  // Sets the title of the node.
  virtual void SetTitle(const std::u16string& title) { title_ = title; }

  // TreeModelNode:
  const std::u16string& GetTitle() const override { return title_; }

  const std::u16string& GetAccessibleTitle() const override {
    return title_.empty() ? placeholder_accessible_title_ : title_;
  }

  void SetPlaceholderAccessibleTitle(
      std::u16string placeholder_accessible_title) {
    placeholder_accessible_title_ = placeholder_accessible_title;
  }

  // Returns true if this == ancestor, or one of this nodes parents is
  // ancestor.
  bool HasAncestor(const NodeType* ancestor) const {
    if (ancestor == this)
      return true;
    if (!ancestor)
      return false;
    return parent_ ? parent_->HasAncestor(ancestor) : false;
  }

  // Reorders children according to a new arbitrary order. |new_order| must
  // contain one entry per child node, and the value of the entry at position
  // |i| represents the new position, which must be unique and in the range
  // between 0 (inclusive) and the number of children (exclusive).
  void ReorderChildren(const std::vector<size_t>& new_order) {
    const size_t children_count = children_.size();
    DCHECK_EQ(children_count, new_order.size());
    DCHECK_EQ(children_count,
              std::set(new_order.begin(), new_order.end()).size());
    TreeNodes new_children(children_count);
    for (size_t old_index = 0; old_index < children_count; ++old_index) {
      const size_t new_index = new_order[old_index];
      DCHECK_LT(new_index, children_count);
      DCHECK(children_[old_index]);
      DCHECK(!new_children[new_index]);
      new_children[new_index] = std::move(children_[old_index]);
    }
    children_ = std::move(new_children);
  }

  // Sorts children according to a comparator.
  template <typename Compare>
  void SortChildren(Compare comp) {
    std::stable_sort(children_.begin(), children_.end(), comp);
  }

 private:
  // Title displayed in the tree.
  std::u16string title_;

  // If set, a placeholder accessible title to fall back to if there is no
  // title.
  std::u16string placeholder_accessible_title_;

  // This node's parent.
  raw_ptr<NodeType> parent_;

  // This node's children.
  TreeNodes children_;
};

// TreeNodeWithValue ----------------------------------------------------------

// See top of file for documentation. Example:
//
//  using MyNode = ui::TreeNodeWithValue<MyData>;
//  using MyModel = ui::TreeNodeModel<MyNode>;
template <class ValueType>
class TreeNodeWithValue : public TreeNode<TreeNodeWithValue<ValueType>> {
 public:
  TreeNodeWithValue() {}

  explicit TreeNodeWithValue(const ValueType& value)
      : ParentType(std::u16string()), value(value) {}

  TreeNodeWithValue(const std::u16string& title, const ValueType& value)
      : ParentType(title), value(value) {}

  TreeNodeWithValue(const TreeNodeWithValue&) = delete;
  TreeNodeWithValue& operator=(const TreeNodeWithValue&) = delete;

  ValueType value;

 private:
  using ParentType = TreeNode<TreeNodeWithValue<ValueType>>;
};

// TreeNodeModel --------------------------------------------------------------

// TreeModel implementation intended to be used with TreeNodes.
template <class NodeType>
class TreeNodeModel : public TreeModel {
 public:
  // Creates a TreeNodeModel with the specified root node.
  explicit TreeNodeModel(std::unique_ptr<NodeType> root)
      : root_(std::move(root)) {}

  TreeNodeModel(const TreeNodeModel&) = delete;
  TreeNodeModel& operator=(const TreeNodeModel&) = delete;

  virtual ~TreeNodeModel() override {}

  static NodeType* AsNode(TreeModelNode* model_node) {
    return static_cast<NodeType*>(model_node);
  }
  static const NodeType* AsNode(const TreeModelNode* model_node) {
    return static_cast<const NodeType*>(model_node);
  }

  NodeType* Add(NodeType* parent,
                std::unique_ptr<NodeType> node,
                size_t index) {
    DCHECK(parent);
    DCHECK(node);
    NodeType* node_ptr = parent->Add(std::move(node), index);
    NotifyObserverTreeNodeAdded(parent, index);
    return node_ptr;
  }

  // Shorthand for "add at end".
  NodeType* Add(NodeType* parent, std::unique_ptr<NodeType> node) {
    return Add(parent, std::move(node), parent->children().size());
  }

  std::unique_ptr<NodeType> Remove(NodeType* parent, size_t index) {
    DCHECK(parent);
    std::unique_ptr<NodeType> owned_node = parent->Remove(index);
    NotifyObserverTreeNodeRemoved(parent, index);
    return owned_node;
  }

  std::unique_ptr<NodeType> Remove(NodeType* parent, NodeType* node) {
    DCHECK(parent);
    return Remove(parent, parent->GetIndexOf(node).value());
  }

  void NotifyObserverTreeNodeAdded(NodeType* parent, size_t index) {
    observer_list_.Notify(&TreeModelObserver::TreeNodeAdded, this, parent,
                          index);
  }

  void NotifyObserverTreeNodeRemoved(NodeType* parent, size_t index) {
    observer_list_.Notify(&TreeModelObserver::TreeNodeRemoved, this, parent,
                          index);
  }

  void NotifyObserverTreeNodeChanged(TreeModelNode* node) {
    observer_list_.Notify(&TreeModelObserver::TreeNodeChanged, this, node);
  }

  // TreeModel:

  // C++ allows one to override a base class' virtual function with one that
  // returns a different type, as long as that type implements the base class'
  // return type. This is convenient because it allows callers with references
  // to the specific TreeNodeModel to get the proper return type without
  // casting.
  //
  // However, this does require that the NodeType be defined when this is
  // parsed (normally one could forward define this).
  NodeType* GetRoot() override {
    return root_.get();
  }

  Nodes GetChildren(const TreeModelNode* parent) const override {
    DCHECK(parent);
    const auto& children = AsNode(parent)->children();
    Nodes nodes;
    nodes.reserve(children.size());
    base::ranges::transform(children, std::back_inserter(nodes),
                            &TreeNode<NodeType>::TreeNodes::value_type::get);
    return nodes;
  }

  std::optional<size_t> GetIndexOf(TreeModelNode* parent,
                                   TreeModelNode* child) const override {
    DCHECK(parent);
    return AsNode(parent)->GetIndexOf(AsNode(child));
  }

  TreeModelNode* GetParent(TreeModelNode* node) const override {
    DCHECK(node);
    return AsNode(node)->parent();
  }

  void AddObserver(TreeModelObserver* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(TreeModelObserver* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void SetTitle(TreeModelNode* node, const std::u16string& title) override {
    DCHECK(node);
    AsNode(node)->SetTitle(title);
    NotifyObserverTreeNodeChanged(node);
  }

 private:
  // The observers.
  base::ObserverList<TreeModelObserver>::Unchecked observer_list_;

  // The root.
  std::unique_ptr<NodeType> root_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_TREE_NODE_MODEL_H_
