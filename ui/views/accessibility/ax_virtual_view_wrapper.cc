// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_virtual_view_wrapper.h"

#include "ui/views/accessibility/ax_view_obj_wrapper.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"

namespace views {

AXVirtualViewWrapper::AXVirtualViewWrapper(AXVirtualView* virtual_view,
                                           AXAuraObjCache* cache)
    : AXAuraObjWrapper(cache), virtual_view_(virtual_view) {}

AXVirtualViewWrapper::~AXVirtualViewWrapper() = default;

bool AXVirtualViewWrapper::IsIgnored() {
  return false;
}

AXAuraObjWrapper* AXVirtualViewWrapper::GetParent() {
  if (virtual_view_->virtual_parent_view()) {
    return const_cast<AXVirtualView*>(virtual_view_->virtual_parent_view())
        ->GetOrCreateWrapper(aura_obj_cache_);
  }
  if (virtual_view_->GetOwnerView())
    return aura_obj_cache_->GetOrCreate(virtual_view_->GetOwnerView());

  return nullptr;
}

void AXVirtualViewWrapper::GetChildren(
    std::vector<AXAuraObjWrapper*>* out_children) {
  for (const auto& child : virtual_view_->children())
    out_children->push_back(child->GetOrCreateWrapper(aura_obj_cache_));
}

void AXVirtualViewWrapper::Serialize(ui::AXNodeData* out_node_data) {
  *out_node_data = virtual_view_->GetData();
}

int32_t AXVirtualViewWrapper::GetUniqueId() const {
  return virtual_view_->GetUniqueId().Get();
}

bool AXVirtualViewWrapper::HandleAccessibleAction(
    const ui::AXActionData& action) {
  return virtual_view_->HandleAccessibleAction(action);
}

std::string AXVirtualViewWrapper::ToString() const {
  std::string description = "Virtual view child of ";
  return description + virtual_view_->GetOwnerView()->GetClassName();
}

}  // namespace views
