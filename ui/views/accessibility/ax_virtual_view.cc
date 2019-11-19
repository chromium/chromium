// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_virtual_view.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/callback.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if OS_WIN
#include "ui/views/win/hwnd_util.h"
#endif

namespace views {

// Tracks all virtual ax views.
std::map<int32_t, AXVirtualView*>& GetIdMap() {
  static base::NoDestructor<std::map<int32_t, AXVirtualView*>> id_to_obj_map;
  return *id_to_obj_map;
}

// static
const char AXVirtualView::kViewClassName[] = "AXVirtualView";

// static
AXVirtualView* AXVirtualView::GetFromId(int32_t id) {
  auto& id_map = GetIdMap();
  const auto& it = id_map.find(id);
  return it != id_map.end() ? it->second : nullptr;
}

AXVirtualView::AXVirtualView() {
  GetIdMap()[unique_id_.Get()] = this;
  ax_platform_node_ = ui::AXPlatformNode::Create(this);
  DCHECK(ax_platform_node_);
  custom_data_.AddStringAttribute(ax::mojom::StringAttribute::kClassName,
                                  GetViewClassName());
}

AXVirtualView::~AXVirtualView() {
  GetIdMap().erase(unique_id_.Get());
  DCHECK(!parent_view_ || !virtual_parent_view_)
      << "Either |parent_view_| or |virtual_parent_view_| could be set but "
         "not both.";

  if (ax_platform_node_) {
    ax_platform_node_->Destroy();
    ax_platform_node_ = nullptr;
  }
}

void AXVirtualView::AddChildView(std::unique_ptr<AXVirtualView> view) {
  DCHECK(view);
  if (view->virtual_parent_view_ == this)
    return;
  AddChildViewAt(std::move(view), GetChildCount());

  if (GetOwnerView()) {
    GetOwnerView()->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                             false);
  }
}

void AXVirtualView::AddChildViewAt(std::unique_ptr<AXVirtualView> view,
                                   int index) {
  DCHECK(view);
  CHECK_NE(view.get(), this)
      << "You cannot add an AXVirtualView as its own child.";
  DCHECK(!view->parent_view_) << "This |view| already has a View "
                                 "parent. Call RemoveVirtualChildView first.";
  DCHECK(!view->virtual_parent_view_) << "This |view| already has an "
                                         "AXVirtualView parent. Call "
                                         "RemoveChildView first.";
  DCHECK_GE(index, 0);
  DCHECK_LE(index, GetChildCount());

  view->virtual_parent_view_ = this;
  children_.insert(children_.begin() + index, std::move(view));
  if (GetOwnerView()) {
    GetOwnerView()->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                             false);
  }
}

void AXVirtualView::ReorderChildView(AXVirtualView* view, int index) {
  DCHECK(view);
  if (index >= GetChildCount())
    return;
  if (index < 0)
    index = GetChildCount() - 1;

  DCHECK_EQ(view->virtual_parent_view_, this);
  if (children_[index].get() == view)
    return;

  int cur_index = GetIndexOf(view);
  if (cur_index < 0)
    return;

  std::unique_ptr<AXVirtualView> child = std::move(children_[cur_index]);
  children_.erase(children_.begin() + cur_index);
  children_.insert(children_.begin() + index, std::move(child));

  GetOwnerView()->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                           false);
}

std::unique_ptr<AXVirtualView> AXVirtualView::RemoveChildView(
    AXVirtualView* view) {
  DCHECK(view);
  int cur_index = GetIndexOf(view);
  if (cur_index < 0)
    return {};

  if (GetOwnerView()) {
    ViewAccessibility& view_accessibility =
        GetOwnerView()->GetViewAccessibility();
    if (view_accessibility.FocusedVirtualChild() &&
        Contains(view_accessibility.FocusedVirtualChild())) {
      view_accessibility.OverrideFocus(nullptr);
    }
  }

  std::unique_ptr<AXVirtualView> child = std::move(children_[cur_index]);
  children_.erase(children_.begin() + cur_index);
  child->virtual_parent_view_ = nullptr;
  child->populate_data_callback_.Reset();

  if (GetOwnerView()) {
    GetOwnerView()->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                             false);
  }

  return child;
}

void AXVirtualView::RemoveAllChildViews() {
  while (!children_.empty())
    RemoveChildView(children_.back().get());

  if (GetOwnerView()) {
    GetOwnerView()->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                             false);
  }
}

bool AXVirtualView::Contains(const AXVirtualView* view) const {
  DCHECK(view);
  for (const AXVirtualView* v = view; v; v = v->virtual_parent_view_) {
    if (v == this)
      return true;
  }
  return false;
}

int AXVirtualView::GetIndexOf(const AXVirtualView* view) const {
  DCHECK(view);
  const auto iter =
      std::find_if(children_.begin(), children_.end(),
                   [view](const auto& child) { return child.get() == view; });
  return iter != children_.end() ? static_cast<int>(iter - children_.begin())
                                 : -1;
}

const char* AXVirtualView::GetViewClassName() const {
  return kViewClassName;
}

gfx::NativeViewAccessible AXVirtualView::GetNativeObject() const {
  DCHECK(ax_platform_node_);
  return ax_platform_node_->GetNativeViewAccessible();
}

void AXVirtualView::NotifyAccessibilityEvent(ax::mojom::Event event_type) {
  DCHECK(ax_platform_node_);
  ax_platform_node_->NotifyAccessibilityEvent(event_type);
}

ui::AXNodeData& AXVirtualView::GetCustomData() {
  return custom_data_;
}

void AXVirtualView::SetPopulateDataCallback(
    base::RepeatingCallback<void(const View&, ui::AXNodeData*)> callback) {
  populate_data_callback_ = std::move(callback);
}

void AXVirtualView::UnsetPopulateDataCallback() {
  populate_data_callback_.Reset();
}

// ui::AXPlatformNodeDelegate

const ui::AXNodeData& AXVirtualView::GetData() const {
  // Make a copy of our |custom_data_| so that any modifications will not be
  // made to the data that users of this class will be manipulating.
  static ui::AXNodeData node_data;
  node_data = custom_data_;

  node_data.id = GetUniqueId().Get();

  if (!GetOwnerView() || !GetOwnerView()->GetEnabled())
    node_data.SetRestriction(ax::mojom::Restriction::kDisabled);

  if (!GetOwnerView() || !GetOwnerView()->IsDrawn())
    node_data.AddState(ax::mojom::State::kInvisible);

  if (GetOwnerView() && GetOwnerView()->context_menu_controller())
    node_data.AddAction(ax::mojom::Action::kShowContextMenu);

  if (populate_data_callback_ && GetOwnerView())
    populate_data_callback_.Run(*GetOwnerView(), &node_data);
  return node_data;
}

int AXVirtualView::GetChildCount() {
  return static_cast<int>(children_.size());
}

gfx::NativeViewAccessible AXVirtualView::ChildAtIndex(int index) {
  DCHECK_GE(index, 0) << "Child indices should be greater or equal to 0.";
  DCHECK_LT(index, GetChildCount())
      << "Child indices should be less than the child count.";
  if (index >= 0 && index < GetChildCount())
    return children_[index]->GetNativeObject();
  return nullptr;
}

#if !defined(OS_MACOSX)
gfx::NativeViewAccessible AXVirtualView::GetNSWindow() {
  NOTREACHED();
  return nullptr;
}
#endif

gfx::NativeViewAccessible AXVirtualView::GetNativeViewAccessible() {
  return GetNativeObject();
}

gfx::NativeViewAccessible AXVirtualView::GetParent() {
  if (parent_view_)
    return parent_view_->GetNativeObject();

  if (virtual_parent_view_)
    return virtual_parent_view_->GetNativeObject();

  // This virtual view hasn't been added to a parent view yet.
  return nullptr;
}

gfx::Rect AXVirtualView::GetBoundsRect(
    const ui::AXCoordinateSystem coordinate_system,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  switch (coordinate_system) {
    case ui::AXCoordinateSystem::kScreen:
      // We could optionally add clipping here if ever needed.
      // TODO(nektar): Implement bounds that are relative to the parent.
      return gfx::ToEnclosingRect(custom_data_.relative_bounds.bounds);
    case ui::AXCoordinateSystem::kRootFrame:
    case ui::AXCoordinateSystem::kFrame:
      NOTIMPLEMENTED();
      return gfx::Rect();
  }
}

gfx::NativeViewAccessible AXVirtualView::HitTestSync(int x, int y) {
  // TODO(nektar): Implement.
  return GetNativeObject();
}

gfx::NativeViewAccessible AXVirtualView::GetFocus() {
  if (parent_view_)
    return parent_view_->GetFocusedDescendant();

  if (virtual_parent_view_)
    return virtual_parent_view_->GetFocus();

  // This virtual view hasn't been added to a parent view yet.
  return nullptr;
}

ui::AXPlatformNode* AXVirtualView::GetFromNodeID(int32_t id) {
  // TODO(nektar): Implement.
  return nullptr;
}

bool AXVirtualView::AccessibilityPerformAction(const ui::AXActionData& data) {
  bool result = false;
  if (custom_data_.HasAction(data.action))
    result = HandleAccessibleAction(data);
  if (!result && GetOwnerView())
    return GetOwnerView()->HandleAccessibleAction(data);
  return result;
}

bool AXVirtualView::ShouldIgnoreHoveredStateForTesting() {
  // TODO(nektar): Implement.
  return false;
}

bool AXVirtualView::IsOffscreen() const {
  // TODO(nektar): Implement.
  return false;
}

const ui::AXUniqueId& AXVirtualView::GetUniqueId() const {
  return unique_id_;
}

// Virtual views need to implement this function in order for A11Y events
// to be routed correctly.
gfx::AcceleratedWidget AXVirtualView::GetTargetForNativeAccessibilityEvent() {
#if defined(OS_WIN)
  if (GetOwnerView())
    return HWNDForView(GetOwnerView());
#endif
  return gfx::kNullAcceleratedWidget;
}

bool AXVirtualView::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  return false;
}

View* AXVirtualView::GetOwnerView() const {
  if (parent_view_)
    return parent_view_->view();

  if (virtual_parent_view_)
    return virtual_parent_view_->GetOwnerView();

  // This virtual view hasn't been added to a parent view yet.
  return nullptr;
}

AXVirtualViewWrapper* AXVirtualView::GetOrCreateWrapper(
    views::AXAuraObjCache* cache) {
#if defined(USE_AURA)
  if (!wrapper_)
    wrapper_ = std::make_unique<AXVirtualViewWrapper>(this, cache);
#endif
  return wrapper_.get();
}

}  // namespace views
