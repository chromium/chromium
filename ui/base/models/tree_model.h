// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_TREE_MODEL_H_
#define UI_BASE_MODELS_TREE_MODEL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"

namespace ui {

class ImageModel;
class TreeModel;

// TreeModelNode --------------------------------------------------------------

// Type of class returned from the model. This is a low-level interface.
// Generally you will want to use TreeNode or TreeNodeWithValue which provides
// a basic implementation for storing the tree hierarchy. See
// tree_node_model.h.
class TreeModelNode {
 public:
  // Returns the title for the node.
  virtual const std::u16string& GetTitle() const = 0;

  // Returns the accessible title for the node.
  virtual const std::u16string& GetAccessibleTitle() const = 0;

 protected:
  virtual ~TreeModelNode() {}
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

// The model for TreeView. This is a low-level interface and requires a lot
// of bookkeeping for the tree to be implemented. Generally you will want to
// use TreeNodeModel which provides a standard implementation for basic
// hierarchy and observer notification. See tree_node_model.h.
class COMPONENT_EXPORT(UI_BASE) TreeModel {
 public:
  using Nodes = std::vector<TreeModelNode*>;

  // Returns the root of the tree. This may or may not be shown in the tree,
  // see SetRootShown for details.
  virtual TreeModelNode* GetRoot() = 0;

  // Returns the children of |parent|.
  virtual Nodes GetChildren(const TreeModelNode* parent) const = 0;

  // Returns the index of |child| in |parent|.
  virtual std::optional<size_t> GetIndexOf(TreeModelNode* parent,
                                           TreeModelNode* child) const = 0;

  // Returns the parent of |node|, or NULL if |node| is the root.
  virtual TreeModelNode* GetParent(TreeModelNode* node) const = 0;

  // Adds an observer of the model.
  virtual void AddObserver(TreeModelObserver* observer) = 0;

  // Removes an observer of the model.
  virtual void RemoveObserver(TreeModelObserver* observer) = 0;

  // Sets the title of |node|.
  // This is only invoked if the node is editable and the user edits a node.
  virtual void SetTitle(TreeModelNode* node, const std::u16string& title);

  // Returns the set of icons for the nodes in the tree. You only need override
  // this if you don't want to use the default folder icons.
  virtual void GetIcons(std::vector<ui::ImageModel>* icons) {}

  // Returns the index of the icon to use for |node|. Return nullopt to use the
  // default icon. The index is relative to the list of icons returned from
  // GetIcons.
  virtual std::optional<size_t> GetIconIndex(TreeModelNode* node);

 protected:
  virtual ~TreeModel() {}
};

}  // namespace ui

#endif  // UI_BASE_MODELS_TREE_MODEL_H_
