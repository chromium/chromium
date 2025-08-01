// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_virtual_view.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/containers/adapters.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/buildflags.h"
#include "ui/base/layout.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/accessibility/ax_update_notifier.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/accessibility/view_ax_platform_node_delegate.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
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

AXVirtualView::AXVirtualView() : ViewAccessibility(nullptr) {
  GetIdMap()[ViewAccessibility::GetUniqueId()] = this;
  ax_platform_node_ = ui::AXPlatformNode::Create(*this);
  DCHECK(ax_platform_node_);
  SetClassName(GetViewClassName());
}

AXVirtualView::~AXVirtualView() {
  GetIdMap().erase(ViewAccessibility::GetUniqueId());
  DCHECK(!parent_view_ || !virtual_parent_view_)
      << "Either |parent_view_| or |virtual_parent_view_| could be set but "
         "not both.";

#if defined(USE_AURA)
  if (ax_aura_obj_cache_) {
    ax_aura_obj_cache_->Remove(this);
  }
#endif
}

void AXVirtualView::AddChildView(std::unique_ptr<AXVirtualView> view) {
  DCHECK(view);
  if (view->virtual_parent_view_ == this) {
    return;  // Already a child of this virtual view.
  }
  AddChildViewAt(std::move(view), children_.size());
}

void AXVirtualView::AddChildViewAt(std::unique_ptr<AXVirtualView> view,
                                   size_t index) {
  DCHECK(view);
  CHECK_NE(view.get(), this)
      << "You cannot add an AXVirtualView as its own child.";
  DCHECK(!view->parent_view_) << "This |view| already has a View "
                                 "parent. Call RemoveVirtualChildView first.";
  DCHECK(!view->virtual_parent_view_) << "This |view| already has an "
                                         "AXVirtualView parent. Call "
                                         "RemoveChildView first.";
  DCHECK_LE(index, children_.size());

  view->virtual_parent_view_ = this;
  children_.insert(children_.begin() + static_cast<ptrdiff_t>(index),
                   std::move(view));
  views::View* owner_view = GetOwnerView();

  AXVirtualView* added_view = children_[index].get();
  added_view->OnViewHasNewAncestor(
      /* ancestor_focusable */ data().HasState(ax::mojom::State::kFocusable) ||
      has_focusable_ancestor());

  if (owner_view) {
    owner_view->NotifyAccessibilityEventDeprecated(
        ax::mojom::Event::kChildrenChanged, true);
  }
}

void AXVirtualView::ReorderChildView(AXVirtualView* view, size_t index) {
  DCHECK(view);
  index = std::min(index, children_.size() - 1);

  DCHECK_EQ(view->virtual_parent_view_, this);
  if (children_[index].get() == view) {
    return;
  }

  auto cur_index = GetIndexOf(view);
  if (!cur_index.has_value()) {
    return;
  }

  std::unique_ptr<AXVirtualView> child =
      std::move(children_[cur_index.value()]);
  children_.erase(children_.begin() +
                  static_cast<ptrdiff_t>(cur_index.value()));
  children_.insert(children_.begin() + static_cast<ptrdiff_t>(index),
                   std::move(child));

  GetOwnerView()->NotifyAccessibilityEventDeprecated(
      ax::mojom::Event::kChildrenChanged, true);
}

std::unique_ptr<AXVirtualView> AXVirtualView::RemoveFromParentView() {
  if (parent_view_) {
    return parent_view_->RemoveVirtualChildView(this);
  }

  // This virtual view hasn't been added to a parent view yet.
  CHECK(virtual_parent_view_)
      << "Cannot remove from parent view if there is no parent.";
  return virtual_parent_view_->RemoveChildView(this);
}

std::unique_ptr<AXVirtualView> AXVirtualView::RemoveChildView(
    AXVirtualView* view) {
  DCHECK(view);
  auto cur_index = GetIndexOf(view);
  if (!cur_index.has_value()) {
    return {};
  }

  bool focus_changed = false;
  if (GetOwnerView()) {
    ViewAccessibility& view_accessibility =
        GetOwnerView()->GetViewAccessibility();
    if (view_accessibility.FocusedVirtualChild() &&
        Contains(view_accessibility.FocusedVirtualChild())) {
      focus_changed = true;
    }
  }

  std::unique_ptr<AXVirtualView> child =
      std::move(children_[cur_index.value()]);
  children_.erase(children_.begin() +
                  static_cast<ptrdiff_t>(cur_index.value()));
  child->virtual_parent_view_ = nullptr;

  if (GetOwnerView()) {
    if (focus_changed) {
      GetOwnerView()->GetViewAccessibility().OverrideFocus(nullptr);
    }
    GetOwnerView()->NotifyAccessibilityEventDeprecated(
        ax::mojom::Event::kChildrenChanged, true);
  }

  return child;
}

void AXVirtualView::RemoveAllChildViews() {
  while (!children_.empty()) {
    RemoveChildView(children_.back().get());
  }
}

bool AXVirtualView::Contains(const AXVirtualView* view) const {
  DCHECK(view);
  for (const AXVirtualView* v = view; v; v = v->virtual_parent_view_) {
    if (v == this) {
      return true;
    }
  }
  return false;
}

std::optional<size_t> AXVirtualView::GetIndexOf(
    const AXVirtualView* view) const {
  DCHECK(view);
  const auto iter =
      std::ranges::find(children_, view, &std::unique_ptr<AXVirtualView>::get);
  return iter != children_.end()
             ? std::make_optional(static_cast<size_t>(iter - children_.begin()))
             : std::nullopt;
}

const char* AXVirtualView::GetViewClassName() const {
  return kViewClassName;
}

gfx::NativeViewAccessible AXVirtualView::GetNativeObject() const {
  DCHECK(ax_platform_node_);
  return ax_platform_node_->GetNativeViewAccessible();
}

Widget* AXVirtualView::GetWidget() const {
  View* owner_view = GetOwnerView();
  if (owner_view) {
    return owner_view->GetWidget();
  }
  return nullptr;
}

ViewAccessibility* AXVirtualView::GetViewAccessibilityParent() const {
  if (parent_view_) {
    return parent_view_;
  }
  if (virtual_parent_view_) {
    return virtual_parent_view_;
  }
  // This virtual view hasn't been added to a parent view yet.
  return nullptr;
}

std::string AXVirtualView::GetDebugString() const {
  View* owner_view = GetOwnerView();
  if (!owner_view) {
    return std::string("Virtual view with no owner view");
  }
  return base::StrCat({"Virtual view child of ", owner_view->GetClassName()});
}

void AXVirtualView::NotifyEvent(ax::mojom::Event event_type,
                                bool send_native_event) {
  // If `ready_to_notify_events_` is false, it means we are initializing
  // property values. In this specific case, we do not want to notify platform
  // assistive technologies that a property has changed.
  if (!ready_to_notify_events_) {
    return;
  }

  DCHECK(ax_platform_node_);
  if (event_type == ax::mojom::Event::kAlert) {
    CHECK(ui::IsAlert(GetRole()))
        << "On some platforms, the alert event does not work correctly unless "
           "it is fired on an object with an alert role. Role was "
        << GetRole();
  }
  if (GetOwnerView()) {
    const ViewAccessibility::AccessibilityEventsCallback& events_callback =
        GetOwnerView()->GetViewAccessibility().accessibility_events_callback();
    if (events_callback) {
      events_callback.Run(this, event_type);
    }
  }

  // This is used on platforms that have a native accessibility API.
  ax_platform_node_->NotifyAccessibilityEvent(event_type);

  // This is used on platforms that don't have a native accessibility API.
  AXUpdateNotifier::Get()->NotifyVirtualViewEvent(this, event_type);
}

void AXVirtualView::NotifyDataChanged() {
  AXUpdateNotifier::Get()->NotifyVirtualViewDataChanged(this);
}

// ui::AXPlatformNodeDelegate

const ui::AXNodeData& AXVirtualView::GetData() const {
  return data();
}

size_t AXVirtualView::GetChildCount() const {
  size_t count = 0;
  for (const std::unique_ptr<AXVirtualView>& child : children_) {
    if (child->IsIgnored()) {
      count += child->GetChildCount();
    } else {
      ++count;
    }
  }
  return count;
}

gfx::NativeViewAccessible AXVirtualView::ChildAtIndex(size_t index) const {
  DCHECK_LT(index, GetChildCount())
      << "|index| should be less than the child count.";

  for (const std::unique_ptr<AXVirtualView>& child : children_) {
    if (child->IsIgnored()) {
      size_t child_count = child->GetChildCount();
      if (index < child_count) {
        return child->ChildAtIndex(index);
      }
      index -= child_count;
    } else {
      if (index == 0) {
        return child->GetNativeObject();
      }
      --index;
    }
  }

  NOTREACHED() << "|index| should be less than the child count.";
}

#if !BUILDFLAG(IS_MAC)
gfx::NativeViewAccessible AXVirtualView::GetNSWindow() {
  NOTREACHED();
}
#endif

gfx::NativeViewAccessible AXVirtualView::GetNativeViewAccessible() {
  return GetNativeObject();
}

gfx::NativeViewAccessible AXVirtualView::GetParent() const {
  if (parent_view_) {
    if (!parent_view_->GetIsIgnored()) {
      return parent_view_->GetNativeObject();
    }
    return GetDelegate()->GetParent();
  }

  if (virtual_parent_view_) {
    if (virtual_parent_view_->IsIgnored()) {
      return virtual_parent_view_->GetParent();
    }
    return virtual_parent_view_->GetNativeObject();
  }

  // This virtual view hasn't been added to a parent view yet.
  return gfx::NativeViewAccessible();
}

gfx::Rect AXVirtualView::GetBoundsRect(
    const ui::AXCoordinateSystem coordinate_system,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  // We could optionally add clipping here if ever needed.
  // TODO(nektar): Implement bounds that are relative to the parent.
  gfx::Rect bounds = gfx::ToEnclosingRect(GetData().relative_bounds.bounds);
  View* owner_view = GetOwnerView();
  if (owner_view && owner_view->GetWidget()) {
    View::ConvertRectToScreen(owner_view, &bounds);
  }
  switch (coordinate_system) {
    case ui::AXCoordinateSystem::kScreenDIPs:
      return bounds;
    case ui::AXCoordinateSystem::kScreenPhysicalPixels: {
      float scale_factor = 1.0;
      if (auto* widget = GetWidget()) {
        gfx::NativeView native_view = widget->GetNativeView();
        if (native_view) {
          scale_factor = ui::GetScaleFactorForNativeView(native_view);
        }
      }
      return gfx::ScaleToEnclosingRect(bounds, scale_factor);
    }
    case ui::AXCoordinateSystem::kRootFrame:
    case ui::AXCoordinateSystem::kFrame:
      NOTIMPLEMENTED();
      return gfx::Rect();
  }
}

gfx::NativeViewAccessible AXVirtualView::HitTestSync(
    int screen_physical_pixel_x,
    int screen_physical_pixel_y) const {
  if (GetData().IsInvisible()) {
    return gfx::NativeViewAccessible();
  }

  // Check if the point is within any of the virtual children of this view.
  // AXVirtualView's HitTestSync is a recursive function that will return the
  // deepest child, since it does not support relative bounds.
  // Search the greater indices first, since they're on top in the z-order.
  for (const std::unique_ptr<AXVirtualView>& child :
       base::Reversed(children_)) {
    gfx::NativeViewAccessible result =
        child->HitTestSync(screen_physical_pixel_x, screen_physical_pixel_y);
    if (result) {
      return result;
    }
  }

  // If it's not inside any of our virtual children, and it's inside the bounds
  // of this virtual view, then it's inside this virtual view.
  gfx::Rect bounds_in_screen_physical_pixels =
      GetBoundsRect(ui::AXCoordinateSystem::kScreenPhysicalPixels,
                    ui::AXClippingBehavior::kUnclipped);
  if (bounds_in_screen_physical_pixels.Contains(
          static_cast<float>(screen_physical_pixel_x),
          static_cast<float>(screen_physical_pixel_y)) &&
      !IsIgnored()) {
    return GetNativeObject();
  }

  return gfx::NativeViewAccessible();
}

gfx::NativeViewAccessible AXVirtualView::GetFocus() const {
  View* owner_view = GetOwnerView();
  if (owner_view) {
    if (!(owner_view->HasFocus())) {
      return gfx::NativeViewAccessible();
    }
    return owner_view->GetViewAccessibility().GetFocusedDescendant();
  }

  // This virtual view hasn't been added to a parent view yet.
  return gfx::NativeViewAccessible();
}

ui::AXPlatformNode* AXVirtualView::GetFromNodeID(int32_t id) {
  AXVirtualView* virtual_view = GetFromId(id);
  if (virtual_view) {
    return virtual_view->ax_platform_node();
  }
  return nullptr;
}

bool AXVirtualView::AccessibilityPerformAction(const ui::AXActionData& data) {
  bool result = false;
  if (ViewAccessibility::data().HasAction(data.action)) {
    result = HandleAccessibleAction(data);
  }
  if (!result && GetOwnerView()) {
    return HandleAccessibleActionInOwnerView(data);
  }
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

ui::AXPlatformNodeId AXVirtualView::GetUniqueId() const {
  // The unique ID is held in the `ViewAccessibility`.
  return ViewAccessibility::GetUniqueId();
}

// Virtual views need to implement this function in order for accessibility
// events to be routed correctly.
gfx::AcceleratedWidget AXVirtualView::GetTargetForNativeAccessibilityEvent() {
#if BUILDFLAG(IS_WIN)
  if (GetOwnerView()) {
    return HWNDForView(GetOwnerView());
  }
#endif
  return gfx::kNullAcceleratedWidget;
}

std::vector<int32_t> AXVirtualView::GetColHeaderNodeIds() const {
  return GetDelegate()->GetColHeaderNodeIds();
}

std::vector<int32_t> AXVirtualView::GetColHeaderNodeIds(int col_index) const {
  return GetDelegate()->GetColHeaderNodeIds(col_index);
}

std::optional<int32_t> AXVirtualView::GetCellId(int row_index,
                                                int col_index) const {
  return GetDelegate()->GetCellId(row_index, col_index);
}

bool AXVirtualView::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  if (!GetOwnerView()) {
    return false;
  }

  switch (action_data.action) {
    case ax::mojom::Action::kShowContextMenu: {
      const gfx::Rect screen_bounds = GetBoundsRect(
          ui::AXCoordinateSystem::kScreenDIPs, ui::AXClippingBehavior::kClipped,
          nullptr /* offscreen_result */);
      if (!screen_bounds.IsEmpty()) {
        GetOwnerView()->ShowContextMenu(screen_bounds.CenterPoint(),
                                        ui::mojom::MenuSourceType::kKeyboard);
        return true;
      }
      break;
    }

    default:
      break;
  }

  return HandleAccessibleActionInOwnerView(action_data);
}

bool AXVirtualView::HandleAccessibleActionInOwnerView(
    const ui::AXActionData& action_data) {
  DCHECK(GetOwnerView());
  // Save the node id so that the owner view can determine which virtual view
  // is being targeted for action.
  ui::AXActionData forwarded_action_data = action_data;
  forwarded_action_data.target_node_id = GetData().id;
  return GetOwnerView()->HandleAccessibleAction(forwarded_action_data);
}

void AXVirtualView::set_cache(AXAuraObjCache* cache) {
#if defined(USE_AURA)
  if (ax_aura_obj_cache_ && cache) {
    ax_aura_obj_cache_->Remove(this);
  }
#endif

  ax_aura_obj_cache_ = cache;
}

View* AXVirtualView::GetOwnerView() const {
  if (parent_view_) {
    return parent_view_->view();
  }

  if (virtual_parent_view_) {
    return virtual_parent_view_->GetOwnerView();
  }

  // This virtual view hasn't been added to a parent view yet.
  return nullptr;
}

ViewAXPlatformNodeDelegate* AXVirtualView::GetDelegate() const {
  DCHECK(GetOwnerView());
#if BUILDFLAG(HAS_NATIVE_ACCESSIBILITY)
  return static_cast<ViewAXPlatformNodeDelegate*>(
      &GetOwnerView()->GetViewAccessibility());
#else
  return nullptr;
#endif
}

AXVirtualViewWrapper* AXVirtualView::GetOrCreateWrapper(
    views::AXAuraObjCache* cache) {
#if defined(USE_AURA)
  return static_cast<AXVirtualViewWrapper*>(cache->GetOrCreate(this));
#else
  return nullptr;
#endif
}

void AXVirtualView::PruneVirtualSubtree() {
  pruned_ = true;
  UpdateIgnoredState();
  for (auto& child : children()) {
    child->PruneVirtualSubtree();
  }
}

void AXVirtualView::UnpruneVirtualSubtree() {
  pruned_ = false;
  UpdateIgnoredState();
  for (auto& child : children()) {
    child->UnpruneVirtualSubtree();
  }
}

void AXVirtualView::ForceSetIsFocusable(bool focusable) {
  should_be_focusable_ = focusable;

  // To align with previous behavior, if a virtual view is set to explicitly
  // focusable, we must make sure it is not ignored.
  if (focusable) {
    data_.RemoveState(ax::mojom::State::kIgnored);
  } else {
    if (should_be_ignored_) {
      data_.AddState(ax::mojom::State::kIgnored);
    }
  }

  UpdateFocusableState();
}

void AXVirtualView::ResetIsFocusable() {
  should_be_focusable_ = std::nullopt;
  UpdateFocusableState();
}

void AXVirtualView::OnViewHasNewAncestor(bool ancestor_focusable) {
  // We need to make sure that we are propagating the right values down the
  // recursive calls. For the invisible state, this means we look at the direct
  // parent, rather than the new ancestor, which in subsequent recursive calls
  // could be a root of an entire tree that is getting reparented. This is
  // because if at some point during the recursion, the parent is invisible, it
  // should affect its descendants, even if the new ancestor is not. For
  // example, if we have a tree like this: A (visible)
  //   B (invisible)
  // and then a separate tree:
  // C (invisible)
  //   D (invisible by inheritance of C)
  // and then we reparent C to be a child of A:
  // A (visible)
  //   B (invisible)
  //   C (invisible)
  //     D (invisible by inheritance of C)
  // Even though `A` is visible ( A would be the new ancestor), we need to make
  // sure that during the recursion, we don't mark `D` as visible, since it's
  // parent is invisible.
  bool parent_invisible = false;
  if (parent_view()) {
    CHECK(parent_view()->view());
    parent_invisible = parent_view()->is_invisible_by_inheritance() ||
                       !parent_view()->view()->GetVisible();
  } else {
    CHECK(virtual_parent_view());
    // We only need to check if the parent view is drawn, because the accessible
    // invisible state does not get propagated down the hierarchy.
    parent_invisible = !virtual_parent_view()->parent_view_is_drawn();
  }

  parent_view_is_drawn_ = !parent_invisible;

  UpdateInvisibleState();

  // We only want to propagate the `ancestor_focusable` value if it's true. This
  // is because if this view is unfocusable, and it gets added to a tree with a
  // focusable ancestor, it should now be marked as ignored. However, being
  // added to a tree with an unfocusable ancestor doesn't affect the ignored
  // state of this view or its descendants.
  if (ancestor_focusable) {
    SetHasFocusableAncestor(ancestor_focusable);
  }

  UpdateReadyToNotifyEvents();
  for (auto& child : children_) {
    child->OnViewHasNewAncestor(ancestor_focusable);
  }
}

void AXVirtualView::UpdateFocusableState() {
  bool is_focusable =
      (GetIsEnabled() && !data().HasState(ax::mojom::State::kInvisible) &&
       !ViewAccessibility::GetIsIgnored());

  if (should_be_focusable_.has_value()) {
    is_focusable = should_be_focusable_.value();
  }

  SetState(ax::mojom::State::kFocusable, is_focusable);
}

void AXVirtualView::UpdateInvisibleState() {
  bool is_invisible = !parent_view_is_drawn_ || should_be_invisible_;
  SetState(ax::mojom::State::kInvisible, is_invisible);
  UpdateFocusableState();
}

void AXVirtualView::OnWidgetClosing(Widget* widget) {
  // The RootView's ViewAccessibility should be the only registered
  // WidgetObserver.
  CHECK_EQ(GetOwnerView(), widget->GetRootView());
  SetWidgetClosedRecursive(widget, true);
}

void AXVirtualView::OnWidgetDestroyed(Widget* widget) {
  // The RootView's ViewAccessibility should be the only registered
  // WidgetObserver.
  CHECK(widget->GetRootView());
  CHECK_EQ(GetOwnerView(), widget->GetRootView());
  SetWidgetClosedRecursive(widget, true);
}

void AXVirtualView::OnWidgetUpdated(Widget* widget, Widget* old_widget) {
  CHECK(widget);
  DCHECK_EQ(widget, GetWidget());
  if (widget == old_widget) {
    return;
  }

  // There's a chance we are reparenting a view that was previously a root
  // view in another widget, if so we need to remove it as an observer of the
  // old widget.
  if (old_widget && old_widget != widget) {
    old_widget->RemoveObserver(this);
  }

  // If we have already marked `is_widget_closed_` as true, then there's a
  // chance that the view was reparented to a non-closed widget. If so, we must
  // update `is_widget_closed_` in case the new widget is not closed.
  SetWidgetClosedRecursive(widget, widget->IsClosed());
}

void AXVirtualView::UpdateIgnoredState() {
// TODO(crbug.com/371237539): In ChromeOS, its not an expectation that being
// a view unfocusable descendant of a focusable ancestor will make the view
// ignored.
#if !BUILDFLAG(IS_CHROMEOS)
  bool is_ignored =
      should_be_ignored_ || pruned_ ||
      GetCachedRole() == ax::mojom::Role::kNone ||
      (has_focusable_ancestor_ &&
       (should_be_focusable_.has_value() && !should_be_focusable_.value()));

  if (should_be_focusable_.has_value()) {
    is_ignored = is_ignored && !should_be_focusable_.value();
  }
#else
  bool is_ignored =
      should_be_ignored_ || pruned_ || data().role == ax::mojom::Role::kNone;
#endif  // !BUILDFLAG(IS_CHROMEOS)
  SetState(ax::mojom::State::kIgnored, is_ignored);
  UpdateFocusableState();
}

void AXVirtualView::UpdateReadyToNotifyEvents() {
  auto* parent = parent_view() ? parent_view() : virtual_parent_view();
  if (parent && parent->IsReadyToNotifyEvents()) {
    SetReadyToNotifyEvents();
  }
}

void AXVirtualView::UpdateParentViewIsDrawnRecursive(
    const views::View* initial_view,
    bool parent_view_is_drawn) {
  parent_view_is_drawn_ = parent_view_is_drawn;
  UpdateInvisibleState();

  // Now we do the same for any virtual children.
  for (auto& child : children_) {
    child->UpdateParentViewIsDrawnRecursive(initial_view, parent_view_is_drawn);
  }
}

void AXVirtualView::SetIsEnabled(bool enabled) {
  if (enabled == GetIsEnabled()) {
    return;
  }

  if (!enabled) {
    data_.SetRestriction(ax::mojom::Restriction::kDisabled);
  } else if (data_.GetRestriction() == ax::mojom::Restriction::kDisabled) {
    // Take into account the possibility that the View is marked as readonly
    // but enabled. In other words, we can't just remove all restrictions,
    // unless the View is explicitly marked as disabled. Note that readonly is
    // another restriction state in addition to enabled and disabled, (see
    // `ax::mojom::Restriction`).
    data_.SetRestriction(ax::mojom::Restriction::kNone);
  }
}

void AXVirtualView::SetShowContextMenu(bool show_context_menu) {
  if (show_context_menu) {
    data_.AddAction(ax::mojom::Action::kShowContextMenu);
  } else {
    data_.RemoveAction(ax::mojom::Action::kShowContextMenu);
  }
}

void AXVirtualView::SetIsEnabledRecursive(bool enabled) {
  SetIsEnabled(enabled);
  for (auto& child : children_) {
    child->SetIsEnabledRecursive(enabled);
  }
}

void AXVirtualView::SetShowContextMenuRecursive(bool show_context_menu) {
  SetShowContextMenu(show_context_menu);
  for (auto& child : children_) {
    child->SetShowContextMenuRecursive(show_context_menu);
  }
}
}  // namespace views
