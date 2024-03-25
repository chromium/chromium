// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_serializer.h"

namespace ui {

ClientTreeNode::ClientTreeNode(AXNodeID id, ClientTreeNode* parent)
    : id(id), parent(parent) {}

ClientTreeNode::~ClientTreeNode() {
}

}  // namespace ui
