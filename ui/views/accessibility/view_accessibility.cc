// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_accessibility.h"

#include <algorithm>
#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/base/buildflags.h"
#include "ui/views/accessibility/views_ax_tree_manager.h"
#include "ui/views/accessibility/widget_ax_tree_id_map.h"
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
    case ax::mojom::Role::kDocument:  // Used for ARIA role="document".
    case ax::mojom::Role::kIframe:
    case ax::mojom::Role::kIframePresentational:
    case ax::mojom::Role::kNone:
    case ax::mojom::Role::kPdfRoot:
    case ax::mojom::Role::kPortal:
    case ax::mojom::Role::kRootWebArea:
    case ax::mojom::Role::kSvgRoot:
    case ax::mojom::Role::kUnknown:
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
      is_ignored_(false) {
#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  if (features::IsAccessibilityTreeForViewsEnabled()) {
    Widget* widget = view_->GetWidget();
    if (widget && widget->is_top_level() &&
        !WidgetAXTreeIDMap::GetInstance().HasWidget(widget)) {
      View* root_view = static_cast<View*>(widget->GetRootView());
      if (root_view && root_view == view) {
        ax_tree_manager_ = std::make_unique<views::ViewsAXTreeManager>(widget);
      }
    }
  }
#endif
}

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

void ViewAccessibility::GetAccessibleNodeData(ui::AXNodeData* data) const {
  data->id = GetUniqueId().Get();
  data->AddStringAttribute(ax::mojom::StringAttribute::kClassName,
                           view_->GetClassName());

  // Views may misbehave if their widget is closed; return an unknown role
  // rather than possibly crashing.
  const views::Widget* widget = view_->GetWidget();
  if (!widget || !widget->widget_delegate() || widget->IsClosed()) {
    data->role = ax::mojom::Role::kUnknown;
    data->SetRestriction(ax::mojom::Restriction::kDisabled);

    // Ordinarily, a view cannot be focusable if its widget has already closed.
    // So, it would have been appropriate to set the focusable state to false in
    // this particular case. However, the `FocusManager` may sometimes try to
    // retrieve the focusable state of this view via
    // `View::IsAccessibilityFocusable()`, even after this view's widget has
    // been closed. Returning the wrong result might cause a crash, because the
    // focus manager might be expecting the result to be the same regardless of
    // the state of the view's widget.
    if (ViewAccessibility::IsAccessibilityFocusable())
      data->AddState(ax::mojom::State::kFocusable);
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

  static constexpr ax::mojom::IntAttribute kOverridableIntAttributes[]{
      ax::mojom::IntAttribute::kPosInSet,
      ax::mojom::IntAttribute::kSetSize,
  };
  for (auto attribute : kOverridableIntAttributes) {
    if (custom_data_.HasIntAttribute(attribute))
      data->AddIntAttribute(attribute, custom_data_.GetIntAttribute(attribute));
  }

  static constexpr ax::mojom::IntListAttribute kOverridableIntListAttributes[]{
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
      data->SetDescription(base::UTF16ToUTF8(tooltip));
    }
  }

  data->relative_bounds.bounds = gfx::RectF(view_->GetBoundsInScreen());
  if (!custom_data_.relative_bounds.bounds.IsEmpty())
    data->relative_bounds.bounds = custom_data_.relative_bounds.bounds;

  // We need to add the ignored state to all ignored Views, similar to how Blink
  // exposes ignored DOM nodes. Calling AXNodeData::IsIgnored() would also check
  // if the role is in the list of roles that are inherently ignored.
  // Furthermore, we add the ignored state if this View is a descendant of a
  // leaf View. We call this class's "IsChildOfLeaf" method instead of the one
  // in our platform specific subclass because subclasses determine if a node is
  // a leaf by (among other things) counting the number of unignored children,
  // which would create a circular definition of the ignored state.
  if (is_ignored_ || data->IsIgnored() || ViewAccessibility::IsChildOfLeaf())
    data->AddState(ax::mojom::State::kIgnored);

  if (ViewAccessibility::IsAccessibilityFocusable())
    data->AddState(ax::mojom::State::kFocusable);

  if (is_enabled_) {
    if (*is_enabled_) {
      // Take into account the possibility that the View is marked as readonly
      // but enabled. In other words, we can't just remove all restrictions,
      // unless the View is explicitly marked as disabled. Note that readonly is
      // another restriction state in addition to enabled and disabled, (see
      // ax::mojom::Restriction).
      if (data->GetRestriction() == ax::mojom::Restriction::kDisabled)
        data->SetRestriction(ax::mojom::Restriction::kNone);
    } else {
      data->SetRestriction(ax::mojom::Restriction::kDisabled);
    }
  } else if (!view_->GetEnabled()) {
    data->SetRestriction(ax::mojom::Restriction::kDisabled);
  }

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

bool ViewAccessibility::IsAccessibilityFocusable() const {
  // Descendants of leaf nodes should not be reported as focusable, because all
  // such descendants are not exposed to the accessibility APIs of any platform.
  // (See `AXNode::IsLeaf()` for more information.) We avoid calling
  // `IsChildOfLeaf()` for performance reasons, because `FocusManager` makes use
  // of this method, which means that it would be called frequently. However,
  // since all descendants of leaf nodes are ignored by default, and since our
  // testing framework enforces the condition that all ignored nodes should not
  // be focusable, if there is test coverage, such a situation will cause a test
  // failure.
  return view_->GetFocusBehavior() != View::FocusBehavior::NEVER &&
         ViewAccessibility::IsAccessibilityEnabled() && view_->IsDrawn() &&
         !is_ignored_;
}

bool ViewAccessibility::IsFocusedForTesting() const {
  return view_->HasFocus() && !focused_virtual_child_;
}

void ViewAccessibility::SetPopupFocusOverride() {
  NOTIMPLEMENTED();
}

void ViewAccessibility::EndPopupFocusOverride() {
  NOTIMPLEMENTED();
}

void ViewAccessibility::FireFocusAfterMenuClose() {
  NotifyAccessibilityEvent(ax::mojom::Event::kFocusAfterMenuClose);
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

bool ViewAccessibility::IsLeaf() const {
  return is_leaf_;
}

bool ViewAccessibility::IsChildOfLeaf() const {
  // Note to future developers: This method is called from
  // "GetAccessibleNodeData". We should avoid calling any methods in any of our
  // subclasses that might try and retrieve our AXNodeData, because this will
  // cause an infinite loop.
  // TODO(crbug.com/1100047): Make this method non-virtual and delete it from
  // all subclasses.
  if (const View* parent_view = view_->parent()) {
    const ViewAccessibility& view_accessibility =
        parent_view->GetViewAccessibility();
    if (view_accessibility.ViewAccessibility::IsLeaf())
      return true;
    return view_accessibility.ViewAccessibility::IsChildOfLeaf();
  }
  return false;
}

void ViewAccessibility::OverrideIsIgnored(bool value) {
  is_ignored_ = value;
}

bool ViewAccessibility::IsIgnored() const {
  // TODO(nektar): Make this method non-virtual and implement as follows:
  // ui::AXNodeData out_data;
  // GetAccessibleNodeData(&out_data);
  // return out_data.IsIgnored();
  return is_ignored_;
}

void ViewAccessibility::OverrideIsEnabled(bool enabled) {
  // Cannot store this value in `custom_data_` because
  // `AXNodeData::AddIntAttribute` will DCHECK if you add an IntAttribute that
  // is equal to kNone. Adding an IntAttribute that is equal to kNone is
  // ambiguous, since it is unclear what would be the difference between doing
  // this and not adding the attribute at all.
  is_enabled_ = enabled;
}

bool ViewAccessibility::IsAccessibilityEnabled() const {
  if (is_enabled_)
    return *is_enabled_;
  return view_->GetEnabled();
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

Widget* ViewAccessibility::GetNextFocus() const {
  return next_focus_;
}

Widget* ViewAccessibility::GetPreviousFocus() const {
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

const ui::AXUniqueId& ViewAccessibility::GetUniqueId() const {
  return unique_id_;
}

ViewsAXTreeManager* ViewAccessibility::AXTreeManager() const {
  ViewsAXTreeManager* manager = nullptr;
#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  Widget* widget = view_->GetWidget();

  // Don't return managers for closing Widgets.
  if (!widget || !widget->widget_delegate() || widget->IsClosed())
    return nullptr;

  manager = ax_tree_manager_.get();

  // ViewsAXTreeManagers are only created for top-level windows (Widgets). For
  // non top-level Views, look up the Widget's tree ID to retrieve the manager.
  if (!manager) {
    ui::AXTreeID tree_id =
        WidgetAXTreeIDMap::GetInstance().GetWidgetTreeID(widget);
    DCHECK_NE(tree_id, ui::AXTreeIDUnknown());
    manager = static_cast<views::ViewsAXTreeManager*>(
        ui::AXTreeManagerMap::GetInstance().GetManager(tree_id));
  }
#endif
  return manager;
}

gfx::NativeViewAccessible ViewAccessibility::GetFocusedDescendant() {
  if (focused_virtual_child_)
    return focused_virtual_child_->GetNativeObject();
  return view_->GetNativeViewAccessible();
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
