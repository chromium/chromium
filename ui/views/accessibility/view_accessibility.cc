// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_accessibility.h"

#include <algorithm>
#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/base/buildflags.h"
#include "ui/views/view.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

bool IsValidRoleForViews(ax::mojom::Role role) {
  switch (role) {
    // These roles all have special meaning and shouldn't ever be
    // set on a View.
    case ax::mojom::Role::kDesktop:
    case ax::mojom::Role::kNone:
    case ax::mojom::Role::kRootWebArea:
    case ax::mojom::Role::kSvgRoot:
    case ax::mojom::Role::kUnknown:
    case ax::mojom::Role::kWebArea:
      return false;

    default:
      return true;
  }
}

}  // namespace

#if !BUILDFLAG_INTERNAL_HAS_NATIVE_ACCESSIBILITY()
// static
std::unique_ptr<ViewAccessibility> ViewAccessibility::Create(View* view) {
  // Cannot use std::make_unique because constructor is protected.
  return base::WrapUnique(new ViewAccessibility(view));
}
#endif

ViewAccessibility::ViewAccessibility(View* view)
    : view_(view),
      focused_virtual_child_(nullptr),
      is_leaf_(false),
      is_ignored_(false) {}

ViewAccessibility::~ViewAccessibility() = default;

void ViewAccessibility::AddVirtualChildView(
    std::unique_ptr<AXVirtualView> virtual_view) {
  AddVirtualChildViewAt(std::move(virtual_view), int{virtual_children_.size()});
}

void ViewAccessibility::AddVirtualChildViewAt(
    std::unique_ptr<AXVirtualView> virtual_view,
    int index) {
  DCHECK(virtual_view);
  DCHECK_GE(index, 0);
  DCHECK_LE(size_t{index}, virtual_children_.size());

  if (virtual_view->parent_view() == this)
    return;
  DCHECK(!virtual_view->parent_view()) << "This |view| already has a View "
                                          "parent. Call RemoveVirtualChildView "
                                          "first.";
  DCHECK(!virtual_view->virtual_parent_view()) << "This |view| already has an "
                                                  "AXVirtualView parent. Call "
                                                  "RemoveChildView first.";
  virtual_view->set_parent_view(this);
  auto insert_iterator = virtual_children_.begin() + index;
  virtual_children_.insert(insert_iterator, std::move(virtual_view));
}

std::unique_ptr<AXVirtualView> ViewAccessibility::RemoveVirtualChildView(
    AXVirtualView* virtual_view) {
  DCHECK(virtual_view);
  int cur_index = GetIndexOf(virtual_view);
  if (cur_index < 0)
    return {};

  std::unique_ptr<AXVirtualView> child =
      std::move(virtual_children_[cur_index]);
  virtual_children_.erase(virtual_children_.begin() + cur_index);
  child->set_parent_view(nullptr);
  child->UnsetPopulateDataCallback();
  if (focused_virtual_child_ && child->Contains(focused_virtual_child_))
    OverrideFocus(nullptr);
  return child;
}

void ViewAccessibility::RemoveAllVirtualChildViews() {
  while (!virtual_children_.empty())
    RemoveVirtualChildView(virtual_children_.back().get());
}

bool ViewAccessibility::Contains(const AXVirtualView* virtual_view) const {
  DCHECK(virtual_view);
  for (const auto& virtual_child : virtual_children_) {
    // AXVirtualView::Contains() also checks if the provided virtual view is the
    // same as |this|.
    if (virtual_child->Contains(virtual_view))
      return true;
  }
  return false;
}

int ViewAccessibility::GetIndexOf(const AXVirtualView* virtual_view) const {
  DCHECK(virtual_view);
  const auto iter =
      std::find_if(virtual_children_.begin(), virtual_children_.end(),
                   [virtual_view](const auto& child) {
                     return child.get() == virtual_view;
                   });
  return iter != virtual_children_.end()
             ? static_cast<int>(iter - virtual_children_.begin())
             : -1;
}

const ui::AXUniqueId& ViewAccessibility::GetUniqueId() const {
  return unique_id_;
}

bool ViewAccessibility::IsLeaf() const {
  return is_leaf_;
}

void ViewAccessibility::GetAccessibleNodeData(ui::AXNodeData* data) const {
  data->id = GetUniqueId().Get();

  // Views may misbehave if their widget is closed; return an unknown role
  // rather than possibly crashing.
  const views::Widget* widget = view_->GetWidget();
  if (!widget || !widget->widget_delegate() || widget->IsClosed()) {
    data->role = ax::mojom::Role::kUnknown;
    data->SetRestriction(ax::mojom::Restriction::kDisabled);
    return;
  }

  view_->GetAccessibleNodeData(data);
  if (custom_data_.role != ax::mojom::Role::kUnknown)
    data->role = custom_data_.role;
  if (data->role == ax::mojom::Role::kAlertDialog) {
    // When an alert dialog is used, indicate this with xml-roles. This helps
    // JAWS understand that it's a dialog and not just an ordinary alert, even
    // though xml-roles is normally used to expose ARIA roles in web content.
    // Specifically, this enables the JAWS Insert+T read window title command.
    // Note: if an alert has focusable descendants such as buttons, it should
    // use kAlertDialog, not kAlert.
    data->AddStringAttribute(ax::mojom::StringAttribute::kRole, "alertdialog");
  }

  if (custom_data_.HasStringAttribute(ax::mojom::StringAttribute::kName)) {
    data->SetName(
        custom_data_.GetStringAttribute(ax::mojom::StringAttribute::kName));
  }

  if (custom_data_.HasStringAttribute(
          ax::mojom::StringAttribute::kDescription)) {
    data->SetDescription(custom_data_.GetStringAttribute(
        ax::mojom::StringAttribute::kDescription));
  }

  if (custom_data_.GetHasPopup() != ax::mojom::HasPopup::kFalse)
    data->SetHasPopup(custom_data_.GetHasPopup());

  static const ax::mojom::IntAttribute kOverridableIntAttributes[]{
      ax::mojom::IntAttribute::kPosInSet,
      ax::mojom::IntAttribute::kSetSize,
  };
  for (auto attribute : kOverridableIntAttributes) {
    if (custom_data_.HasIntAttribute(attribute))
      data->AddIntAttribute(attribute, custom_data_.GetIntAttribute(attribute));
  }

  static const ax::mojom::IntListAttribute kOverridableIntListAttributes[]{
      ax::mojom::IntListAttribute::kDescribedbyIds,
  };
  for (auto attribute : kOverridableIntListAttributes) {
    if (custom_data_.HasIntListAttribute(attribute))
      data->AddIntListAttribute(attribute,
                                custom_data_.GetIntListAttribute(attribute));
  }

  if (!data->HasStringAttribute(ax::mojom::StringAttribute::kDescription)) {
    base::string16 tooltip = view_->GetTooltipText(gfx::Point());
    // Some screen readers announce the accessible description right after the
    // accessible name. Only use the tooltip as the accessible description if
    // it's different from the name, otherwise users might be puzzled as to why
    // their screen reader is announcing the same thing twice.
    if (tooltip !=
        data->GetString16Attribute(ax::mojom::StringAttribute::kName)) {
      data->AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                               base::UTF16ToUTF8(tooltip));
    }
  }

  data->relative_bounds.bounds = gfx::RectF(view_->GetBoundsInScreen());
  if (!custom_data_.relative_bounds.bounds.IsEmpty())
    data->relative_bounds.bounds = custom_data_.relative_bounds.bounds;

  data->AddStringAttribute(ax::mojom::StringAttribute::kClassName,
                           view_->GetClassName());

  if (IsIgnored()) {
    // Prevent screen readers from navigating to or speaking ignored nodes.
    data->AddState(ax::mojom::State::kInvisible);
    data->AddState(ax::mojom::State::kIgnored);
    data->role = ax::mojom::Role::kIgnored;
    return;
  }

  if (view_->IsAccessibilityFocusable() && !focused_virtual_child_)
    data->AddState(ax::mojom::State::kFocusable);

  if (!view_->GetEnabled())
    data->SetRestriction(ax::mojom::Restriction::kDisabled);

  if (!view_->GetVisible() && data->role != ax::mojom::Role::kAlert)
    data->AddState(ax::mojom::State::kInvisible);

  if (view_->context_menu_controller())
    data->AddAction(ax::mojom::Action::kShowContextMenu);
}

void ViewAccessibility::OverrideFocus(AXVirtualView* virtual_view) {
  DCHECK(!virtual_view || Contains(virtual_view))
      << "|virtual_view| must be nullptr or a descendant of this view.";
  focused_virtual_child_ = virtual_view;

  if (view_->HasFocus()) {
    if (focused_virtual_child_) {
      focused_virtual_child_->NotifyAccessibilityEvent(
          ax::mojom::Event::kFocus);
    } else {
      view_->NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
    }
  }
}

void ViewAccessibility::SetPopupFocusOverride() {}

void ViewAccessibility::EndPopupFocusOverride() {}

bool ViewAccessibility::IsFocusedForTesting() {
  return view_->HasFocus() && !focused_virtual_child_;
}

void ViewAccessibility::OverrideRole(const ax::mojom::Role role) {
  DCHECK(IsValidRoleForViews(role)) << "Invalid role for Views.";
  custom_data_.role = role;
}

void ViewAccessibility::OverrideName(const std::string& name) {
  custom_data_.SetName(name);
}

void ViewAccessibility::OverrideName(const base::string16& name) {
  custom_data_.SetName(name);
}

void ViewAccessibility::OverrideDescription(const std::string& description) {
  custom_data_.SetDescription(description);
}

void ViewAccessibility::OverrideDescription(const base::string16& description) {
  custom_data_.SetDescription(description);
}

void ViewAccessibility::OverrideIsLeaf(bool value) {
  is_leaf_ = value;
}

void ViewAccessibility::OverrideIsIgnored(bool value) {
  is_ignored_ = value;
}

void ViewAccessibility::OverrideBounds(const gfx::RectF& bounds) {
  custom_data_.relative_bounds.bounds = bounds;
}

void ViewAccessibility::OverrideDescribedBy(View* described_by_view) {
  int described_by_id =
      described_by_view->GetViewAccessibility().GetUniqueId().Get();
  custom_data_.AddIntListAttribute(ax::mojom::IntListAttribute::kDescribedbyIds,
                                   {described_by_id});
}

void ViewAccessibility::OverrideHasPopup(const ax::mojom::HasPopup has_popup) {
  custom_data_.SetHasPopup(has_popup);
}

void ViewAccessibility::OverridePosInSet(int pos_in_set, int set_size) {
  custom_data_.AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, pos_in_set);
  custom_data_.AddIntAttribute(ax::mojom::IntAttribute::kSetSize, set_size);
}

void ViewAccessibility::OverrideNextFocus(Widget* widget) {
  next_focus_ = widget;
}

void ViewAccessibility::OverridePreviousFocus(Widget* widget) {
  previous_focus_ = widget;
}

Widget* ViewAccessibility::GetNextFocus() {
  return next_focus_;
}

Widget* ViewAccessibility::GetPreviousFocus() {
  return previous_focus_;
}

gfx::NativeViewAccessible ViewAccessibility::GetNativeObject() const {
  return nullptr;
}

void ViewAccessibility::NotifyAccessibilityEvent(ax::mojom::Event event_type) {
  // On certain platforms, e.g. Chrome OS, we don't create any
  // AXPlatformDelegates, so the base method in this file would be called.
  if (accessibility_events_callback_)
    accessibility_events_callback_.Run(nullptr, event_type);
}

void ViewAccessibility::AnnounceText(const base::string16& text) {
  Widget* const widget = view_->GetWidget();
  if (!widget)
    return;
  auto* const root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  if (!root_view)
    return;
  root_view->AnnounceText(text);
}

gfx::NativeViewAccessible ViewAccessibility::GetFocusedDescendant() {
  if (focused_virtual_child_)
    return focused_virtual_child_->GetNativeObject();
  return view_->GetNativeViewAccessible();
}

void ViewAccessibility::FireFocusAfterMenuClose() {
  NotifyAccessibilityEvent(ax::mojom::Event::kFocusAfterMenuClose);
}

const ViewAccessibility::AccessibilityEventsCallback&
ViewAccessibility::accessibility_events_callback() const {
  return accessibility_events_callback_;
}

void ViewAccessibility::set_accessibility_events_callback(
    ViewAccessibility::AccessibilityEventsCallback callback) {
  accessibility_events_callback_ = std::move(callback);
}

}  // namespace views
