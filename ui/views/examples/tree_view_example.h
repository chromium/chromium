// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TREE_VIEW_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TREE_VIEW_EXAMPLE_H_

#include <memory>

#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/models/tree_node_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
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
      public ButtonListener,
      public TreeViewController,
      public ContextMenuController,
      public ui::SimpleMenuModel::Delegate {
 public:
  TreeViewExample();
  ~TreeViewExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  // IDs used by the context menu.
  enum MenuIDs {
    ID_EDIT,
    ID_REMOVE,
    ID_ADD
  };

  // Adds a new node.
  void AddNewNode();

  // Non-const version of IsCommandIdEnabled.
  bool IsCommandIdEnabled(int command_id);

  // ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

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
  TreeView* tree_view_ = nullptr;

  // Control buttons to modify the model.
  LabelButton* add_ = nullptr;
  LabelButton* remove_ = nullptr;
  LabelButton* change_title_ = nullptr;

  using NodeType = ui::TreeNodeWithValue<int>;

  ui::TreeNodeModel<NodeType> model_;

  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<MenuRunner> context_menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(TreeViewExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TREE_VIEW_EXAMPLE_H_
