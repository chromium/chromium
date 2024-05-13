// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_accessibility.h"

#include <utility>

#include "base/auto_reset.h"
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
#include "ui/views/accessibility/atomic_view_ax_tree_manager.h"
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
    case ax::mojom::Role::kIframe:
    case ax::mojom::Role::kIframePresentational:
    case ax::mojom::Role::kPdfRoot:
    case ax::mojom::Role::kPortal:
    case ax::mojom::Role::kRootWebArea:
    case ax::mojom::Role::kSvgRoot:
    case ax::mojom::Role::kUnknown:
      return false;

    // The role kDocument should not be allowed on Views, but it needs to be
    // allowed temporarily for the CaptionBubbleLabel view. This is because the
    // CaptionBubbleLabel is designed to be interacted with by a braille display
    // in virtual buffer mode. In order to activate the virtual buffer in NVDA,
    // we set the role to kDocument and the readonly restriction.
    //
    // TODO(crbug.com/339479333): Investigate this further to either add a
    // views-specific role that maps to the document role on the various
    // platform APIs, or remove this comment and update the allowed usage of the
    // kDocument role.
    case ax::mojom::Role::kDocument:  // Used for ARIA role="document".
      return true;
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

std::optional<size_t> ViewAccessibility::GetIndexOf(
    const AXVirtualView* virtual_view) const {
  DCHECK(virtual_view);
  const auto iter = base::ranges::find(virtual_children_, virtual_view,
                                       &std::unique_ptr<AXVirtualView>::get);
  return iter != virtual_children_.end()
             ? std::make_optional(
                   static_cast<size_t>(iter - virtual_children_.begin()))
             : std::nullopt;
}

void ViewAccessibility::GetAccessibleNodeData(ui::AXNodeData* data) const {
  data->id = GetUniqueId().Get();
  data->AddStringAttribute(ax::mojom::StringAttribute::kClassName,
                           view_->GetClassName());

  // Views may misbehave if their widget is closed; return an unknown role
  // rather than possibly crashing.
  const views::Widget* widget = view_->GetWidget();
  if (!ignore_missing_widget_for_testing_ &&
      (!widget || !widget->widget_delegate() || widget->IsClosed())) {
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

  // The role and the name needs to be set on the data member passed to
  // View::GetAccessibleNodeData in case the view changes either or both.
  //
  // TODO(accessibility): This won't be necessary once all Views initialize
  // their role and name in Views::GetAccessibleNodeData, and that function
  // is only called once to initialize the cached data.
  data->role = data_.role;
  data->SetNameFrom(GetCachedNameFrom());
  if (!GetCachedName().empty()) {
    data->SetName(GetCachedName());
  }

  view_->GetAccessibleNodeData(data);

  // TODO(accessibility): This next check should be added to SetRole.
  if (data->role == ax::mojom::Role::kAlertDialog) {
    // When an alert dialog is used, indicate this with xml-roles. This helps
    // JAWS understand that it's a dialog and not just an ordinary alert, even
    // though xml-roles is normally used to expose ARIA roles in web content.
    // Specifically, this enables the JAWS Insert+T read window title command.
    // Note: if an alert has focusable descendants such as buttons, it should
    // use kAlertDialog, not kAlert.
    data->AddStringAttribute(ax::mojom::StringAttribute::kRole, "alertdialog");
  }

  data->relative_bounds.bounds = gfx::RectF(view_->GetBoundsInScreen());

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

  // ***IMPORTANT***
  //
  // This step absolutely needs to be at the very end of the function in order
  // for us to catch all the attributes that have been set through a different
  // way than the ViewsAX AXNodeData push system. See `data_` for more info.

#if DCHECK_IS_ON()
  // This will help keep track of the attributes that have already
  // been migrated from the old system of computing AXNodeData for Views (pull),
  // to the new system (push). This will help ensure that new Views don't use
  // the old system for attributes that have already been migrated.
  // TODO(accessibility): Remove once migration is complete.
  views::ViewsAXCompletedAttributes::Validate(*data);
#endif

  views::ViewAccessibilityUtils::Merge(/*source*/ data_, /*destination*/ *data);

  // This was previously found earlier in the function. It has been moved here,
  // after the call to `ViewAccessibility::Merge`, so that we only check the
  // `data` after all the attributes have been set. Otherwise, there was a bug
  // where the description was not yet populated into the out `data` member in
  // `Merge` and so we were falling into the `if` block below, which led to
  // hangs. See https://crbug.com/326509144 for more details.
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

  // Nothing should be added beyond this point. Reach out to the Chromium
  // accessibility team in Slack, or to benjamin.beaudry@microsoft.com if you
  // absolutely need to add something past this point.
}

void ViewAccessibility::NotifyEvent(ax::mojom::Event event_type,
                                    bool send_native_event) {
  // If `pause_accessibility_events_` is true, it means we are initializing
  // property values. In this specific case, we do not want to notify platform
  // assistive technologies that a property has changed.
  if (pause_accessibility_events_) {
    return;
  }

  Widget* const widget = view_->GetWidget();
  // If it belongs to a widget but its native widget is already destructed, do
  // not send such accessibility event as it's unexpected to send such events
  // during destruction, and is likely to lead to crashes/problems.
  if (widget && !widget->GetNativeView()) {
    return;
  }

  AXEventManager::Get()->NotifyViewEvent(view_, event_type);

  if (send_native_event && widget) {
    FireNativeEvent(event_type);
  }

  view_->OnAccessibilityEvent(event_type);
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
      NotifyEvent(ax::mojom::Event::kFocus, true);
    }
  }
}

bool ViewAccessibility::IsAccessibilityFocusable() const {
  return data_.HasState(ax::mojom::State::kFocusable);
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
  NotifyEvent(ax::mojom::Event::kFocusAfterMenuClose, true);
}

void ViewAccessibility::SetProperties(
    std::optional<ax::mojom::Role> role,
    std::optional<std::u16string> name,
    std::optional<std::u16string> description,
    std::optional<std::u16string> role_description,
    std::optional<ax::mojom::NameFrom> name_from,
    std::optional<ax::mojom::DescriptionFrom> description_from) {
  base::AutoReset<bool> initializing(&pause_accessibility_events_, true);
  if (role.has_value()) {
    if (role_description.has_value()) {
      SetRole(role.value(), role_description.value());
    } else {
      SetRole(role.value());
    }
  }

  // Defining the NameFrom value without specifying the name doesn't make much
  // sense. The only exception might be if the NameFrom is setting the name to
  // explicitly empty. In order to prevent surprising/confusing behavior, we
  // only use the NameFrom value if we have an explicit name. As a result, any
  // caller setting the name to explicitly empty must set the name to an empty
  // string.
  if (name.has_value()) {
    if (name_from.has_value()) {
      SetName(name.value(), name_from.value());
    } else {
      SetName(name.value());
    }
  }

  // See the comment above regarding the NameFrom value.
  if (description.has_value()) {
    if (description_from.has_value()) {
      SetDescription(description.value(), description_from.value());
    } else {
      SetDescription(description.value());
    }
  }
}

void ViewAccessibility::SetIsLeaf(bool value) {
  if (value == ViewAccessibility::IsLeaf()) {
    return;
  }

  if (value) {
    PruneSubtree();
  } else {
    UnpruneSubtree();
  }

  is_leaf_ = value;
}

bool ViewAccessibility::IsLeaf() const {
  // TODO(javiercon): The overridden check is temporary until all of ash/ has
  // been migrated to use the new setters.
  return is_leaf_ || overridden_is_leaf_;
}

bool ViewAccessibility::IsChildOfLeaf() const {
  return pruned_;
}

bool ViewAccessibility::GetIsPruned() const {
  return pruned_;
}

void ViewAccessibility::SetCharacterOffsets(
    const std::vector<int32_t>& offsets) {
  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets,
                            offsets);
}

void ViewAccessibility::SetWordStarts(const std::vector<int32_t>& offsets) {
  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts, offsets);
}

void ViewAccessibility::SetWordEnds(const std::vector<int32_t>& offsets) {
  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds, offsets);
}

void ViewAccessibility::ClearTextOffsets() {
  data_.RemoveIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets);
  data_.RemoveIntListAttribute(ax::mojom::IntListAttribute::kWordStarts);
  data_.RemoveIntListAttribute(ax::mojom::IntListAttribute::kWordEnds);
}

void ViewAccessibility::SetHasPopup(const ax::mojom::HasPopup has_popup) {
  data_.SetHasPopup(has_popup);
}

void ViewAccessibility::SetRole(const ax::mojom::Role role) {
  DCHECK(IsValidRoleForViews(role)) << "Invalid role for Views.";
  if (role == GetCachedRole()) {
    return;
  }

  data_.role = role;
  UpdateIgnoredState();
  UpdateInvisibleState();
}

void ViewAccessibility::SetRole(const ax::mojom::Role role,
                                const std::u16string& role_description) {
  if (role_description == data_.GetString16Attribute(
                              ax::mojom::StringAttribute::kRoleDescription)) {
    // No changes to the role description, update the role and return early.
    SetRole(role);
    return;
  }

  if (!role_description.empty()) {
    data_.AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                             base::UTF16ToUTF8(role_description));
  } else {
    data_.RemoveStringAttribute(ax::mojom::StringAttribute::kRoleDescription);
  }

  SetRole(role);
}

void ViewAccessibility::SetName(std::u16string name,
                                ax::mojom::NameFrom name_from) {
  // Allow subclasses to adjust the name.
  view_->AdjustAccessibleName(name, name_from);

  // Ensure we have a current `name_from` value. For instance, the name might
  // still be an empty string, but a view is now indicating that this is by
  // design by setting `NameFrom::kAttributeExplicitlyEmpty`.
  data_.SetNameFrom(name_from);

  if (name == GetCachedName()) {
    return;
  }

  if (name.empty()) {
    data_.RemoveStringAttribute(ax::mojom::StringAttribute::kName);
  } else {
    // |AXNodeData::SetName| expects a valid role. Some Views call |SetRole|
    // prior to setting the name. For those that don't, see if we can get the
    // default role from the View.
    // TODO(accessibility): This is a temporary workaround to avoid a DCHECK,
    // once we have migrated all Views to use the new setters and we always set
    // a role in the constructors for views, we can remove this.
    if (data_.role == ax::mojom::Role::kUnknown) {
      ui::AXNodeData data;
      view_->GetAccessibleNodeData(&data);
      data_.role = data.role;
    }

    data_.SetName(name);
  }

  view_->OnAccessibleNameChanged(name);
  NotifyEvent(ax::mojom::Event::kTextChanged, true);
}

void ViewAccessibility::SetName(const std::string& name,
                                ax::mojom::NameFrom name_from) {
  std::u16string string_name = base::UTF8ToUTF16(name);
  SetName(string_name, name_from);
}

void ViewAccessibility::SetName(const std::string& name) {
  SetName(name, GetCachedNameFrom());
}

void ViewAccessibility::SetName(const std::u16string& name) {
  SetName(name, GetCachedNameFrom());
}

void ViewAccessibility::SetName(View& naming_view) {
  DCHECK_NE(view_, &naming_view);

  // TODO(javiercon): This is a temporary workaround to avoid the DCHECK below
  // in the scenario where the View's accessible name is being set through
  // either the GetAccessibleNodeData override pipeline or the SetAccessibleName
  // pipeline, which would make the call to `GetCachedName` return an
  // empty string. (this is the case for `Label` view). Once these are migrated
  // we can remove this `if`, otherwise we must retrieve the name from there if
  // needed.
  if (naming_view.GetViewAccessibility().GetCachedName().empty()) {
    ui::AXNodeData label_data;
    const_cast<View&>(naming_view).GetAccessibleNodeData(&label_data);
    const std::string& name =
        label_data.GetStringAttribute(ax::mojom::StringAttribute::kName);
    DCHECK(!name.empty());
    SetName(name, ax::mojom::NameFrom::kRelatedElement);
  } else {
    std::u16string name = naming_view.GetViewAccessibility().GetCachedName();
    DCHECK(!name.empty());
    SetName(name, ax::mojom::NameFrom::kRelatedElement);
  }

  data_.AddIntListAttribute(
      ax::mojom::IntListAttribute::kLabelledbyIds,
      {naming_view.GetViewAccessibility().GetUniqueId().Get()});
}

std::u16string ViewAccessibility::GetCachedName() const {
  return data_.GetString16Attribute(ax::mojom::StringAttribute::kName);
}

ax::mojom::NameFrom ViewAccessibility::GetCachedNameFrom() const {
  return static_cast<ax::mojom::NameFrom>(
      data_.GetIntAttribute(ax::mojom::IntAttribute::kNameFrom));
}

ax::mojom::Role ViewAccessibility::GetCachedRole() const {
  return data_.role;
}

void ViewAccessibility::SetBounds(const gfx::RectF& bounds) {
  data_.relative_bounds.bounds = bounds;
}

void ViewAccessibility::SetPosInSet(int pos_in_set) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, pos_in_set);
}

void ViewAccessibility::SetSetSize(int set_size) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kSetSize, set_size);
}

void ViewAccessibility::ClearPosInSet() {
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kPosInSet);
}

void ViewAccessibility::ClearSetSize() {
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kSetSize);
}

void ViewAccessibility::SetActiveDescendant(views::View& view) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                        view.GetViewAccessibility().GetUniqueId().Get());
}

void ViewAccessibility::ClearActiveDescendant() {
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kActivedescendantId);
}

void ViewAccessibility::SetIsEnabled(bool is_enabled) {
  if (is_enabled == GetIsEnabled()) {
    return;
  }

  if (!is_enabled) {
    data_.SetRestriction(ax::mojom::Restriction::kDisabled);
  } else if (data_.GetRestriction() == ax::mojom::Restriction::kDisabled) {
    // Take into account the possibility that the View is marked as readonly
    // but enabled. In other words, we can't just remove all restrictions,
    // unless the View is explicitly marked as disabled. Note that readonly is
    // another restriction state in addition to enabled and disabled, (see
    // `ax::mojom::Restriction`).
    data_.SetRestriction(ax::mojom::Restriction::kNone);
  }

  UpdateFocusableState();

  // TODO(crbug.com/40896388): We need a specific enabled-changed event for
  // this. Some platforms have specific state-changed events and this generic
  // event does not suggest what changed.
  NotifyEvent(ax::mojom::Event::kStateChanged, true);
}

bool ViewAccessibility::GetIsEnabled() const {
  return data_.GetRestriction() != ax::mojom::Restriction::kDisabled;
}

void ViewAccessibility::SetDescription(
    const std::string& description,
    const ax::mojom::DescriptionFrom description_from) {
  if (description.empty() &&
      description_from !=
          ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty) {
    data_.RemoveStringAttribute(ax::mojom::StringAttribute::kDescription);
    data_.RemoveIntAttribute(ax::mojom::IntAttribute::kDescriptionFrom);
    return;
  }

  data_.SetDescriptionFrom(description_from);
  data_.SetDescription(description);
}

void ViewAccessibility::SetDescription(
    const std::u16string& description,
    const ax::mojom::DescriptionFrom description_from) {
  SetDescription(base::UTF16ToUTF8(description), description_from);
}

void ViewAccessibility::SetDescription(View& describing_view) {
  DCHECK_NE(view_, &describing_view);

  std::u16string name = describing_view.GetViewAccessibility().GetCachedName();
  if (name.empty()) {
    // TODO(javiercon): This is a temporary workaround for the scenarios where
    // the name is set via View::SetAccessibleName, which means that
    // ViewAccessibility's data_ will not have the name set. So we first check
    // if it has been set via the old system, and if so we use it. Once
    // SetAccessibleName is migrated to use the new system, remove this check
    // but keep the DCHECK to make sure the name is not empty.
    ui::AXNodeData data;
    const_cast<View&>(describing_view).GetAccessibleNodeData(&data);
    const std::string& view_name =
        data.GetStringAttribute(ax::mojom::StringAttribute::kName).empty()
            ? base::UTF16ToUTF8(describing_view.GetAccessibleName())
            : data.GetStringAttribute(ax::mojom::StringAttribute::kName);
    DCHECK(!view_name.empty());
    SetDescription(view_name, ax::mojom::DescriptionFrom::kRelatedElement);
    data_.AddIntListAttribute(
        ax::mojom::IntListAttribute::kDescribedbyIds,
        {describing_view.GetViewAccessibility().GetUniqueId().Get()});
  } else {
    SetDescription(name, ax::mojom::DescriptionFrom::kRelatedElement);
    data_.AddIntListAttribute(
        ax::mojom::IntListAttribute::kDescribedbyIds,
        {describing_view.GetViewAccessibility().GetUniqueId().Get()});
  }
}

std::u16string ViewAccessibility::GetCachedDescription() const {
  if (data_.HasStringAttribute(ax::mojom::StringAttribute::kDescription)) {
    return base::UTF8ToUTF16(
        data_.GetStringAttribute(ax::mojom::StringAttribute::kDescription));
  }
  return std::u16string();
}

void ViewAccessibility::SetCheckedState(ax::mojom::CheckedState checked_state) {
  data_.SetCheckedState(checked_state);
}

void ViewAccessibility::RemoveCheckedState() {
  if (data_.HasCheckedState()) {
    data_.RemoveIntAttribute(ax::mojom::IntAttribute::kCheckedState);
  }
}

void ViewAccessibility::SetIsSelected(bool selected) {
  data_.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, selected);
}

void ViewAccessibility::SetIsIgnored(bool is_ignored) {
  if (is_ignored == should_be_ignored_) {
    return;
  }

  should_be_ignored_ = is_ignored;

  UpdateIgnoredState();
  NotifyEvent(ax::mojom::Event::kTreeChanged, true);
}

bool ViewAccessibility::GetIsIgnored() const {
  return data_.HasState(ax::mojom::State::kIgnored);
}

void ViewAccessibility::OverrideNativeWindowTitle(const std::string& title) {
  NOTIMPLEMENTED() << "Only implemented on Mac for now.";
}

void ViewAccessibility::OverrideNativeWindowTitle(const std::u16string& title) {
  OverrideNativeWindowTitle(base::UTF16ToUTF8(title));
}

void ViewAccessibility::OverrideIsLeaf(bool value) {
  overridden_is_leaf_ = value;
}

void ViewAccessibility::SetNextFocus(Widget* widget) {
  if (widget)
    next_focus_ = widget->GetWeakPtr();
  else
    next_focus_ = nullptr;
}

void ViewAccessibility::SetPreviousFocus(Widget* widget) {
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

void ViewAccessibility::SetShowContextMenu(bool show_context_menu) {
  if (show_context_menu) {
    data_.AddAction(ax::mojom::Action::kShowContextMenu);
  } else {
    data_.RemoveAction(ax::mojom::Action::kShowContextMenu);
  }
}

void ViewAccessibility::SetState(ax::mojom::State state, bool is_enabled) {
  if (is_enabled) {
    data_.AddState(state);
  } else {
    data_.RemoveState(state);
  }
}

void ViewAccessibility::UpdateFocusableState() {
  bool is_focusable = view_->GetFocusBehavior() != View::FocusBehavior::NEVER &&
                      GetIsEnabled() && view_->IsDrawn() &&
                      !ViewAccessibility::GetIsIgnored();
  SetState(ax::mojom::State::kFocusable, is_focusable);
}

void ViewAccessibility::UpdateFocusableStateRecursive() {
  internal::ScopedChildrenLock lock(view_);
  UpdateFocusableState();
  for (auto& child : view_->children()) {
    child->GetViewAccessibility().UpdateFocusableStateRecursive();
  }
}

void ViewAccessibility::UpdateInvisibleState() {
  bool is_invisible =
      !view_->GetVisible() && data_.role != ax::mojom::Role::kAlert;
  SetState(ax::mojom::State::kInvisible, is_invisible);
}

void ViewAccessibility::OverrideChildTreeID(ui::AXTreeID tree_id) {
  if (tree_id == ui::AXTreeIDUnknown())
    child_tree_id_ = std::nullopt;
  else
    child_tree_id_ = tree_id;
}

ui::AXTreeID ViewAccessibility::GetChildTreeID() const {
  return child_tree_id_ ? *child_tree_id_ : ui::AXTreeIDUnknown();
}

gfx::NativeViewAccessible ViewAccessibility::GetNativeObject() const {
  return nullptr;
}

void ViewAccessibility::AnnounceAlert(const std::u16string& text) {
  if (auto* const widget = view_->GetWidget()) {
    if (auto* const root_view =
            static_cast<internal::RootView*>(widget->GetRootView())) {
      root_view->AnnounceTextAs(text,
                                ui::AXPlatformNode::AnnouncementType::kAlert);
    }
  }
}

void ViewAccessibility::AnnouncePolitely(const std::u16string& text) {
  if (auto* const widget = view_->GetWidget()) {
    if (auto* const root_view =
            static_cast<internal::RootView*>(widget->GetRootView())) {
      root_view->AnnounceTextAs(text,
                                ui::AXPlatformNode::AnnouncementType::kPolite);
    }
  }
}

void ViewAccessibility::AnnounceText(const std::u16string& text) {
  AnnounceAlert(text);
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

AtomicViewAXTreeManager*
ViewAccessibility::GetAtomicViewAXTreeManagerForTesting() const {
  return nullptr;
}

gfx::NativeViewAccessible ViewAccessibility::GetFocusedDescendant() {
  if (focused_virtual_child_)
    return focused_virtual_child_->GetNativeObject();
  return view_->GetNativeViewAccessible();
}

void ViewAccessibility::FireNativeEvent(ax::mojom::Event event_type) {
  if (accessibility_events_callback_) {
    accessibility_events_callback_.Run(nullptr, event_type);
  }
}

const ViewAccessibility::AccessibilityEventsCallback&
ViewAccessibility::accessibility_events_callback() const {
  return accessibility_events_callback_;
}

void ViewAccessibility::set_accessibility_events_callback(
    ViewAccessibility::AccessibilityEventsCallback callback) {
  accessibility_events_callback_ = std::move(callback);
}

void ViewAccessibility::PruneSubtree() {
  internal::ScopedChildrenLock lock(view_);
  for (auto& child : view_->children()) {
    child->GetViewAccessibility().pruned_ = true;
    child->GetViewAccessibility().UpdateIgnoredState();
    child->GetViewAccessibility().PruneSubtree();
  }

  for (auto& child : virtual_children()) {
    child->PruneVirtualSubtree();
  }
}

void ViewAccessibility::UnpruneSubtree() {
  internal::ScopedChildrenLock lock(view_);
  for (auto& child : view_->children()) {
    child->GetViewAccessibility().pruned_ = false;
    child->GetViewAccessibility().UpdateIgnoredState();
    // If we encounter a node that has already been explicitly set to be a leaf,
    // don't unprune it/its subtree. Otherwise we could end up in situations
    // where we have a node that is set to be a leaf, but has unpruned children.
    if (child->GetViewAccessibility().ViewAccessibility::IsLeaf()) {
      continue;
    }
    child->GetViewAccessibility().UnpruneSubtree();
  }

  for (auto& child : virtual_children()) {
    child->UnpruneVirtualSubtree();
  }
}

void ViewAccessibility::UpdateIgnoredState() {
  bool is_ignored =
      should_be_ignored_ || pruned_ || data_.role == ax::mojom::Role::kNone;
  SetState(ax::mojom::State::kIgnored, is_ignored);
  UpdateFocusableState();
}

}  // namespace views
