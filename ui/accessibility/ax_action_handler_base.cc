// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_action_handler_base.h"

#include "ui/accessibility/ax_action_handler_registry.h"

namespace ui {

bool AXActionHandlerBase::RequiresPerformActionPointInPixels() const {
  return false;
}

AXActionHandlerBase::AXActionHandlerBase()
    : AXActionHandlerBase(ui::AXTreeIDUnknown()) {}

AXActionHandlerBase::AXActionHandlerBase(const AXTreeID& ax_tree_id)
    : tree_id_(ax_tree_id) {}

AXActionHandlerBase::~AXActionHandlerBase() {
  AXActionHandlerRegistry::GetInstance()->RemoveAXTreeID(tree_id_);
}

void AXActionHandlerBase::SetAXTreeID(AXTreeID new_ax_tree_id) {
  DCHECK_NE(new_ax_tree_id, ui::AXTreeIDUnknown());
  AXActionHandlerRegistry::GetInstance()->RemoveAXTreeID(tree_id_);
  tree_id_ = new_ax_tree_id;
  AXActionHandlerRegistry::GetInstance()->SetAXTreeID(tree_id_, this);
}

}  // namespace ui
