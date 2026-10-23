// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_TREE_MODEL_H_
#define UI_BASE_MODELS_TREE_MODEL_H_

#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"

namespace ui {

class TreeModel;

// TreeModelNode --------------------------------------------------------------

class COMPONENT_EXPORT(UI_BASE) TreeModelNode {
 public:
  using TreeNodes = std::vector<std::unique_ptr<TreeModelNode>>;

  TreeModelNode();
  explicit TreeModelNode(const std::u16string& title);
  TreeModelNode(const TreeModelNode&) = delete;
  TreeModelNode& operator=(const TreeModelNode&) = delete;
  virtual ~TreeModelNode();

  template <typename NodeType = TreeModelNode>
    requires std::derived_from<NodeType, TreeModelNode>
  NodeType* Add(std::unique_ptr<NodeType> node, size_t index) {
    NodeType* ptr = node.get();
    AddImpl(std::move(node), index);
    return ptr;
  }

  template <typename NodeType = TreeModelNode>
    requires std::derived_from<NodeType, TreeModelNode>
  NodeType* Add(std::unique_ptr<NodeType> node) {
    return Add(std::move(node), children_.size());
  }

  std::unique_ptr<TreeModelNode> Remove(size_t index);

  void DeleteAll();

  const TreeModelNode* parent() const { return parent_; }
  TreeModelNode* parent() { return parent_; }
  bool is_root() const { return parent_ == nullptr; }
  const TreeNodes& children() const { return children_; }

  size_t GetTotalNodeCount() const;
  std::optional<size_t> GetIndexOf(const TreeModelNode* node) const;
  bool HasAncestor(const TreeModelNode* ancestor) const;

  void SetTitle(const std::u16string& title);

  // Returns the title for the node.
  const std::u16string& GetTitle() const;

  // Returns the accessible title for the node.
  const std::u16string& GetAccessibleTitle() const;

  void SetPlaceholderAccessibleTitle(
      std::u16string placeholder_accessible_title);

 private:
  void AddImpl(std::unique_ptr<TreeModelNode> node, size_t index);

  std::u16string title_;
  std::u16string placeholder_accessible_title_;
  raw_ptr<TreeModelNode> parent_ = nullptr;
  TreeNodes children_;
};

// Observer for the TreeModel. Notified of significant events to the model.
class COMPONENT_EXPORT(UI_BASE) TreeModelObserver {
 public:
  // Notification that nodes were added to the specified parent.
  virtual void TreeNodeAdded(TreeModel* model,
                             TreeModelNode* parent,
                             size_t index) {}

  // Notification that nodes were removed from the specified parent.
  virtual void TreeNodeRemoved(TreeModel* model,
                               TreeModelNode* parent,
                               size_t index) {}

  // Notification that the contents of a node has changed.
  virtual void TreeNodeChanged(TreeModel* model, TreeModelNode* node) {}

 protected:
  virtual ~TreeModelObserver() {}
};

// TreeModel ------------------------------------------------------------------

class COMPONENT_EXPORT(UI_BASE) TreeModel {
 public:
  using Nodes = std::vector<TreeModelNode*>;

  explicit TreeModel(std::unique_ptr<TreeModelNode> root);
  TreeModel(const TreeModel&) = delete;
  TreeModel& operator=(const TreeModel&) = delete;
  virtual ~TreeModel();

  // Returns the root of the tree. This may or may not be shown in the tree,
  // see SetRootShown for details.
  TreeModelNode* GetRoot();
  const TreeModelNode* GetRoot() const;

  template <typename NodeType = TreeModelNode>
    requires std::derived_from<NodeType, TreeModelNode>
  NodeType* Add(TreeModelNode* parent,
                std::unique_ptr<NodeType> node,
                size_t index) {
    NodeType* ptr = node.get();
    AddImpl(parent, std::move(node), index);
    return ptr;
  }

  template <typename NodeType = TreeModelNode>
    requires std::derived_from<NodeType, TreeModelNode>
  NodeType* Add(TreeModelNode* parent, std::unique_ptr<NodeType> node) {
    return Add(parent, std::move(node), parent->children().size());
  }

  std::unique_ptr<TreeModelNode> Remove(TreeModelNode* parent, size_t index);
  std::unique_ptr<TreeModelNode> Remove(TreeModelNode* parent,
                                        TreeModelNode* node);

  void NotifyObserverTreeNodeAdded(TreeModelNode* parent, size_t index);
  void NotifyObserverTreeNodeRemoved(TreeModelNode* parent, size_t index);
  void NotifyObserverTreeNodeChanged(TreeModelNode* node);

  // Returns the children of |parent|.
  Nodes GetChildren(const TreeModelNode* parent) const;

  // Returns the index of |child| in |parent|.
  std::optional<size_t> GetIndexOf(TreeModelNode* parent,
                                   TreeModelNode* child) const;

  // Returns the parent of |node|, or NULL if |node| is the root.
  TreeModelNode* GetParent(TreeModelNode* node) const;

  // Adds an observer of the model.
  void AddObserver(TreeModelObserver* observer);

  // Removes an observer of the model.
  void RemoveObserver(TreeModelObserver* observer);

  // Sets the title of |node|.
  // This is only invoked if the node is editable and the user edits a node.
  virtual void SetTitle(TreeModelNode* node, const std::u16string& title);

 private:
  void AddImpl(TreeModelNode* parent,
               std::unique_ptr<TreeModelNode> node,
               size_t index);

  base::ObserverList<TreeModelObserver>::Unchecked observer_list_;
  std::unique_ptr<TreeModelNode> root_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_TREE_MODEL_H_
