// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_widget_obj_wrapper.h"

#include <vector>

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
  widget_observer_.Add(widget);
  widget->AddRemovalsObserver(this);
}

AXWidgetObjWrapper::~AXWidgetObjWrapper() {
  widget_->RemoveRemovalsObserver(this);
}

bool AXWidgetObjWrapper::IsIgnored() {
  return false;
}

AXAuraObjWrapper* AXWidgetObjWrapper::GetParent() {
  return aura_obj_cache_->GetOrCreate(widget_->GetNativeView());
}

void AXWidgetObjWrapper::GetChildren(
    std::vector<AXAuraObjWrapper*>* out_children) {
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

int32_t AXWidgetObjWrapper::GetUniqueId() const {
  return unique_id_.Get();
}

void AXWidgetObjWrapper::OnWidgetDestroying(Widget* widget) {
  aura_obj_cache_->Remove(widget);
}

void AXWidgetObjWrapper::OnWidgetClosing(Widget* widget) {
  aura_obj_cache_->Remove(widget);
}

void AXWidgetObjWrapper::OnWidgetVisibilityChanged(Widget*, bool) {
  // If a widget changes visibility it may affect what's focused, in particular
  // when a widget that contains the focused view gets hidden.
  aura_obj_cache_->OnFocusedViewChanged();
}

void AXWidgetObjWrapper::OnWillRemoveView(Widget* widget, View* view) {
  aura_obj_cache_->RemoveViewSubtree(view);
}

}  // namespace views
