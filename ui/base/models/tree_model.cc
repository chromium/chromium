// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/tree_model.h"

#include "base/notreached.h"

namespace ui {

void TreeModel::SetTitle(TreeModelNode* node, const std::u16string& title) {
  NOTREACHED();
}

std::optional<size_t> TreeModel::GetIconIndex(TreeModelNode* node) {
  return std::nullopt;
}

}  // namespace ui
