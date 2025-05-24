// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_action_handler_base.h"

#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ui {

bool AXActionHandlerBase::RequiresPerformActionPointInPixels() const {
  return false;
}

AXActionHandlerBase::AXActionHandlerBase()
    : AXActionHandlerBase(AXTreeIDUnknown()) {}

AXActionHandlerBase::AXActionHandlerBase(const AXTreeID& ax_tree_id)
    : tree_id_(ax_tree_id) {}

AXActionHandlerBase::~AXActionHandlerBase() {
  AXActionHandlerRegistry::GetInstance()->RemoveAXTreeID(tree_id_);
}

void AXActionHandlerBase::SetAXTreeID(AXTreeID new_ax_tree_id) {
  DCHECK_NE(new_ax_tree_id, AXTreeIDUnknown());
  AXActionHandlerRegistry::GetInstance()->RemoveAXTreeID(tree_id_);
  tree_id_ = new_ax_tree_id;
  AXActionHandlerRegistry::GetInstance()->SetAXTreeID(tree_id_, this);
}

void AXActionHandlerBase::RemoveAXTreeID() {
  DCHECK_NE(tree_id_, ui::AXTreeIDUnknown());
  AXActionHandlerRegistry::GetInstance()->RemoveAXTreeID(tree_id_);
  tree_id_ = AXTreeIDUnknown();
}

}  // namespace ui
