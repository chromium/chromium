// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/tree_view_example.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/controls/tree/tree_view_drawing_provider.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace {

class ExampleTreeViewDrawingProvider : public views::TreeViewDrawingProvider {
 public:
  ExampleTreeViewDrawingProvider() = default;
  ~ExampleTreeViewDrawingProvider() override = default;

  std::u16string GetAuxiliaryTextForNode(views::TreeView* tree_view,
                                         ui::TreeModelNode* node) override {
    if (tree_view->GetSelectedNode() == node)
      return GetStringUTF16(IDS_TREE_VIEW_SELECTED_LABEL);
    return views::TreeViewDrawingProvider::GetAuxiliaryTextForNode(tree_view,
                                                                   node);
  }

  bool ShouldDrawIconForNode(views::TreeView* tree_view,
                             ui::TreeModelNode* node) override {
    return tree_view->GetSelectedNode() != node;
  }
};

}  // namespace

namespace views::examples {

TreeViewExample::TreeViewExample()
    : ExampleBase(GetStringUTF8(IDS_TREE_VIEW_SELECT_LABEL).c_str()),
      model_(std::make_unique<NodeType>(
          GetStringUTF16(IDS_TREE_VIEW_ROOT_NODE_LABEL),
          1)) {}

TreeViewExample::~TreeViewExample() {
  if (tree_view_) {
    tree_view_->SetModel(nullptr);
    tree_view_->set_context_menu_controller(nullptr);
    tree_view_->SetController(nullptr);
  }
}

void TreeViewExample::CreateExampleView(View* container) {
  // Add some sample data.
  NodeType* colors_node = model_.GetRoot()->Add(
      std::make_unique<NodeType>(GetStringUTF16(IDS_TREE_VIEW_COLOR_NODE_LABEL),
                                 1),
      0);
  colors_node->Add(std::make_unique<NodeType>(
                       GetStringUTF16(IDS_TREE_VIEW_COLOR_RED_LABEL), 1),
                   0);
  colors_node->Add(std::make_unique<NodeType>(
                       GetStringUTF16(IDS_TREE_VIEW_COLOR_GREEN_LABEL), 1),
                   1);
  colors_node->Add(std::make_unique<NodeType>(
                       GetStringUTF16(IDS_TREE_VIEW_COLOR_BLUE_LABEL), 1),
                   2);

  NodeType* sheep_node = model_.GetRoot()->Add(
      std::make_unique<NodeType>(GetStringUTF16(IDS_TREE_VIEW_SHEEP_NODE_LABEL),
                                 1),
      0);
  sheep_node->Add(
      std::make_unique<NodeType>(GetStringUTF16(IDS_TREE_VIEW_SHEEP1_LABEL), 1),
      0);
  sheep_node->Add(
      std::make_unique<NodeType>(GetStringUTF16(IDS_TREE_VIEW_SHEEP2_LABEL), 1),
      1);

  auto tree_view = std::make_unique<TreeView>();
  tree_view->set_context_menu_controller(this);
  tree_view->SetRootShown(false);
  tree_view->SetModel(&model_);
  tree_view->SetController(this);
  tree_view->SetDrawingProvider(
      std::make_unique<ExampleTreeViewDrawingProvider>());
  auto add = std::make_unique<LabelButton>(
      base::BindRepeating(&TreeViewExample::AddNewNode, base::Unretained(this)),
      GetStringUTF16(IDS_TREE_VIEW_ADD_BUTTON_LABEL));
  add->SetRequestFocusOnPress(true);
  auto remove = std::make_unique<LabelButton>(
      base::BindRepeating(&TreeViewExample::RemoveSelectedNode,
                          base::Unretained(this)),
      GetStringUTF16(IDS_TREE_VIEW_REMOVE_BUTTON_LABEL));
  remove->SetRequestFocusOnPress(true);
  auto change_title = std::make_unique<LabelButton>(
      base::BindRepeating(&TreeViewExample::SetSelectedNodeTitle,
                          base::Unretained(this)),
      GetStringUTF16(IDS_TREE_VIEW_CHANGE_TITLE_LABEL));
  change_title->SetRequestFocusOnPress(true);

  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(LayoutOrientation::kVertical);

  auto full_flex = FlexSpecification(MinimumFlexSizeRule::kScaleToZero,
                                     MaximumFlexSizeRule::kUnbounded)
                       .WithWeight(1);

  tree_view_ = tree_view.get();
  container
      ->AddChildView(TreeView::CreateScrollViewWithTree(std::move(tree_view)))
      ->SetProperty(views::kFlexBehaviorKey, full_flex);

  // Add control buttons horizontally.
  auto* button_panel = container->AddChildView(std::make_unique<View>());
  button_panel->SetLayoutManager(std::make_unique<FlexLayout>())
      ->SetOrientation(LayoutOrientation::kHorizontal);

  add_ = button_panel->AddChildView(std::move(add));
  remove_ = button_panel->AddChildView(std::move(remove));
  change_title_ = button_panel->AddChildView(std::move(change_title));

  for (View* view : button_panel->children())
    view->SetProperty(views::kFlexBehaviorKey, full_flex);
}

void TreeViewExample::AddNewNode() {
  NodeType* selected_node =
      static_cast<NodeType*>(tree_view_->GetSelectedNode());
  if (!selected_node)
    selected_node = model_.GetRoot();
  NodeType* new_node = model_.Add(
      selected_node, std::make_unique<NodeType>(selected_node->GetTitle(), 1));
  tree_view_->SetSelectedNode(new_node);
}

void TreeViewExample::RemoveSelectedNode() {
  auto* selected_node = static_cast<NodeType*>(tree_view_->GetSelectedNode());
  DCHECK(selected_node);
  DCHECK_NE(model_.GetRoot(), selected_node);
  model_.Remove(selected_node->parent(), selected_node);
}

void TreeViewExample::SetSelectedNodeTitle() {
  auto* selected_node = static_cast<NodeType*>(tree_view_->GetSelectedNode());
  DCHECK(selected_node);
  model_.SetTitle(
      selected_node,
      selected_node->GetTitle() + GetStringUTF16(IDS_TREE_VIEW_NEW_NODE_LABEL));
}

bool TreeViewExample::IsCommandIdEnabled(int command_id) {
  return command_id != ID_REMOVE ||
         tree_view_->GetSelectedNode() != model_.GetRoot();
}

void TreeViewExample::OnTreeViewSelectionChanged(TreeView* tree_view) {
  ui::TreeModelNode* node = tree_view_->GetSelectedNode();
  if (node) {
    change_title_->SetEnabled(true);
    remove_->SetEnabled(node != model_.GetRoot());
  } else {
    change_title_->SetEnabled(false);
    remove_->SetEnabled(false);
  }
}

bool TreeViewExample::CanEdit(TreeView* tree_view, ui::TreeModelNode* node) {
  return true;
}

void TreeViewExample::ShowContextMenuForViewImpl(
    View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  context_menu_model_->AddItem(ID_EDIT,
                               GetStringUTF16(IDS_TREE_VIEW_EDIT_BUTTON_LABEL));
  context_menu_model_->AddItem(
      ID_REMOVE, GetStringUTF16(IDS_TREE_VIEW_REMOVE_BUTTON_LABEL));
  context_menu_model_->AddItem(ID_ADD,
                               GetStringUTF16(IDS_TREE_VIEW_ADD_BUTTON_LABEL));
  context_menu_runner_ =
      std::make_unique<MenuRunner>(context_menu_model_.get(), 0);
  context_menu_runner_->RunMenuAt(source->GetWidget(), nullptr,
                                  gfx::Rect(point, gfx::Size()),
                                  MenuAnchorPosition::kTopLeft, source_type);
}

bool TreeViewExample::IsCommandIdChecked(int command_id) const {
  return false;
}

bool TreeViewExample::IsCommandIdEnabled(int command_id) const {
  return const_cast<TreeViewExample*>(this)->IsCommandIdEnabled(command_id);
}

void TreeViewExample::ExecuteCommand(int command_id, int event_flags) {
  NodeType* selected_node =
      static_cast<NodeType*>(tree_view_->GetSelectedNode());
  switch (command_id) {
    case ID_EDIT:
      tree_view_->StartEditing(selected_node);
      break;
    case ID_REMOVE:
      model_.Remove(selected_node->parent(), selected_node);
      break;
    case ID_ADD:
      AddNewNode();
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace views::examples
