// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TREE_VIEW_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TREE_VIEW_EXAMPLE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/models/tree_node_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/controls/tree/tree_view_controller.h"
#include "ui/views/examples/example_base.h"

namespace ui {
class SimpleMenuModel;
}

namespace views {

class LabelButton;
class MenuRunner;

namespace examples {

class VIEWS_EXAMPLES_EXPORT TreeViewExample
    : public ExampleBase,
      public TreeViewController,
      public ContextMenuController,
      public ui::SimpleMenuModel::Delegate {
 public:
  TreeViewExample();

  TreeViewExample(const TreeViewExample&) = delete;
  TreeViewExample& operator=(const TreeViewExample&) = delete;

  ~TreeViewExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  // IDs used by the context menu.
  enum MenuIDs { ID_EDIT, ID_REMOVE, ID_ADD };

  void AddNewNode();
  void RemoveSelectedNode();
  void SetSelectedNodeTitle();

  // Non-const version of IsCommandIdEnabled.
  bool IsCommandIdEnabled(int command_id);

  // TreeViewController:
  void OnTreeViewSelectionChanged(TreeView* tree_view) override;
  bool CanEdit(TreeView* tree_view, ui::TreeModelNode* node) override;

  // ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // The tree view to be tested.
  raw_ptr<TreeView> tree_view_ = nullptr;

  // Control buttons to modify the model.
  raw_ptr<LabelButton> add_ = nullptr;
  raw_ptr<LabelButton> remove_ = nullptr;
  raw_ptr<LabelButton> change_title_ = nullptr;

  using NodeType = ui::TreeNodeWithValue<int>;

  ui::TreeNodeModel<NodeType> model_;

  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<MenuRunner> context_menu_runner_;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TREE_VIEW_EXAMPLE_H_
