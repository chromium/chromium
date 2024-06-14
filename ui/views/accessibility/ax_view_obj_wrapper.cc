// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_view_obj_wrapper.h"

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

namespace views {

AXViewObjWrapper::AXViewObjWrapper(AXAuraObjCache* aura_obj_cache, View* view)
    : AXAuraObjWrapper(aura_obj_cache), view_(view) {
  if (view->GetWidget())
    aura_obj_cache_->GetOrCreate(view->GetWidget());
  observation_.Observe(view);
}

AXViewObjWrapper::~AXViewObjWrapper() = default;

AXAuraObjWrapper* AXViewObjWrapper::GetParent() {
  if (view_->parent()) {
    if (view_->parent()->GetViewAccessibility().GetChildTreeID() !=
        ui::AXTreeIDUnknown())
      return nullptr;

    return aura_obj_cache_->GetOrCreate(view_->parent());
  }

  if (view_->GetWidget())
    return aura_obj_cache_->GetOrCreate(view_->GetWidget());

  return nullptr;
}

void AXViewObjWrapper::GetChildren(
    std::vector<raw_ptr<AXAuraObjWrapper, VectorExperimental>>* out_children) {
  const ViewAccessibility& view_accessibility = view_->GetViewAccessibility();

  // Ignore this view's descendants if it has a child tree.
  if (view_accessibility.GetChildTreeID() != ui::AXTreeIDUnknown())
    return;

  if (view_accessibility.IsLeaf()) {
    return;
  }

  // TODO(dtseng): Need to handle |Widget| child of |View|.
  for (View* child : view_->children()) {
    if (child->GetVisible())
      out_children->push_back(aura_obj_cache_->GetOrCreate(child));
  }

  for (const auto& child : view_accessibility.virtual_children())
    out_children->push_back(child->GetOrCreateWrapper(aura_obj_cache_));
}

void AXViewObjWrapper::Serialize(ui::AXNodeData* out_node_data) {
  ViewAccessibility& view_accessibility = view_->GetViewAccessibility();
  view_accessibility.GetAccessibleNodeData(out_node_data);

  out_node_data->relative_bounds.bounds =
      gfx::RectF(view_->GetBoundsInScreen());

  if (view_accessibility.GetNextWindowFocus()) {
    out_node_data->AddIntAttribute(
        ax::mojom::IntAttribute::kNextWindowFocusId,
        aura_obj_cache_->GetOrCreate(view_accessibility.GetNextWindowFocus())
            ->GetUniqueId());
  }

  if (view_accessibility.GetPreviousWindowFocus()) {
    out_node_data->AddIntAttribute(
        ax::mojom::IntAttribute::kPreviousWindowFocusId,
        aura_obj_cache_
            ->GetOrCreate(view_accessibility.GetPreviousWindowFocus())
            ->GetUniqueId());
  }
}

ui::AXNodeID AXViewObjWrapper::GetUniqueId() const {
  return view_->GetViewAccessibility().GetUniqueId();
}

bool AXViewObjWrapper::HandleAccessibleAction(const ui::AXActionData& action) {
  return view_->HandleAccessibleAction(action);
}

std::string AXViewObjWrapper::ToString() const {
  return std::string(view_->GetClassName());
}

void AXViewObjWrapper::OnViewIsDeleting(View* observed_view) {
  DCHECK_EQ(view_, observed_view);
  observation_.Reset();
  // Remove() deletes |this|, so this should be the last line in the function.
  aura_obj_cache_->Remove(observed_view);
}

}  // namespace views
