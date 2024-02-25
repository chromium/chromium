// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_widget_obj_wrapper.h"

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

AXWidgetObjWrapper::AXWidgetObjWrapper(AXAuraObjCache* aura_obj_cache,
                                       Widget* widget)
    : AXAuraObjWrapper(aura_obj_cache), widget_(widget) {
  DCHECK(widget->GetNativeView());
  widget_observation_.Observe(widget);
}

AXWidgetObjWrapper::~AXWidgetObjWrapper() = default;

AXAuraObjWrapper* AXWidgetObjWrapper::GetParent() {
  return aura_obj_cache_->GetOrCreate(widget_->GetNativeView());
}

void AXWidgetObjWrapper::GetChildren(
    std::vector<raw_ptr<AXAuraObjWrapper, VectorExperimental>>* out_children) {
  if (!widget_->IsVisible() || !widget_->GetRootView() ||
      !widget_->GetRootView()->GetVisible()) {
    return;
  }

  out_children->push_back(aura_obj_cache_->GetOrCreate(widget_->GetRootView()));
}

void AXWidgetObjWrapper::Serialize(ui::AXNodeData* out_node_data) {
  out_node_data->id = GetUniqueId();
  out_node_data->role = widget_->widget_delegate()->GetAccessibleWindowRole();
  out_node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kName,
      base::UTF16ToUTF8(
          widget_->widget_delegate()->GetAccessibleWindowTitle()));
  out_node_data->AddStringAttribute(ax::mojom::StringAttribute::kClassName,
                                    "Widget");
  out_node_data->relative_bounds.bounds =
      gfx::RectF(widget_->GetWindowBoundsInScreen());
  out_node_data->state = 0;
}

ui::AXNodeID AXWidgetObjWrapper::GetUniqueId() const {
  return unique_id_.Get();
}

std::string AXWidgetObjWrapper::ToString() const {
  return "Widget";
}

void AXWidgetObjWrapper::OnWidgetDestroying(Widget* widget) {
  aura_obj_cache_->Remove(widget);
}

void AXWidgetObjWrapper::OnWidgetDestroyed(Widget* widget) {
  // Normally this does not run because of OnWidgetDestroying should have
  // removed |this| from cache. However, some code could trigger a destroying
  // widget to be created after OnWidgetDestroying. This guards against such
  // situation and ensures the destroyed widget is removed from cache.
  // See https://crbug.com/1091545
  aura_obj_cache_->Remove(widget);
}

}  // namespace views
