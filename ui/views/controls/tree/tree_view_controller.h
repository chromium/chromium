// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TREE_TREE_VIEW_CONTROLLER_H_
#define UI_VIEWS_CONTROLS_TREE_TREE_VIEW_CONTROLLER_H_

#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/views_export.h"

namespace ui {
class TreeModelNode;
}

namespace views {

class TreeView;

// TreeViewController ---------------------------------------------------------

// Controller for the treeview.
class VIEWS_EXPORT TreeViewController {
 public:
  // Notification that the selection of the tree view has changed. Use
  // GetSelectedNode to find the current selection.
  virtual void OnTreeViewSelectionChanged(TreeView* tree_view) = 0;

  // Returns true if the node can be edited. This is only used if the
  // TreeView is editable.
  virtual bool CanEdit(TreeView* tree_view, ui::TreeModelNode* node);

 protected:
  virtual ~TreeViewController();
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TREE_TREE_VIEW_CONTROLLER_H_
