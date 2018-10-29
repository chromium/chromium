// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_virtual_view.h"

#include <stdint.h>

#include <algorithm>
#include <utility>

#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"

namespace views {

AXVirtualView::AXVirtualView()
    : parent_view_(nullptr), virtual_parent_view_(nullptr) {
  ax_platform_node_ = ui::AXPlatformNode::Create(this);
  DCHECK(ax_platform_node_);
}

AXVirtualView::~AXVirtualView() {
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
}

void AXVirtualView::AddChildViewAt(std::unique_ptr<AXVirtualView> view,
                                   int index) {
  DCHECK(view);
  CHECK_NE(view.get(), this)
      << "You cannot add an AXVirtualView as its own child.";
  DCHECK(!view->parent_view_) << "This |view| already has an AXVirtualView "
                                 "parent. Call RemoveVirtualChildView first.";
  DCHECK(!view->virtual_parent_view_)
      << "This |view| already has a View parent. Call RemoveChildView first.";
  DCHECK_GE(index, 0);
  DCHECK_LE(index, GetChildCount());

  view->virtual_parent_view_ = this;
  children_.insert(children_.begin() + index, std::move(view));
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
}

std::unique_ptr<AXVirtualView> AXVirtualView::RemoveChildView(
    AXVirtualView* view) {
  DCHECK(view);
  int cur_index = GetIndexOf(view);
  if (cur_index < 0)
    return {};

  std::unique_ptr<AXVirtualView> child = std::move(children_[cur_index]);
  children_.erase(children_.begin() + cur_index);
  child->virtual_parent_view_ = nullptr;
  return child;
}

void AXVirtualView::RemoveAllChildViews() {
  while (!children_.empty())
    RemoveChildView(children_.back().get());
}

const AXVirtualView* AXVirtualView::child_at(int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(children_.size()));
  return children_[index].get();
}

AXVirtualView* AXVirtualView::child_at(int index) {
  return const_cast<AXVirtualView*>(
      const_cast<const AXVirtualView*>(this)->child_at(index));
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

gfx::NativeViewAccessible AXVirtualView::GetNativeObject() const {
  DCHECK(ax_platform_node_);
  return ax_platform_node_->GetNativeViewAccessible();
}

void AXVirtualView::NotifyAccessibilityEvent(ax::mojom::Event event_type) {
  DCHECK(ax_platform_node_);
  ax_platform_node_->NotifyAccessibilityEvent(event_type);
}

void AXVirtualView::OverrideRole(const ax::mojom::Role role) {
  custom_data_.role = role;
}

void AXVirtualView::OverrideState(ax::mojom::State state) {
  custom_data_.state = static_cast<int32_t>(state);
}

void AXVirtualView::OverrideName(const std::string& name) {
  custom_data_.SetName(name);
}

void AXVirtualView::OverrideName(const base::string16& name) {
  custom_data_.SetName(name);
}

void AXVirtualView::OverrideDescription(const std::string& description) {
  custom_data_.SetDescription(description);
}

void AXVirtualView::OverrideDescription(const base::string16& description) {
  custom_data_.SetDescription(description);
}

void AXVirtualView::OverrideBoundsRect(const gfx::RectF& location) {
  custom_data_.location = location;
}

// ui::AXPlatformNodeDelegate

const ui::AXNodeData& AXVirtualView::GetData() const {
  if (!IsParentVisible()) {
    custom_data_.AddState(ax::mojom::State::kInvisible);
  } else {
    custom_data_.RemoveState(ax::mojom::State::kInvisible);
  }
  return custom_data_;
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

gfx::NativeWindow AXVirtualView::GetTopLevelWidget() {
  // TODO(nektar): Implement.
  return nullptr;
}

gfx::NativeViewAccessible AXVirtualView::GetParent() {
  if (parent_view_)
    return parent_view_->GetNativeViewAccessible();

  if (virtual_parent_view_)
    return virtual_parent_view_->GetNativeObject();

  return nullptr;
}

gfx::Rect AXVirtualView::GetClippedScreenBoundsRect() const {
  // We could optionally add clipping here if ever needed.
  // TODO(nektar): Implement.
  return {};
}

gfx::Rect AXVirtualView::GetUnclippedScreenBoundsRect() const {
  // TODO(nektar): Implement.
  return {};
}

gfx::NativeViewAccessible AXVirtualView::HitTestSync(int x, int y) {
  // TODO(nektar): Implement.
  return GetNativeObject();
}

gfx::NativeViewAccessible AXVirtualView::GetFocus() {
  // TODO(nektar): Implement.
  return nullptr;
}

ui::AXPlatformNode* AXVirtualView::GetFromNodeID(int32_t id) {
  // TODO(nektar): Implement.
  return nullptr;
}

bool AXVirtualView::AccessibilityPerformAction(const ui::AXActionData& data) {
  // TODO(nektar): Implement.
  return false;
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

bool AXVirtualView::IsParentVisible() const {
  if (parent_view_) {
    const auto* parent_node = static_cast<ui::AXPlatformNodeBase*>(
        ui::AXPlatformNode::FromNativeViewAccessible(
            parent_view_->GetNativeViewAccessible()));
    if (!parent_node) {
      NOTREACHED() << "AXVirtualView should be created on a platform with "
                      "native accessibility support.";
      return false;
    }

    const ui::AXNodeData& parent_data = parent_node->GetData();
    return !parent_data.HasState(ax::mojom::State::kInvisible);
  }

  if (virtual_parent_view_) {
    return !virtual_parent_view_->GetData().HasState(
        ax::mojom::State::kInvisible);
  }

  // Not attached to a parent yet.
  return false;
}

}  // namespace views
