// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tree/tree_view_controller.h"

#include "ui/base/models/tree_model.h"
#include "ui/views/controls/tree/tree_view.h"

namespace views {

bool TreeViewController::CanEdit(TreeView* tree_view, ui::TreeModelNode* node) {
  return true;
}

TreeViewController::~TreeViewController() = default;

}  // namespace views
