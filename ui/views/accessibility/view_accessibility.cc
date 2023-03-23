// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_accessibility.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/base/buildflags.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
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
    : view_(view), focused_virtual_child_(nullptr) {
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
  AddVirtualChildViewAt(std::move(virtual_view), virtual_children_.size());
}

void ViewAccessibility::AddVirtualChildViewAt(
    std::unique_ptr<AXVirtualView> virtual_view,
    size_t index) {
  DCHECK(virtual_view);
  DCHECK_LE(index, virtual_children_.size());

  if (virtual_view->parent_view() == this)
    return;
  DCHECK(!virtual_view->parent_view()) << "This |view| already has a View "
                                          "parent. Call RemoveVirtualChildView "
                                          "first.";
  DCHECK(!virtual_view->virtual_parent_view()) << "This |view| already has an "
                                                  "AXVirtualView parent. Call "
                                                  "RemoveChildView first.";
  virtual_view->set_parent_view(this);
  auto insert_iterator =
      virtual_children_.begin() + static_cast<ptrdiff_t>(index);
  virtual_children_.insert(insert_iterator, std::move(virtual_view));
}

std::unique_ptr<AXVirtualView> ViewAccessibility::RemoveVirtualChildView(
    AXVirtualView* virtual_view) {
  DCHECK(virtual_view);
  auto cur_index = GetIndexOf(virtual_view);
  if (!cur_index.has_value())
    return {};

  std::unique_ptr<AXVirtualView> child =
      std::move(virtual_children_[cur_index.value()]);
  virtual_children_.erase(virtual_children_.begin() +
                          static_cast<ptrdiff_t>(cur_index.value()));
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

absl::optional<size_t> ViewAccessibility::GetIndexOf(
    const AXVirtualView* virtual_view) const {
  DCHECK(virtual_view);
  const auto iter = base::ranges::find(virtual_children_, virtual_view,
                                       &std::unique_ptr<AXVirtualView>::get);
  return iter != virtual_children_.end()
             ? absl::make_optional(
                   static_cast<size_t>(iter - virtual_children_.begin()))
             : absl::nullopt;
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

    // TODO(accessibility): Returning early means that any custom data which
    // had been set via the Override functions is not included. Preserving
    // and exposing these properties might be worth doing, even in the case
    // of object destruction.

    // Ordinarily, a view cannot be focusable if its widget has already closed.
    // So, it would have been appropriate to set the focusable state to false in
    // this particular case. However, the `FocusManager` may sometimes try to
    // retrieve the focusable state of this view via
    // `View::IsAccessibilityFocusable()`, even after this view's widget has
    // been closed. Returning the wrong result might cause a crash, because the
    // focus manager might be expecting the result to be the same regardless of
    // the state of the view's widget.
    if (ViewAccessibility::IsAccessibilityFocusable()) {
      data->AddState(ax::mojom::State::kFocusable);
      // Set this node as intentionally nameless to avoid DCHECKs for a missing
      // name of a focusable.
      data->SetNameExplicitlyEmpty();
    }
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

  std::string name;
  if (custom_data_.GetStringAttribute(ax::mojom::StringAttribute::kName,
                                      &name)) {
    if (!name.empty())
      data->SetNameChecked(name);
    else
      data->SetNameExplicitlyEmpty();
  }

  std::string description;
  if (custom_data_.GetStringAttribute(ax::mojom::StringAttribute::kDescription,
                                      &description)) {
    if (!description.empty())
      data->SetDescription(description);
    else
      data->SetDescriptionExplicitlyEmpty();
  }

  if (custom_data_.GetHasPopup() != ax::mojom::HasPopup::kFalse)
    data->SetHasPopup(custom_data_.GetHasPopup());

  static constexpr ax::mojom::IntAttribute kOverridableIntAttributes[]{
      ax::mojom::IntAttribute::kDescriptionFrom,
      ax::mojom::IntAttribute::kNameFrom,
      ax::mojom::IntAttribute::kPosInSet,
      ax::mojom::IntAttribute::kSetSize,
  };
  for (auto attribute : kOverridableIntAttributes) {
    if (custom_data_.HasIntAttribute(attribute))
      data->AddIntAttribute(attribute, custom_data_.GetIntAttribute(attribute));
  }

  static constexpr ax::mojom::IntListAttribute kOverridableIntListAttributes[]{
      ax::mojom::IntListAttribute::kLabelledbyIds,
      ax::mojom::IntListAttribute::kDescribedbyIds,
  };
  for (auto attribute : kOverridableIntListAttributes) {
    if (custom_data_.HasIntListAttribute(attribute))
      data->AddIntListAttribute(attribute,
                                custom_data_.GetIntListAttribute(attribute));
  }

  if (!data->HasStringAttribute(ax::mojom::StringAttribute::kDescription)) {
    std::u16string tooltip = view_->GetTooltipText(gfx::Point());
    // Some screen readers announce the accessible description right after the
    // accessible name. Only use the tooltip as the accessible description if
    // it's different from the name, otherwise users might be puzzled as to why
    // their screen reader is announcing the same thing twice.
    if (!tooltip.empty() && tooltip != data->GetString16Attribute(
                                           ax::mojom::StringAttribute::kName)) {
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

  DCHECK(!data->HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId))
      << "Please annotate child tree ids using "
         "ViewAccessibility::OverrideChildTreeID.";
  if (child_tree_id_) {
    data->AddChildTreeId(child_tree_id_.value());

    if (widget && widget->GetNativeView() && display::Screen::GetScreen()) {
      const float scale_factor =
          display::Screen::GetScreen()
              ->GetDisplayNearestView(view_->GetWidget()->GetNativeView())
              .device_scale_factor();
      data->AddFloatAttribute(ax::mojom::FloatAttribute::kChildTreeScale,
                              scale_factor);
    }
  }
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
  view_->NotifyAccessibilityEvent(ax::mojom::Event::kFocusAfterMenuClose, true);
}

void ViewAccessibility::OverrideRole(const ax::mojom::Role role) {
  DCHECK(IsValidRoleForViews(role)) << "Invalid role for Views.";
  custom_data_.role = role;
}

void ViewAccessibility::OverrideName(const std::string& name,
                                     const ax::mojom::NameFrom name_from) {
  DCHECK_EQ(name.empty(),
            name_from == ax::mojom::NameFrom::kAttributeExplicitlyEmpty)
      << "If the name is being removed to improve the user experience, "
         "|name_from| should be set to |kAttributeExplicitlyEmpty|.";

  // |AXNodeData::SetName| expects a valid role. Some Views call |OverrideRole|
  // prior to overriding the name. For those that don't, see if we can get the
  // default role from the View.
  if (custom_data_.role == ax::mojom::Role::kUnknown) {
    ui::AXNodeData data;
    view_->GetAccessibleNodeData(&data);
    custom_data_.role = data.role;
  }

  custom_data_.SetNameFrom(name_from);
  custom_data_.SetNameChecked(name);
}

void ViewAccessibility::OverrideName(const std::u16string& name,
                                     const ax::mojom::NameFrom name_from) {
  OverrideName(base::UTF16ToUTF8(name), name_from);
}

void ViewAccessibility::OverrideLabelledBy(
    const View* labelled_by_view,
    const ax::mojom::NameFrom name_from) {
  DCHECK_NE(labelled_by_view, view_);
  // |OverrideName| might have been used before |OverrideLabelledBy|.
  // We don't want to keep an old/incorrect name. In addition, some ATs might
  // expect the name to be provided by us from the label. So try to get the
  // name from the labelling View and use the result.
  //
  // |ViewAccessibility::GetAccessibleNodeData| gets properties from: 1) The
  // View's implementation of |View::GetAccessibleNodeData| and 2) the
  // custom_data_ set via ViewAccessibility's various Override functions.
  // HOWEVER, it returns early prior to checking either of those sources if the
  // Widget does not exist or is closed. Thus given a View whose Widget is about
  // to be created, we cannot use |ViewAccessibility::GetAccessibleNodeData| to
  // obtain the name. If |OverrideLabelledBy| is being called, presumably the
  // labelling View is not in the process of being destroyed. So manually check
  // the two sources.
  ui::AXNodeData label_data;
  const_cast<View*>(labelled_by_view)->GetAccessibleNodeData(&label_data);
  const std::string& label =
      label_data.GetStringAttribute(ax::mojom::StringAttribute::kName).empty()
          ? labelled_by_view->GetViewAccessibility()
                .custom_data_.GetStringAttribute(
                    ax::mojom::StringAttribute::kName)
          : label_data.GetStringAttribute(ax::mojom::StringAttribute::kName);

  // |OverrideName| includes logic to populate custom_data_.role with the
  // View's default role in cases where |OverrideRole| was not called (yet).
  // This ensures |AXNodeData::SetName| is not called with |Role::kUnknown|.
  OverrideName(label, name_from);

  int32_t labelled_by_id =
      labelled_by_view->GetViewAccessibility().GetUniqueId().Get();
  custom_data_.AddIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds,
                                   {labelled_by_id});
}

void ViewAccessibility::OverrideDescription(
    const std::string& description,
    const ax::mojom::DescriptionFrom description_from) {
  DCHECK_EQ(
      description.empty(),
      description_from == ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty)
      << "If the description is being removed to improve the user experience, "
         "|description_from| should be set to |kAttributeExplicitlyEmpty|.";
  custom_data_.SetDescriptionFrom(description_from);
  custom_data_.SetDescription(description);
}

void ViewAccessibility::OverrideDescription(
    const std::u16string& description,
    const ax::mojom::DescriptionFrom description_from) {
  OverrideDescription(base::UTF16ToUTF8(description), description_from);
}

void ViewAccessibility::OverrideNativeWindowTitle(const std::string& title) {
  NOTIMPLEMENTED() << "Only implemented on Mac for now.";
}

void ViewAccessibility::OverrideNativeWindowTitle(const std::u16string& title) {
  OverrideNativeWindowTitle(base::UTF16ToUTF8(title));
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

void ViewAccessibility::OverrideHasPopup(const ax::mojom::HasPopup has_popup) {
  custom_data_.SetHasPopup(has_popup);
}

void ViewAccessibility::OverridePosInSet(int pos_in_set, int set_size) {
  custom_data_.AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, pos_in_set);
  custom_data_.AddIntAttribute(ax::mojom::IntAttribute::kSetSize, set_size);
}

void ViewAccessibility::ClearPosInSetOverride() {
  custom_data_.RemoveIntAttribute(ax::mojom::IntAttribute::kPosInSet);
  custom_data_.RemoveIntAttribute(ax::mojom::IntAttribute::kSetSize);
}

void ViewAccessibility::OverrideNextFocus(Widget* widget) {
  if (widget)
    next_focus_ = widget->GetWeakPtr();
  else
    next_focus_ = nullptr;
}

void ViewAccessibility::OverridePreviousFocus(Widget* widget) {
  if (widget)
    previous_focus_ = widget->GetWeakPtr();
  else
    previous_focus_ = nullptr;
}

Widget* ViewAccessibility::GetNextWindowFocus() const {
  return next_focus_.get();
}

Widget* ViewAccessibility::GetPreviousWindowFocus() const {
  return previous_focus_.get();
}

void ViewAccessibility::OverrideChildTreeID(ui::AXTreeID tree_id) {
  if (tree_id == ui::AXTreeIDUnknown())
    child_tree_id_ = absl::nullopt;
  else
    child_tree_id_ = tree_id;
}

ui::AXTreeID ViewAccessibility::GetChildTreeID() const {
  return child_tree_id_ ? *child_tree_id_ : ui::AXTreeIDUnknown();
}

gfx::NativeViewAccessible ViewAccessibility::GetNativeObject() const {
  return nullptr;
}

void ViewAccessibility::NotifyAccessibilityEvent(ax::mojom::Event event_type) {
  // Used for unit testing.
  if (accessibility_events_callback_)
    accessibility_events_callback_.Run(nullptr, event_type);
}

void ViewAccessibility::AnnounceText(const std::u16string& text) {
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
        ui::AXTreeManager::FromID(tree_id));
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
