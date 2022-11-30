// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tree/tree_view_drawing_provider.h"

#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/controls/tree/tree_view.h"

namespace views {

TreeViewDrawingProvider::TreeViewDrawingProvider() = default;
TreeViewDrawingProvider::~TreeViewDrawingProvider() = default;

SkColor TreeViewDrawingProvider::GetBackgroundColorForNode(
    TreeView* tree_view,
    ui::TreeModelNode* node) {
  ui::ColorId color_id = (tree_view->HasFocus() || tree_view->GetEditingNode())
                             ? ui::kColorTreeNodeBackgroundSelectedFocused
                             : ui::kColorTreeNodeBackgroundSelectedUnfocused;
  return tree_view->GetColorProvider()->GetColor(color_id);
}

SkColor TreeViewDrawingProvider::GetTextColorForNode(TreeView* tree_view,
                                                     ui::TreeModelNode* node) {
  ui::ColorId color_id = ui::kColorTreeNodeForeground;
  if (tree_view->GetSelectedNode() == node) {
    color_id = tree_view->HasFocus()
                   ? ui::kColorTreeNodeForegroundSelectedFocused
                   : ui::kColorTreeNodeForegroundSelectedUnfocused;
  }
  return tree_view->GetColorProvider()->GetColor(color_id);
}

SkColor TreeViewDrawingProvider::GetAuxiliaryTextColorForNode(
    TreeView* tree_view,
    ui::TreeModelNode* node) {
  // Default to using the same color as the primary text.
  return GetTextColorForNode(tree_view, node);
}

std::u16string TreeViewDrawingProvider::GetAuxiliaryTextForNode(
    TreeView* tree_view,
    ui::TreeModelNode* node) {
  return std::u16string();
}

bool TreeViewDrawingProvider::ShouldDrawIconForNode(TreeView* tree_view,
                                                    ui::TreeModelNode* node) {
  return true;
}

}  // namespace views
