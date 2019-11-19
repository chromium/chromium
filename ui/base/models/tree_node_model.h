// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_TREE_NODE_MODEL_H_
#define UI_BASE_MODELS_TREE_NODE_MODEL_H_

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "ui/base/models/tree_model.h"

namespace bookmarks {
class BookmarkModel;
}

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
//     std::make_unique<TreeNodeWithValue<int>>(ASCIIToUTF16("child 1"), 0));
// root->Add(
//     std::make_unique<TreeNodeWithValue<int>>(ASCIIToUTF16("child 2"), 1));
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

  explicit TreeNode(const base::string16& title)
      : title_(title), parent_(nullptr) {}

  ~TreeNode() override {}

  // Adds |node| as a child of this node, at |index|. Returns a raw pointer to
  // the node.
  NodeType* Add(std::unique_ptr<NodeType> node, size_t index) {
    DCHECK(node);
    DCHECK_LE(index, children_.size());
    DCHECK(!node->parent_);
    node->parent_ = static_cast<NodeType*>(this);
    NodeType* node_ptr = node.get();
    children_.insert(children_.begin() + index, std::move(node));
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
    children_.erase(children_.begin() + index);
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
  int GetTotalNodeCount() const {
    int count = 1;  // Start with one to include the node itself.
    for (const auto& child : children_)
      count += child->GetTotalNodeCount();
    return count;
  }

  // Returns the index of |node|, or -1 if |node| is not a child of this.
  int GetIndexOf(const NodeType* node) const {
    DCHECK(node);
    auto i = std::find_if(children_.begin(), children_.end(),
                          [node](const std::unique_ptr<NodeType>& ptr) {
                            return ptr.get() == node;
                          });
    return i != children_.end() ? static_cast<int>(i - children_.begin()) : -1;
  }

  // Sets the title of the node.
  virtual void SetTitle(const base::string16& title) { title_ = title; }

  // TreeModelNode:
  const base::string16& GetTitle() const override { return title_; }

  // Returns true if this == ancestor, or one of this nodes parents is
  // ancestor.
  bool HasAncestor(const NodeType* ancestor) const {
    if (ancestor == this)
      return true;
    if (!ancestor)
      return false;
    return parent_ ? parent_->HasAncestor(ancestor) : false;
  }

 private:
  // TODO(https://crbug.com/956314): Remove this.
  friend class bookmarks::BookmarkModel;

  // Title displayed in the tree.
  base::string16 title_;

  // This node's parent.
  NodeType* parent_;

  // This node's children.
  TreeNodes children_;

  DISALLOW_COPY_AND_ASSIGN(TreeNode);
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
      : ParentType(base::string16()), value(value) {}

  TreeNodeWithValue(const base::string16& title, const ValueType& value)
      : ParentType(title), value(value) {}

  ValueType value;

 private:
  using ParentType = TreeNode<TreeNodeWithValue<ValueType>>;

  DISALLOW_COPY_AND_ASSIGN(TreeNodeWithValue);
};

// TreeNodeModel --------------------------------------------------------------

// TreeModel implementation intended to be used with TreeNodes.
template <class NodeType>
class TreeNodeModel : public TreeModel {
 public:
  // Creates a TreeNodeModel with the specified root node.
  explicit TreeNodeModel(std::unique_ptr<NodeType> root)
      : root_(std::move(root)) {}
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
    NotifyObserverTreeNodesAdded(parent, index, 1);
    return node_ptr;
  }

  // Shorthand for "add at end".
  NodeType* Add(NodeType* parent, std::unique_ptr<NodeType> node) {
    return Add(parent, std::move(node), parent->children().size());
  }

  std::unique_ptr<NodeType> Remove(NodeType* parent, size_t index) {
    DCHECK(parent);
    std::unique_ptr<NodeType> owned_node = parent->Remove(index);
    NotifyObserverTreeNodesRemoved(parent, index, 1);
    return owned_node;
  }

  std::unique_ptr<NodeType> Remove(NodeType* parent, NodeType* node) {
    DCHECK(parent);
    return Remove(parent, size_t{parent->GetIndexOf(node)});
  }

  void NotifyObserverTreeNodesAdded(NodeType* parent,
                                    size_t start,
                                    size_t count) {
    for (TreeModelObserver& observer : observer_list_)
      observer.TreeNodesAdded(this, parent, start, count);
  }

  void NotifyObserverTreeNodesRemoved(NodeType* parent,
                                      size_t start,
                                      size_t count) {
    for (TreeModelObserver& observer : observer_list_)
      observer.TreeNodesRemoved(this, parent, start, count);
  }

  void NotifyObserverTreeNodeChanged(TreeModelNode* node) {
    for (TreeModelObserver& observer : observer_list_)
      observer.TreeNodeChanged(this, node);
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
    std::transform(children.cbegin(), children.cend(),
                   std::back_inserter(nodes),
                   [](const auto& child) { return child.get(); });
    return nodes;
  }

  int GetIndexOf(TreeModelNode* parent, TreeModelNode* child) const override {
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

  void SetTitle(TreeModelNode* node,
                const base::string16& title) override {
    DCHECK(node);
    AsNode(node)->SetTitle(title);
    NotifyObserverTreeNodeChanged(node);
  }

 private:
  // The observers.
  base::ObserverList<TreeModelObserver>::Unchecked observer_list_;

  // The root.
  std::unique_ptr<NodeType> root_;

  DISALLOW_COPY_AND_ASSIGN(TreeNodeModel);
};

}  // namespace ui

#endif  // UI_BASE_MODELS_TREE_NODE_MODEL_H_
