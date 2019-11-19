// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tree/tree_view_drawing_provider.h"

#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/tree/tree_view.h"

namespace views {

TreeViewDrawingProvider::TreeViewDrawingProvider() = default;
TreeViewDrawingProvider::~TreeViewDrawingProvider() = default;

SkColor TreeViewDrawingProvider::GetBackgroundColorForNode(
    TreeView* tree_view,
    ui::TreeModelNode* node) {
  ui::NativeTheme::ColorId color_id =
      (tree_view->HasFocus() || tree_view->GetEditingNode())
          ? ui::NativeTheme::kColorId_TreeSelectionBackgroundFocused
          : ui::NativeTheme::kColorId_TreeSelectionBackgroundUnfocused;
  return tree_view->GetNativeTheme()->GetSystemColor(color_id);
}

SkColor TreeViewDrawingProvider::GetTextColorForNode(TreeView* tree_view,
                                                     ui::TreeModelNode* node) {
  ui::NativeTheme::ColorId color_id = ui::NativeTheme::kColorId_TreeText;
  if (tree_view->GetSelectedNode() == node) {
    if (tree_view->HasFocus())
      color_id = ui::NativeTheme::kColorId_TreeSelectedText;
    else
      color_id = ui::NativeTheme::kColorId_TreeSelectedTextUnfocused;
  }
  return tree_view->GetNativeTheme()->GetSystemColor(color_id);
}

base::string16 TreeViewDrawingProvider::GetAuxiliaryTextForNode(
    TreeView* tree_view,
    ui::TreeModelNode* node) {
  return base::string16();
}

bool TreeViewDrawingProvider::ShouldDrawIconForNode(TreeView* tree_view,
                                                    ui::TreeModelNode* node) {
  return true;
}

}  // namespace views
