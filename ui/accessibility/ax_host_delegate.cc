// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_host_delegate.h"

#include "ui/accessibility/ax_tree_id_registry.h"

namespace ui {

AXHostDelegate::AXHostDelegate()
    : tree_id_(AXTreeIDRegistry::GetInstance()->GetOrCreateAXTreeID(this)) {}

AXHostDelegate::AXHostDelegate(AXTreeID tree_id) : tree_id_(tree_id) {
  AXTreeIDRegistry::GetInstance()->SetDelegateForID(this, tree_id);
}

AXHostDelegate::~AXHostDelegate() {
  AXTreeIDRegistry::GetInstance()->RemoveAXTreeID(tree_id_);
}

}  // namespace ui
