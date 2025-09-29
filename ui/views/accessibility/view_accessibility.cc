// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_accessibility.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "build/buildflag.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/base/buildflags.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/atomic_view_ax_tree_manager.h"
#include "ui/views/accessibility/ax_update_notifier.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include "ui/views/accessibility/view_ax_platform_node_delegate_win.h"
#elif BUILDFLAG(IS_MAC)
#include "ui/views/accessibility/view_ax_platform_node_delegate_mac.h"
#elif BUILDFLAG(IS_LINUX)
#include "ui/views/accessibility/view_ax_platform_node_delegate_auralinux.h"
#endif

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

#define RETURN_IF_UNAVAILABLE()                                          \
  if (is_widget_closed_) {                                               \
    return;                                                              \
  }                                                                      \
  CHECK(initialization_state_ != State::kInitializing)                   \
      << "Accessibility cache setters must not be used during complete " \
         "initialization of the accessibility cache. Instead, set the "  \
         "attributes directly on `AXNodeData` parameter.";

// static
std::unique_ptr<ViewAccessibility> ViewAccessibility::Create(View* view) {
  // With the feature enabled, the accessibility tree for Views is built using
  // the `BrowserAccessibilityManager` owned by the `BrowserViewsAXManager`.
  // ViewAccessibility is only used to managed the current accessibility state
  // for a view.
  if (::features::IsAccessibilityTreeForViewsEnabled()) {
    // Cannot use std::make_unique because constructor is protected.
    return base::WrapUnique(new ViewAccessibility(view));
  }

#if !BUILDFLAG(HAS_NATIVE_ACCESSIBILITY)
  // Cannot use std::make_unique because constructor is protected.
  return base::WrapUnique(new ViewAccessibility(view));
#elif BUILDFLAG(IS_WIN)
  return ViewAXPlatformNodeDelegateWin::CreatePlatformSpecific(view);
#elif BUILDFLAG(IS_MAC)
  return ViewAXPlatformNodeDelegateMac::CreatePlatformSpecific(view);
#elif BUILDFLAG(IS_LINUX)
  return ViewAXPlatformNodeDelegateAuraLinux::CreatePlatformSpecific(view);
#endif
}

ViewAccessibility::ViewAccessibility(View* view)
    : view_(view), focused_virtual_child_(nullptr) {
  data_.id = GetUniqueId();
  CHECK(data_.id != ui::kInvalidAXNodeID);
}

ViewAccessibility::~ViewAccessibility() = default;

void ViewAccessibility::AddVirtualChildView(
    std::unique_ptr<AXVirtualView> virtual_view) {
  AddVirtualChildViewAt(std::move(virtual_view), virtual_children_.size());
}

void ViewAccessibility::AddVirtualChildViewAt(
    std::unique_ptr<AXVirtualView> virtual_view,
    size_t index) {
  CHECK(view_);
  DCHECK(virtual_view);
  DCHECK_LE(index, virtual_children_.size());

  if (virtual_view->parent_view() == this) {
    return;
  }
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

  AXVirtualView* added_view = virtual_children_[index].get();
  added_view->OnViewHasNewAncestor(view_);
}

std::unique_ptr<AXVirtualView> ViewAccessibility::RemoveVirtualChildView(
    AXVirtualView* virtual_view) {
  DCHECK(virtual_view);
  auto cur_index = GetIndexOf(virtual_view);
  if (!cur_index.has_value()) {
    return {};
  }

  std::unique_ptr<AXVirtualView> child =
      std::move(virtual_children_[cur_index.value()]);
  virtual_children_.erase(virtual_children_.begin() +
                          static_cast<ptrdiff_t>(cur_index.value()));
  child->set_parent_view(nullptr);
  if (focused_virtual_child_ && child->Contains(focused_virtual_child_)) {
    OverrideFocus(nullptr);
  }
  return child;
}

void ViewAccessibility::RemoveAllVirtualChildViews() {
  while (!virtual_children_.empty()) {
    RemoveVirtualChildView(virtual_children_.back().get());
  }
}

bool ViewAccessibility::Contains(const AXVirtualView* virtual_view) const {
  DCHECK(virtual_view);
  for (const auto& virtual_child : virtual_children_) {
    // AXVirtualView::Contains() also checks if the provided virtual view is the
    // same as |this|.
    if (virtual_child->Contains(virtual_view)) {
      return true;
    }
  }
  return false;
}

std::optional<size_t> ViewAccessibility::GetIndexOf(
    const AXVirtualView* virtual_view) const {
  DCHECK(virtual_view);
  const auto iter = std::ranges::find(virtual_children_, virtual_view,
                                      &std::unique_ptr<AXVirtualView>::get);
  return iter != virtual_children_.end()
             ? std::make_optional(
                   static_cast<size_t>(iter - virtual_children_.begin()))
             : std::nullopt;
}

void ViewAccessibility::GetAccessibleNodeData(ui::AXNodeData* data) const {
  // TODO(crbug.com/40672441): Investigate if we can safely remove this now that
  // all accessibility attributes are cached directly.
  if (is_widget_closed_) {
    // Views may misbehave if their widget is closed; set "null-like" attributes
    // rather than possibly crashing.
    SetDataForClosedWidget(data);
    return;
  }

  *data = data_;
}

void ViewAccessibility::NotifyEvent(ax::mojom::Event event_type,
                                    bool send_native_event) {
  CHECK(view_);
  // If `ready_to_notify_events_` is false, it means we are initializing
  // property values. In this specific case, we do not want to notify platform
  // assistive technologies that a property has changed.
  if (!ready_to_notify_events_) {
    return;
  }

  Widget* const widget = view_->GetWidget();
  // If it belongs to a widget but its native widget is already destructed, do
  // not send such accessibility event as it's unexpected to send such events
  // during destruction, and is likely to lead to crashes/problems.
  if (widget && !widget->GetNativeView()) {
    return;
  }

  AXUpdateNotifier::Get()->NotifyViewEvent(view_, event_type);

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
      focused_virtual_child_->NotifyEvent(ax::mojom::Event::kFocus, true);
    } else {
      NotifyEvent(ax::mojom::Event::kFocus, true);
    }
  }
}

bool ViewAccessibility::IsAccessibilityFocusable() const {
  bool focusable = data_.HasState(ax::mojom::State::kFocusable);
  if (focusable) {
    CHECK(!should_be_invisible_ &&
          !data_.HasState(ax::mojom::State::kInvisible))
        << "A view that is focusable should not be marked as invisible. This is"
           "also enforced in RunAccessibilityPaintChecks.";
  }
  return focusable;
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
  return is_leaf_;
}

bool ViewAccessibility::IsChildOfLeaf() const {
  return pruned_;
}

void ViewAccessibility::SetReadOnly(bool read_only) {
  if ((read_only &&
       data_.GetRestriction() == ax::mojom::Restriction::kReadOnly) ||
      (!read_only &&
       data_.GetRestriction() != ax::mojom::Restriction::kReadOnly)) {
    return;
  }

  if (read_only) {
    data_.SetRestriction(ax::mojom::Restriction::kReadOnly);
  } else {
    data_.RemoveIntAttribute(ax::mojom::IntAttribute::kRestriction);
  }

  NotifyDataChanged();
}

bool ViewAccessibility::GetIsPruned() const {
  return pruned_;
}

void ViewAccessibility::SetCharacterOffsets(
    const std::vector<int32_t>& offsets) {
  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets,
                            offsets);

  OnIntListAttributeChanged(ax::mojom::IntListAttribute::kCharacterOffsets,
                            offsets);
  NotifyDataChanged();
}

const std::vector<int32_t>& ViewAccessibility::GetCharacterOffsets() const {
  return data_.GetIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets);
}

void ViewAccessibility::SetWordStarts(const std::vector<int32_t>& offsets) {
  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts, offsets);

  OnIntListAttributeChanged(ax::mojom::IntListAttribute::kWordStarts, offsets);
  NotifyDataChanged();
}

const std::vector<int32_t>& ViewAccessibility::GetWordStarts() const {
  return data_.GetIntListAttribute(ax::mojom::IntListAttribute::kWordStarts);
}

void ViewAccessibility::SetWordEnds(const std::vector<int32_t>& offsets) {
  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds, offsets);

  OnIntListAttributeChanged(ax::mojom::IntListAttribute::kWordEnds, offsets);
  NotifyDataChanged();
}

const std::vector<int32_t>& ViewAccessibility::GetWordEnds() const {
  return data_.GetIntListAttribute(ax::mojom::IntListAttribute::kWordEnds);
}

void ViewAccessibility::ClearTextOffsets() {
  data_.RemoveIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets);
  data_.RemoveIntListAttribute(ax::mojom::IntListAttribute::kWordStarts);
  data_.RemoveIntListAttribute(ax::mojom::IntListAttribute::kWordEnds);

  OnIntListAttributeChanged(ax::mojom::IntListAttribute::kCharacterOffsets,
                            std::nullopt);
  OnIntListAttributeChanged(ax::mojom::IntListAttribute::kWordStarts,
                            std::nullopt);
  OnIntListAttributeChanged(ax::mojom::IntListAttribute::kWordEnds,
                            std::nullopt);
  NotifyDataChanged();
}

void ViewAccessibility::SetControlIds(const std::vector<int32_t>& ids) {
  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kControlsIds, ids);
  NotifyDataChanged();
}

void ViewAccessibility::RemoveControlIds() {
  data_.RemoveIntListAttribute(ax::mojom::IntListAttribute::kControlsIds);
  NotifyDataChanged();
}

void ViewAccessibility::SetClipsChildren(bool clips_children) {
  data_.AddBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren,
                         clips_children);

  OnBoolAttributeChanged(ax::mojom::BoolAttribute::kClipsChildren,
                         clips_children);
  NotifyDataChanged();
}

void ViewAccessibility::SetClassName(const std::string& class_name) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kClassName, class_name);

  OnStringAttributeChanged(ax::mojom::StringAttribute::kClassName, class_name);
  NotifyDataChanged();
}

void ViewAccessibility::SetHasPopup(const ax::mojom::HasPopup has_popup) {
  data_.SetHasPopup(has_popup);
  NotifyDataChanged();
}

void ViewAccessibility::SetRole(const ax::mojom::Role role) {
  RETURN_IF_UNAVAILABLE();
  DCHECK(IsValidRoleForViews(role)) << "Invalid role for Views.";
  if (role == GetCachedRole()) {
    return;
  }

  data_.role = role;

  if (data_.role == ax::mojom::Role::kAlertDialog) {
    // When an alert dialog is used, indicate this with xml-roles. This helps
    // JAWS understand that it's a dialog and not just an ordinary alert, even
    // though xml-roles is normally used to expose ARIA roles in web content.
    // Specifically, this enables the JAWS Insert+T read window title command.
    // Note: if an alert has focusable descendants such as buttons, it should
    // use kAlertDialog, not kAlert.
    data_.AddStringAttribute(ax::mojom::StringAttribute::kRole, "alertdialog");
  } else {
    data_.RemoveStringAttribute(ax::mojom::StringAttribute::kRole);
  }

  UpdateIgnoredState();
  UpdateInvisibleState();

  OnRoleChanged(role);

  NotifyDataChanged();
}

void ViewAccessibility::SetRole(const ax::mojom::Role role,
                                const std::u16string& role_description) {
  RETURN_IF_UNAVAILABLE();

  SetRole(role);
  SetRoleDescription(role_description);
}

void ViewAccessibility::SetName(std::u16string name,
                                ax::mojom::NameFrom name_from) {
  RETURN_IF_UNAVAILABLE();

  // Allow subclasses to adjust the name.
  if (view_) {
    view_->AdjustAccessibleName(name, name_from);
  }

  // Ensure we have a current `name_from` value. For instance, the name might
  // still be an empty string, but a view is now indicating that this is by
  // design by setting `NameFrom::kAttributeExplicitlyEmpty`.
  data_.SetNameFrom(name_from);

  if (name == GetCachedName()) {
    return;
  }

  std::u16string old_name = GetCachedName();

  if (name.empty()) {
    data_.RemoveStringAttribute(ax::mojom::StringAttribute::kName);
  } else {
    data_.SetNameChecked(name);
  }

  if (view_) {
    // If previously the accessible name was the same as the tooltip text, we
    // weren't using the tooltip text as the description, however now that the
    // name has changed, we should check if the tooltip text should be used as
    // the description.
    if (!old_name.empty() && old_name == view_->GetTooltipText()) {
      OnTooltipTextChanged();
    }

    // If a View sets the tooltip text before setting the accessible name, which
    // is a common pattern, and then the View sets the accessible name to the
    // same string, we need to make sure that we clear the description.
    // Otherwise we'll end up with the same accessible name and description.
    if (GetCachedName() == view_->GetTooltipText() &&
        GetCachedDescription() == view_->GetTooltipText()) {
      RemoveDescription();
    }

    view_->OnAccessibleNameChanged(name);
  }

  OnStringAttributeChanged(ax::mojom::StringAttribute::kName,
                           base::UTF16ToUTF8(name));

  NotifyEvent(ax::mojom::Event::kTextChanged, true);
  NotifyDataChanged();
}

void ViewAccessibility::SetName(std::string_view name,
                                ax::mojom::NameFrom name_from) {
  SetName(base::UTF8ToUTF16(name), name_from);
}

void ViewAccessibility::SetName(std::string_view name) {
  SetName(name, GetCachedNameFrom());
}

void ViewAccessibility::SetName(std::u16string name) {
  SetName(std::move(name), GetCachedNameFrom());
}

void ViewAccessibility::SetName(View& naming_view) {
  DCHECK_NE(view_, &naming_view);

  std::u16string name = naming_view.GetViewAccessibility().GetCachedName();
  DCHECK(!name.empty());
  SetName(std::move(name), ax::mojom::NameFrom::kRelatedElement);

  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds,
                            {naming_view.GetViewAccessibility().GetUniqueId()});
  NotifyDataChanged();
}

void ViewAccessibility::RemoveName() {
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kName);
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kNameFrom);

  OnStringAttributeChanged(ax::mojom::StringAttribute::kName, std::nullopt);
  NotifyDataChanged();
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

void ViewAccessibility::SetRoleDescription(
    const std::u16string& role_description) {
  if (role_description == data_.GetString16Attribute(
                              ax::mojom::StringAttribute::kRoleDescription)) {
    return;
  }

  if (!role_description.empty()) {
    data_.AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                             base::UTF16ToUTF8(role_description));
  } else {
    RemoveRoleDescription();
  }

  OnStringAttributeChanged(ax::mojom::StringAttribute::kRoleDescription,
                           base::UTF16ToUTF8(role_description));
  NotifyDataChanged();
}

void ViewAccessibility::SetRoleDescription(
    const std::string& role_description) {
  SetRoleDescription(base::UTF8ToUTF16(role_description));
}

std::u16string ViewAccessibility::GetRoleDescription() const {
  return data_.GetString16Attribute(
      ax::mojom::StringAttribute::kRoleDescription);
}

void ViewAccessibility::RemoveRoleDescription() {
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kRoleDescription);

  OnStringAttributeChanged(ax::mojom::StringAttribute::kRoleDescription,
                           std::nullopt);
  NotifyDataChanged();
}

void ViewAccessibility::SetIsEditable(bool editable) {
  SetState(ax::mojom::State::kEditable, editable);
}

void ViewAccessibility::SetBounds(const gfx::RectF& bounds) {
  if (bounds == data_.relative_bounds.bounds) {
    return;
  }
  data_.relative_bounds.bounds = bounds;
  NotifyEvent(ax::mojom::Event::kLocationChanged, false);
  NotifyDataChanged();
}

void ViewAccessibility::SetPosInSet(int pos_in_set) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, pos_in_set);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kPosInSet, pos_in_set);
  NotifyDataChanged();
}

void ViewAccessibility::SetSetSize(int set_size) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kSetSize, set_size);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kSetSize, set_size);
  NotifyDataChanged();
}

void ViewAccessibility::ClearPosInSet() {
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kPosInSet);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kPosInSet, 0);
  NotifyDataChanged();
}

void ViewAccessibility::ClearSetSize() {
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kSetSize);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kSetSize, 0);
  NotifyDataChanged();
}

void ViewAccessibility::SetScrollX(int scroll_x) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kScrollX, scroll_x);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kScrollX, scroll_x);
  NotifyDataChanged();
}

void ViewAccessibility::SetScrollXMin(int scroll_x_min) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kScrollXMin, scroll_x_min);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kScrollXMin, scroll_x_min);
  NotifyDataChanged();
}

void ViewAccessibility::SetScrollXMax(int scroll_x_max) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kScrollXMax, scroll_x_max);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kScrollXMax, scroll_x_max);
  NotifyDataChanged();
}

void ViewAccessibility::SetScrollY(int scroll_y) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kScrollY, scroll_y);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kScrollY, scroll_y);
  NotifyDataChanged();
}

void ViewAccessibility::SetScrollYMin(int scroll_y_min) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kScrollYMin, scroll_y_min);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kScrollYMin, scroll_y_min);
  NotifyDataChanged();
}

void ViewAccessibility::SetScrollYMax(int scroll_y_max) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kScrollYMax, scroll_y_max);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kScrollYMax, scroll_y_max);
  NotifyDataChanged();
}

void ViewAccessibility::SetIsScrollable(bool is_scrollable) {
  data_.AddBoolAttribute(ax::mojom::BoolAttribute::kScrollable, is_scrollable);

  OnBoolAttributeChanged(ax::mojom::BoolAttribute::kScrollable, is_scrollable);
  NotifyDataChanged();
}

void ViewAccessibility::SetActiveDescendant(views::View& view) {
  SetActiveDescendant(view.GetViewAccessibility().GetUniqueId());
}

void ViewAccessibility::SetActiveDescendant(ui::AXPlatformNodeId id) {
  if (data_.GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId) ==
      id) {
    return;
  }
  data_.AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId, id);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kActivedescendantId, id);

  NotifyEvent(ax::mojom::Event::kActiveDescendantChanged, true);
  NotifyDataChanged();
}

void ViewAccessibility::ClearActiveDescendant() {
  if (!data_.HasIntAttribute(ax::mojom::IntAttribute::kActivedescendantId)) {
    return;
  }
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kActivedescendantId);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kActivedescendantId,
                        std::nullopt);

  NotifyEvent(ax::mojom::Event::kActiveDescendantChanged, true);
  NotifyDataChanged();
}

void ViewAccessibility::SetIsInvisible(bool is_invisible) {
  if (is_invisible == should_be_invisible_) {
    return;
  }

  should_be_invisible_ = is_invisible;

  UpdateInvisibleState();
}

void ViewAccessibility::SetIsDefault(bool is_default) {
  if (data_.HasState(ax::mojom::State::kDefault) == is_default) {
    return;
  }
  SetState(ax::mojom::State::kDefault, is_default);
}

bool ViewAccessibility::GetIsDefault() const {
  return data_.HasState(ax::mojom::State::kDefault);
}

void ViewAccessibility::SetIsEnabled(bool is_enabled) {
  RETURN_IF_UNAVAILABLE();
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

  // AXVirtualViews should be marked as disabled if their
  // owner View is disabled.
  for (auto& virtual_child : virtual_children()) {
    virtual_child->SetIsEnabledRecursive(is_enabled);
  }

  UpdateFocusableState();

  OnIntAttributeChanged(ax::mojom::IntAttribute::kRestriction,
                        static_cast<int32_t>(data_.GetRestriction()));

  // TODO(crbug.com/40896388): We need a specific enabled-changed event for
  // this. Some platforms have specific state-changed events and this generic
  // event does not suggest what changed.
  NotifyEvent(ax::mojom::Event::kStateChanged, true);
  NotifyDataChanged();
}

bool ViewAccessibility::GetIsEnabled() const {
  return data_.GetRestriction() != ax::mojom::Restriction::kDisabled;
}

void ViewAccessibility::SetTableRowCount(int row_count) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kTableRowCount, row_count);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kTableRowCount, row_count);
  NotifyDataChanged();
}

void ViewAccessibility::SetTableColumnCount(int column_count) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kTableColumnCount,
                        column_count);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kTableColumnCount,
                        column_count);
  NotifyDataChanged();
}

void ViewAccessibility::SetAriaTableRowCount(int row_count) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kAriaRowCount, row_count);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kAriaRowCount, row_count);
  NotifyDataChanged();
}

void ViewAccessibility::SetAriaTableColumnCount(int column_count) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kAriaColumnCount,
                        column_count);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kAriaColumnCount,
                        column_count);
  NotifyDataChanged();
}

void ViewAccessibility::ClearTableRowCount() {
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kTableRowCount);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kTableRowCount, std::nullopt);
  NotifyDataChanged();
}

void ViewAccessibility::ClearTableColumnCount() {
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kTableColumnCount);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kTableColumnCount,
                        std::nullopt);
  NotifyDataChanged();
}

void ViewAccessibility::ClearAriaTableRowCount() {
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kAriaRowCount);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kAriaRowCount, std::nullopt);
  NotifyDataChanged();
}

void ViewAccessibility::ClearAriaTableColumnCount() {
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kAriaColumnCount);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kAriaColumnCount,
                        std::nullopt);
  NotifyDataChanged();
}

void ViewAccessibility::SetTableRowIndex(int cell_index) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kTableRowIndex, cell_index);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kTableRowIndex, cell_index);
  NotifyDataChanged();
}

int ViewAccessibility::GetTableRowIndex() const {
  return data_.GetIntAttribute(ax::mojom::IntAttribute::kTableRowIndex);
}

void ViewAccessibility::SetTableCellColumnIndex(int cell_index) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex,
                        cell_index);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kTableCellColumnIndex,
                        cell_index);
  NotifyDataChanged();
}

void ViewAccessibility::SetTableCellRowIndex(int row_index) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex, row_index);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kTableCellRowIndex, row_index);
  NotifyDataChanged();
}

void ViewAccessibility::SetTableCellRowSpan(int row_span) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan, row_span);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kTableCellRowSpan, row_span);
  NotifyDataChanged();
}

void ViewAccessibility::SetTableCellColumnSpan(int column_span) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan,
                        column_span);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kTableCellColumnSpan,
                        column_span);
  NotifyDataChanged();
}

void ViewAccessibility::SetSortDirection(
    ax::mojom::SortDirection sort_direction) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                        static_cast<int>(sort_direction));

  OnIntAttributeChanged(ax::mojom::IntAttribute::kSortDirection,
                        static_cast<int>(sort_direction));
  NotifyDataChanged();
}

void ViewAccessibility::ClearDescriptionAndDescriptionFrom() {
  data_.SetDescriptionExplicitlyEmpty();

  OnStringAttributeChanged(ax::mojom::StringAttribute::kDescription,
                           std::nullopt);
  NotifyDataChanged();
}

void ViewAccessibility::RemoveDescription() {
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kDescription);
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kDescriptionFrom);

  OnStringAttributeChanged(ax::mojom::StringAttribute::kDescription,
                           std::nullopt);
  OnIntAttributeChanged(ax::mojom::IntAttribute::kDescriptionFrom,
                        std::nullopt);
  NotifyDataChanged();
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

  OnStringAttributeChanged(ax::mojom::StringAttribute::kDescription,
                           description);
  OnIntAttributeChanged(ax::mojom::IntAttribute::kDescriptionFrom,
                        static_cast<int>(description_from));
  NotifyDataChanged();
}

void ViewAccessibility::SetDescription(
    const std::u16string& description,
    const ax::mojom::DescriptionFrom description_from) {
  SetDescription(base::UTF16ToUTF8(description), description_from);
}

void ViewAccessibility::SetDescription(View& describing_view) {
  CHECK(view_);
  DCHECK_NE(view_, &describing_view);

  std::u16string name = describing_view.GetViewAccessibility().GetCachedName();
  DCHECK(!name.empty())
      << "The describing view must have an accessible name set.";
  SetDescription(name, ax::mojom::DescriptionFrom::kRelatedElement);

  std::vector<int32_t> ids = {
      describing_view.GetViewAccessibility().GetUniqueId()};

  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kDescribedbyIds, ids);

  OnIntListAttributeChanged(ax::mojom::IntListAttribute::kDescribedbyIds, ids);
  NotifyDataChanged();
}

std::u16string ViewAccessibility::GetCachedDescription() const {
  if (data_.HasStringAttribute(ax::mojom::StringAttribute::kDescription)) {
    return base::UTF8ToUTF16(
        data_.GetStringAttribute(ax::mojom::StringAttribute::kDescription));
  }
  return std::u16string();
}

void ViewAccessibility::OnTooltipTextChanged(
    std::optional<std::u16string> old_tooltip_text) {
  if (!view_) {
    return;
  }

  if (data_.HasStringAttribute(ax::mojom::StringAttribute::kDescription) &&
      view_->GetTooltipText() == GetCachedDescription()) {
    return;
  }
  // Some screen readers announce the accessible description right after the
  // accessible name. Only use the tooltip as the accessible description if
  // it's different from the name, otherwise users might be puzzled as to why
  // their screen reader is announcing the same thing twice.
  const std::u16string tooltip = view_->GetTooltipText();
  // We only want to update the description if we were previously using the
  // tooltip as the description or if we had no description.
  if ((old_tooltip_text.has_value() &&
       old_tooltip_text == GetCachedDescription()) ||
      !data_.HasStringAttribute(ax::mojom::StringAttribute::kDescription)) {
    if (!tooltip.empty() && tooltip != GetCachedName()) {
      SetDescription(tooltip);
    } else {
      RemoveDescription();
    }
  }
}

void ViewAccessibility::OnViewAddedToWidget() {
  if (ViewAccessibility* parent = GetUnignoredParent()) {
    AXUpdateNotifier::Get()->NotifyChildAdded(this, parent);
  }

  // The accessibility class name is set after the view has been attached
  // to a widget, ensuring the object is fully constructed and its class
  // name is stable.
  std::string effective_class = std::string(view_->GetClassName());

#if BUILDFLAG(IS_WIN)
  // On Windows, Narrator restricts focus to web content in Scan Mode only when
  // the root web area’s parent has class name "Chrome_WidgetWin_1". This is a
  // hardcoded behavior. It worked before Chromium enabled UIA by default, since
  // the MSAA Proxy added the root web area under a window with that class name.
  // We’re collaborating with the Narrator team to update their tab detection
  // logic, but rollout will take time. This is a temporary mitigation. See
  // https://crbug.com/443225250 for details.
  if (::ui::AXPlatform::GetInstance().IsUiaProviderEnabled() &&
      features::IsFixNarratorWebContentContainmentEnabled() &&
      effective_class == "ContentsContainerView") {
    effective_class = "Chrome_WidgetWin_1";
  }
#endif  // BUILDFLAG(IS_WIN)

  SetClassName(effective_class);
}

void ViewAccessibility::OnViewRemovedFromWidget() {
  if (ViewAccessibility* parent = GetUnignoredParent()) {
    AXUpdateNotifier::Get()->NotifyChildRemoved(this, parent);
  }
}

void ViewAccessibility::SetPlaceholder(const std::string& placeholder) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kPlaceholder,
                           placeholder);
  NotifyDataChanged();
}

void ViewAccessibility::AddAction(ax::mojom::Action action) {
  if (data_.HasAction(action)) {
    return;
  }

  data_.AddAction(action);
  NotifyDataChanged();
}

void ViewAccessibility::SetCheckedState(ax::mojom::CheckedState checked_state) {
  if (checked_state == data_.GetCheckedState()) {
    return;
  }
  data_.SetCheckedState(checked_state);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kCheckedState,
                        static_cast<int>(checked_state));

  NotifyEvent(ax::mojom::Event::kCheckedStateChanged, true);
  NotifyDataChanged();
}

ax::mojom::CheckedState ViewAccessibility::GetCheckedState() const {
  return data_.GetCheckedState();
}

void ViewAccessibility::RemoveCheckedState() {
  if (data_.HasCheckedState()) {
    data_.RemoveIntAttribute(ax::mojom::IntAttribute::kCheckedState);

    OnIntAttributeChanged(ax::mojom::IntAttribute::kCheckedState, std::nullopt);
    NotifyDataChanged();
  }
}

void ViewAccessibility::SetKeyShortcuts(const std::string& key_shortcuts) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts,
                           key_shortcuts);
  NotifyDataChanged();
}

void ViewAccessibility::RemoveKeyShortcuts() {
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts);
  NotifyDataChanged();
}

void ViewAccessibility::SetAccessKey(const std::string& access_key) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kAccessKey, access_key);
  NotifyDataChanged();
}

void ViewAccessibility::RemoveAccessKey() {
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kAccessKey);
  NotifyDataChanged();
}

void ViewAccessibility::SetChildTreeNodeAppId(const std::string& app_id) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kChildTreeNodeAppId,
                           app_id);
  NotifyDataChanged();
}

void ViewAccessibility::RemoveChildTreeNodeAppId() {
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kChildTreeNodeAppId);
  NotifyDataChanged();
}

void ViewAccessibility::SetIsSelected(bool selected) {
  if (data_.HasBoolAttribute(ax::mojom::BoolAttribute::kSelected) &&
      selected == data_.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
    return;
  }

  data_.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, selected);

  OnBoolAttributeChanged(ax::mojom::BoolAttribute::kSelected, selected);

  // We only want to send the notification if the view gets selected,
  // this is since the event serves to notify of a selection being made, not of
  // a selection being unmade.
  if (selected) {
    NotifyEvent(ax::mojom::Event::kSelection, true);
  }

  NotifyDataChanged();
}

void ViewAccessibility::SetIsMultiselectable(bool multiselectable) {
  SetState(ax::mojom::State::kMultiselectable, multiselectable);
  NotifyDataChanged();
}

void ViewAccessibility::SetIsModal(bool modal) {
  data_.AddBoolAttribute(ax::mojom::BoolAttribute::kModal, modal);
  NotifyDataChanged();
}

void ViewAccessibility::AddHTMLAttributes(
    std::pair<std::string, std::string> attribute) {
  data_.html_attributes.push_back(attribute);
  NotifyDataChanged();
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

void ViewAccessibility::SetNextFocus(Widget* widget) {
  if (widget) {
    next_focus_ = widget->GetWeakPtr();
  } else {
    next_focus_ = nullptr;
  }
}

void ViewAccessibility::SetPreviousFocus(Widget* widget) {
  if (widget) {
    previous_focus_ = widget->GetWeakPtr();
  } else {
    previous_focus_ = nullptr;
  }
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

  // We must add/remove the action to any virtual children as well.
  for (auto& virtual_child : virtual_children()) {
    virtual_child->SetShowContextMenuRecursive(show_context_menu);
  }

  NotifyDataChanged();
}

void ViewAccessibility::SetContainerLiveStatus(const std::string& status) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kContainerLiveStatus,
                           status);
  NotifyDataChanged();
}

void ViewAccessibility::RemoveContainerLiveStatus() {
  if (!data_.HasStringAttribute(
          ax::mojom::StringAttribute::kContainerLiveStatus)) {
    return;
  }
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kContainerLiveStatus);
  NotifyDataChanged();
}

void ViewAccessibility::SetValue(const std::string& value) {
  if (value == data_.GetStringAttribute(ax::mojom::StringAttribute::kValue)) {
    return;
  }
  data_.AddStringAttribute(ax::mojom::StringAttribute::kValue, value);

  if (ready_to_notify_events_) {
    OnStringAttributeChanged(ax::mojom::StringAttribute::kValue, value);
    NotifyEvent(ax::mojom::Event::kValueChanged, true);

    // Only fire a text changed event on text fields and select elements to
    // mimic what is done in the web content.
    if (data_.IsTextField() || ui::IsSelectElement(data_.role)) {
      NotifyEvent(ax::mojom::Event::kTextChanged, true);
    }
  }

  NotifyDataChanged();
}

void ViewAccessibility::SetValue(std::u16string_view value) {
  SetValue(base::UTF16ToUTF8(value));
}

void ViewAccessibility::RemoveValue() {
  if (!data_.HasStringAttribute(ax::mojom::StringAttribute::kValue)) {
    return;
  }
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kValue);

  OnStringAttributeChanged(ax::mojom::StringAttribute::kValue, std::nullopt);

  NotifyEvent(ax::mojom::Event::kValueChanged, true);
  NotifyDataChanged();
}

std::u16string ViewAccessibility::GetValue() const {
  return base::UTF8ToUTF16(
      data_.GetStringAttribute(ax::mojom::StringAttribute::kValue));
}

void ViewAccessibility::SetDefaultActionVerb(
    const ax::mojom::DefaultActionVerb default_action_verb) {
  data_.SetDefaultActionVerb(default_action_verb);
}

ax::mojom::DefaultActionVerb ViewAccessibility::GetDefaultActionVerb() const {
  return data_.GetDefaultActionVerb();
}

void ViewAccessibility::RemoveDefaultActionVerb() {
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb);

  OnIntAttributeChanged(ax::mojom::IntAttribute::kDefaultActionVerb,
                        std::nullopt);
  NotifyDataChanged();
}

void ViewAccessibility::SetAutoComplete(const std::string& autocomplete) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kAutoComplete,
                           autocomplete);
  NotifyDataChanged();
}

void ViewAccessibility::SetHasFocusableAncestor(bool ancestor_focusable) {
  has_focusable_ancestor_ = ancestor_focusable;
  UpdateIgnoredState();
}

void ViewAccessibility::SetHasFocusableAncestorRecursive(
    bool ancestor_focusable) {
  if (view_) {
    for (auto& child : view_->children()) {
      child->GetViewAccessibility().SetHasFocusableAncestor(ancestor_focusable);
      // If the child has been explicitly set to focusable, we skip its subtree
      // since their state will be respected and should already be up to date.
      if (child->GetFocusBehavior() != View::FocusBehavior::NEVER) {
        continue;
      }
      child->GetViewAccessibility().SetHasFocusableAncestorRecursive(
          ancestor_focusable);
    }
  }

  // Now we do the same for any virtual children.
  for (auto& child : virtual_children()) {
    child->SetHasFocusableAncestor(ancestor_focusable);
    child->SetHasFocusableAncestorRecursive(ancestor_focusable);
  }

  UpdateIgnoredState();
}

void ViewAccessibility::UpdateFocusableState() {
  CHECK(view_);
  bool is_focusable = view_->GetFocusBehavior() != View::FocusBehavior::NEVER &&
                      GetIsEnabled() &&
                      !data_.HasState(ax::mojom::State::kInvisible) &&
                      !ViewAccessibility::GetIsIgnored();
  if (is_focusable) {
    CHECK(!should_be_invisible_ &&
          !data_.HasState(ax::mojom::State::kInvisible))
        << "A view that focusable should not be marked as invisible. This is a "
           "check we also make in the Paint Checks.";
  }
  SetState(ax::mojom::State::kFocusable, is_focusable);
}

void ViewAccessibility::UpdateInvisibleByInheritanceRecursive(
    const View* initial_view,
    bool invisible_by_inheritance) {
  CHECK(view_);
  internal::ScopedChildrenLock lock(view_);
  if (view_.get() != initial_view) {
    is_invisible_by_inheritance_ = invisible_by_inheritance;
    if (!view_->GetVisible()) {
      return;
    }
  }
  UpdateInvisibleState();

  for (auto& child : view_->children()) {
    child->GetViewAccessibility().UpdateInvisibleByInheritanceRecursive(
        initial_view, invisible_by_inheritance);
  }

  // Now we do the same for any virtual children.
  for (auto& child : virtual_children()) {
    child->UpdateParentViewIsDrawnRecursive(initial_view,
                                            !invisible_by_inheritance);
  }
}

void ViewAccessibility::OnViewHasNewAncestor(const View* new_ancestor) {
  CHECK(view_->parent());
  // We need to make sure that we are propagating the right values down the
  // recursive calls. For the invisible state, this means we look at the direct
  // parent, rather than the `new_ancestor`, which in subsequent recursive calls
  // could be a root of an entire tree that is getting reparented. This is
  // because if at some point during the recursion, the parent is invisible, it
  // should affect its descendants, even if `new_ancestor` is not. For example,
  // if we have a tree like this:
  // A (visible)
  //   B (invisible)
  // and then a separate tree:
  // C (invisible)
  //   D (invisible by inheritance of C)
  // and then we reparent C to be a child of A:
  // A (visible)
  //   B (invisible)
  //   C (invisible)
  //     D (invisible by inheritance of C)
  // Even though `A` is visible ( A would be `new_ancestor`), we need to make
  // sure that during the recursion, we don't mark `D` as visible, since it's
  // parent is invisible.
  bool parent_invisible =
      view_->parent()->GetViewAccessibility().is_invisible_by_inheritance() ||
      !view_->parent()->GetVisible();
  bool ancestor_focusable =
      new_ancestor->GetFocusBehavior() != View::FocusBehavior::NEVER ||
      new_ancestor->GetViewAccessibility().has_focusable_ancestor();

  internal::ScopedChildrenLock lock(view_);

  is_invisible_by_inheritance_ = parent_invisible;

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
  for (auto& child : view_->children()) {
    child->GetViewAccessibility().OnViewHasNewAncestor(new_ancestor);
  }

  // Now we do the same for any virtual children.
  for (auto& child : virtual_children()) {
    child->OnViewHasNewAncestor(ancestor_focusable);
  }
}

void ViewAccessibility::SetRootViewURL(const std::string& url) {
  CHECK(view_);
  CHECK(!view_->parent())
      << "This method should only be called on the RootView.";
  data_.AddStringAttribute(ax::mojom::StringAttribute::kUrl, url);
  OnStringAttributeChanged(ax::mojom::StringAttribute::kUrl, url);
  NotifyDataChanged();
}

void ViewAccessibility::SetRootViewIsReadyToNotifyEvents() {
  CHECK(view_);
  CHECK(!view_->parent())
      << "This method should only be called on the RootView.";
  ready_to_notify_events_ = true;
}

void ViewAccessibility::UpdateInvisibleState() {
  bool is_invisible =
      (!view_->GetVisible() && data_.role != ax::mojom::Role::kAlert) ||
      is_invisible_by_inheritance_ || should_be_invisible_;
  SetState(ax::mojom::State::kInvisible, is_invisible);
  UpdateFocusableState();
}

void ViewAccessibility::SetChildTreeID(ui::AXTreeID tree_id) {
  CHECK(view_);
  if (tree_id != ui::AXTreeIDUnknown()) {
    data_.AddChildTreeId(tree_id);

    const views::Widget* widget = view_->GetWidget();
    if (widget && widget->GetNativeView() && display::Screen::Get()) {
      // TODO(accessibility): There potentially could be an issue where the
      // device scale factor changes from the time the tree ID is set to the
      // time `GetAccessibleNodeData` is queried. If this ever pops up, a
      // potential solution could be to make ViewAccessibility a DisplayObserver
      // and add `this` as an observer when the tree ID is set. Then, when the
      // display changes, we can update the scale factor in the cache, probably
      // by implementing `OnDisplayMetricsChanged`.
      const float scale_factor =
          display::Screen::Get()
              ->GetDisplayNearestView(widget->GetNativeView())
              .device_scale_factor();
      SetChildTreeScaleFactor(scale_factor);
    }

    OnStringAttributeChanged(ax::mojom::StringAttribute::kChildTreeId,
                             tree_id.ToString());
    NotifyDataChanged();
  }
}

ui::AXTreeID ViewAccessibility::GetChildTreeID() const {
  std::optional<ui::AXTreeID> child_tree_id = data_.GetChildTreeID();
  return child_tree_id ? child_tree_id.value() : ui::AXTreeIDUnknown();
}

void ViewAccessibility::RemoveChildTreeID() {
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kChildTreeId);

  OnStringAttributeChanged(ax::mojom::StringAttribute::kChildTreeId,
                           std::string());
  NotifyDataChanged();
}

void ViewAccessibility::SetChildTreeScaleFactor(float scale_factor) {
  if (data_.HasChildTreeID()) {
    data_.AddFloatAttribute(ax::mojom::FloatAttribute::kChildTreeScale,
                            scale_factor);
    NotifyDataChanged();
  }
}

gfx::NativeViewAccessible ViewAccessibility::GetNativeObject() const {
  return gfx::NativeViewAccessible();
}

void ViewAccessibility::AnnounceAlert(std::u16string_view text) {
  CHECK(view_);
  if (auto* const widget = view_->GetWidget()) {
    if (auto* const root_view =
            static_cast<internal::RootView*>(widget->GetRootView())) {
      root_view->AnnounceTextAs(std::u16string(text),
                                ui::AXPlatformNode::AnnouncementType::kAlert);
    }
  }
}

void ViewAccessibility::AnnouncePolitely(std::u16string_view text) {
  CHECK(view_);
  if (auto* const widget = view_->GetWidget()) {
    if (auto* const root_view =
            static_cast<internal::RootView*>(widget->GetRootView())) {
      root_view->AnnounceTextAs(std::u16string(text),
                                ui::AXPlatformNode::AnnouncementType::kPolite);
    }
  }
}

void ViewAccessibility::AnnounceText(std::u16string_view text) {
  AnnounceAlert(text);
}

ui::AXPlatformNodeId ViewAccessibility::GetUniqueId() const {
  return unique_id_;
}

AtomicViewAXTreeManager*
ViewAccessibility::GetAtomicViewAXTreeManagerForTesting() const {
  return nullptr;
}

Widget* ViewAccessibility::GetWidget() const {
  if (!view_) {
    return nullptr;
  }
  return view_->GetWidget();
}

ViewAccessibility* ViewAccessibility::GetViewAccessibilityParent() const {
  if (!view_) {
    return nullptr;
  }
  if (auto* parent = view_->parent()) {
    return &parent->GetViewAccessibility();
  }
  return nullptr;
}

ViewAccessibility* ViewAccessibility::GetUnignoredParent() const {
  ViewAccessibility* parent = GetViewAccessibilityParent();
  while (parent && parent->GetIsIgnored()) {
    parent = parent->GetViewAccessibilityParent();
  }
  return parent;
}

gfx::NativeViewAccessible ViewAccessibility::GetFocusedDescendant() {
  CHECK(view_);
  if (focused_virtual_child_) {
    return focused_virtual_child_->GetNativeObject();
  }
  return view_->GetNativeViewAccessible();
}

std::vector<raw_ptr<ViewAccessibility>> ViewAccessibility::GetChildren() const {
  std::vector<raw_ptr<ViewAccessibility>> out;

  if (IsLeaf()) {
    return out;
  }

  // The virtual children always override any real children the view might have.
  if (!virtual_children_.empty()) {
    out.reserve(virtual_children_.size());
    for (auto& v : virtual_children_) {
      out.push_back(v.get());
    }
    return out;
  }

  if (!view_) {
    return out;
  }

  const auto& view_children = view_->children();
  out.reserve(view_children.size());
  for (auto child_view : view_children) {
    out.push_back(&child_view->GetViewAccessibility());
  }
  return out;
}

std::string ViewAccessibility::GetDebugString() const {
  return std::string(view_ ? view_->GetClassName() : "ViewAccessibility");
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

void ViewAccessibility::CompleteCacheInitializationRecursive() {
  if (view_) {
    internal::ScopedChildrenLock lock(view_);
  }
  if (initialization_state_ == State::kInitialized) {
    return;
  }

  initialization_state_ = State::kInitializing;

  ui::AXNodeData data;
  if (view_) {
    view_->OnAccessibilityInitializing(&data);
  }

#if DCHECK_IS_ON()
  views::ViewAccessibilityUtils::ValidateAttributesNotSet(data, data_);
#endif

  // Merge it with the cache.
  views::ViewAccessibilityUtils::Merge(/*source*/ data, /*destination*/ data_);

  initialization_state_ = State::kInitialized;

  if (view_) {
    for (auto& child : view_->children()) {
      child->GetViewAccessibility().CompleteCacheInitializationRecursive();
    }
  }

  // Now we do the same for any virtual children.
  for (auto& child : virtual_children()) {
    child->CompleteCacheInitializationRecursive();
  }
}

void ViewAccessibility::OnWidgetUpdatedRecursive(Widget* widget,
                                                 Widget* old_widget) {
  CHECK(widget);

  // If we have already marked `is_widget_closed_` as true, then there's a
  // chance that the view was reparented to a non-closed widget. If so, we must
  // update `is_widget_closed_` in case the new widget is not closed.
  is_widget_closed_ = widget->IsClosed();

  // Initialize the AtomicViewAXTreeManager if necessary when the view gets
  // added to the widget. We must wait for the widget to become available to
  // get valid data our of GetData().
  if (needs_ax_tree_manager()) {
    EnsureAtomicViewAXTreeManager();
  }

  if (view_) {
    internal::ScopedChildrenLock lock(view_);
    for (auto& child : view_->children()) {
      child->GetViewAccessibility().OnWidgetUpdatedRecursive(widget,
                                                             old_widget);
    }
  }
  for (auto& child : virtual_children()) {
    child->OnWidgetUpdatedRecursive(widget, old_widget);
  }
}

void ViewAccessibility::OnWidgetClosing(Widget* widget) {
  // The RootView's ViewAccessibility should be the only registered
  // WidgetObserver.
  CHECK_EQ(view_, widget->GetRootView());
  SetWidgetClosedRecursive(widget, true);
}

void ViewAccessibility::OnWidgetDestroyed(Widget* widget) {
  // The RootView's ViewAccessibility should be the only registered
  // WidgetObserver.
  CHECK(widget->GetRootView());
  CHECK_EQ(view_, widget->GetRootView());
  SetWidgetClosedRecursive(widget, true);
}

void ViewAccessibility::OnWidgetUpdated(Widget* widget, Widget* old_widget) {
  CHECK(widget);
  DCHECK_EQ(widget, view_->GetWidget());
  if (widget == old_widget) {
    return;
  }

  // There's a chance we are reparenting a view that was previously a root
  // view in another widget, if so we need to remove it as an observer of the
  // old widget.
  if (old_widget && old_widget != widget) {
    old_widget->RemoveObserver(this);
  }

  OnWidgetUpdatedRecursive(widget, old_widget);
}

void ViewAccessibility::CompleteCacheInitialization() {
  if (initialization_state_ == State::kInitialized) {
    return;
  }

  CompleteCacheInitializationRecursive();
}

bool ViewAccessibility::IsAccessibilityEnabled() const {
  return ui::AXPlatform::GetInstance().GetMode() == ui::AXMode::kNativeAPIs;
}

void ViewAccessibility::PruneSubtree() {
  CHECK(view_);
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
  CHECK(view_);
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
// TODO(crbug.com/371237539): In ChromeOS, its not an expectation that being
// a view unfocusable descendant of a focusable ancestor will make the view
// ignored.
#if !BUILDFLAG(IS_CHROMEOS)
  bool is_ignored = should_be_ignored_ || pruned_ ||
                    GetCachedRole() == ax::mojom::Role::kNone ||
                    (has_focusable_ancestor_ &&
                     view_->GetFocusBehavior() == View::FocusBehavior::NEVER);
#else
  bool is_ignored = should_be_ignored_ || pruned_ ||
                    GetCachedRole() == ax::mojom::Role::kNone;
#endif  // !BUILDFLAG(IS_CHROMEOS)
  SetState(ax::mojom::State::kIgnored, is_ignored);
  UpdateFocusableState();
}

void ViewAccessibility::UpdateReadyToNotifyEvents() {
  CHECK(view_);
  View* parent = view_->parent();
  if (parent && parent->GetViewAccessibility().ready_to_notify_events_) {
    SetReadyToNotifyEvents();
  }
}

void ViewAccessibility::SetReadyToNotifyEvents() {
  ready_to_notify_events_ = true;
}

void ViewAccessibility::SetWidgetClosedRecursive(Widget* widget, bool value) {
  is_widget_closed_ = value;

  if (view_) {
    internal::ScopedChildrenLock lock(view_);
    for (auto& child : view_->children()) {
      child->GetViewAccessibility().SetWidgetClosedRecursive(widget, value);
    }
  }

  for (auto& child : virtual_children()) {
    child->SetWidgetClosedRecursive(widget, value);
  }
}

void ViewAccessibility::SetDataForClosedWidget(ui::AXNodeData* data) const {
  data->id = data_.id;
  CHECK_EQ(data->id, GetUniqueId());

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
  if (ViewAccessibility::IsAccessibilityFocusable()) {
    data->AddState(ax::mojom::State::kFocusable);
    // Set this node as intentionally nameless to avoid DCHECKs for a missing
    // name of a focusable.
    data->SetNameExplicitlyEmpty();
  }

  // Some of the views like popup_view_views have state collapsed when the
  // widget has already closed and hence explicitly setting the state.
  if (data_.HasState(ax::mojom::State::kCollapsed)) {
    data->AddState(ax::mojom::State::kCollapsed);
  }

  // Some of the views like popup_view_views have state invisible when the
  // widget has already closed and hence explicitly setting the state.
  if (data_.HasState(ax::mojom::State::kInvisible)) {
    data->AddState(ax::mojom::State::kInvisible);
  }
}

void ViewAccessibility::OnRoleChanged(ax::mojom::Role role) {
  GetOrCreateAXAttributeChangedCallbacks()->NotifyRoleChanged(role);
}

base::CallbackListSubscription ViewAccessibility::AddRoleChangedCallback(
    RoleCallbackList::CallbackType callback) {
  return GetOrCreateAXAttributeChangedCallbacks()->AddRoleChangedCallback(
      callback);
}

void ViewAccessibility::OnStringAttributeChanged(
    ax::mojom::StringAttribute attribute,
    const std::optional<std::string>& value) {
  GetOrCreateAXAttributeChangedCallbacks()->NotifyStringAttributeChanged(
      attribute, value);
}

base::CallbackListSubscription
ViewAccessibility::AddStringAttributeChangedCallback(
    ax::mojom::StringAttribute attribute,
    StringAttributeCallbackList::CallbackType callback) {
  return GetOrCreateAXAttributeChangedCallbacks()
      ->AddStringAttributeChangedCallback(attribute, callback);
}

void ViewAccessibility::OnIntAttributeChanged(ax::mojom::IntAttribute attribute,
                                              std::optional<int> value) {
  GetOrCreateAXAttributeChangedCallbacks()->NotifyIntAttributeChanged(attribute,
                                                                      value);
}

base::CallbackListSubscription
ViewAccessibility::AddIntAttributeChangedCallback(
    ax::mojom::IntAttribute attribute,
    IntAttributeCallbackList::CallbackType callback) {
  return GetOrCreateAXAttributeChangedCallbacks()
      ->AddIntAttributeChangedCallback(attribute, callback);
}

void ViewAccessibility::OnBoolAttributeChanged(
    ax::mojom::BoolAttribute attribute,
    std::optional<bool> value) {
  GetOrCreateAXAttributeChangedCallbacks()->NotifyBoolAttributeChanged(
      attribute, value);
}

base::CallbackListSubscription
ViewAccessibility::AddBoolAttributeChangedCallback(
    ax::mojom::BoolAttribute attribute,
    BoolAttributeCallbackList::CallbackType callback) {
  return GetOrCreateAXAttributeChangedCallbacks()
      ->AddBoolAttributeChangedCallback(attribute, callback);
}

void ViewAccessibility::OnStateChanged(ax::mojom::State state, bool value) {
  GetOrCreateAXAttributeChangedCallbacks()->NotifyStateChanged(state, value);
}

base::CallbackListSubscription ViewAccessibility::AddStateChangedCallback(
    ax::mojom::State state,
    StateCallbackList::CallbackType callback) {
  return GetOrCreateAXAttributeChangedCallbacks()->AddStateChangedCallback(
      state, callback);
}

void ViewAccessibility::OnIntListAttributeChanged(
    ax::mojom::IntListAttribute attribute,
    const std::optional<std::vector<int>>& value) {
  GetOrCreateAXAttributeChangedCallbacks()->NotifyIntListAttributeChanged(
      attribute, value);
}

base::CallbackListSubscription
ViewAccessibility::AddIntListAttributeChangedCallback(
    ax::mojom::IntListAttribute attribute,
    IntListAttributeCallbackList::CallbackType callback) {
  return GetOrCreateAXAttributeChangedCallbacks()
      ->AddIntListAttributeChangedCallback(attribute, callback);
}

void ViewAccessibility::SetHierarchicalLevel(int hierarchical_level) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                        hierarchical_level);
}

void ViewAccessibility::SetState(ax::mojom::State state, bool is_enabled) {
  if (is_enabled) {
    data_.AddState(state);
  } else {
    data_.RemoveState(state);
  }

  OnStateChanged(state, is_enabled);
  NotifyDataChanged();
}

void ViewAccessibility::SetBlockNotifyEvents(bool block) {
  // Warning: This method should ONLY be used for special cases by the
  // ScopedAccessibilityEventBlocker. Generally, we want to prevent
  // notifications from being sent until the view is added to a Widget's Views
  // tree, and we should only update `ready_to_notify_events_` when the view is
  // added to the tree. However, there are special cases where we might need to
  // set this variable to true or false at other times. For example, the
  // RootView should always be ready to notify events, and the AnnounceTextView
  // should NOT fire certain types of events due to its nature of being a hidden
  // special view just for making announcements. As of right now, the only
  // special cases are the root view and the announce text view, which is a
  // child of the root view.
  ready_to_notify_events_ = !block;
}

void ViewAccessibility::SetIsHovered(bool is_hovered) {
  if (is_hovered == GetIsHovered()) {
    return;
  }

  SetState(ax::mojom::State::kHovered, is_hovered);
}

bool ViewAccessibility::GetIsHovered() const {
  return data_.HasState(ax::mojom::State::kHovered);
}

void ViewAccessibility::SetPopupForId(ui::AXPlatformNodeId popup_for_id) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kPopupForId, popup_for_id);
  NotifyDataChanged();
}

void ViewAccessibility::SetTextDirection(int text_direction) {
  CHECK_GE(text_direction,
           static_cast<int32_t>(ax::mojom::WritingDirection::kMinValue));
  CHECK_LE(text_direction,
           static_cast<int32_t>(ax::mojom::WritingDirection::kMaxValue));
  data_.AddIntAttribute(ax::mojom::IntAttribute::kTextDirection,
                        text_direction);
  NotifyDataChanged();
}

void ViewAccessibility::SetIsProtected(bool is_protected) {
  if (data_.HasState(ax::mojom::State::kProtected) == is_protected) {
    return;
  }

  SetState(ax::mojom::State::kProtected, is_protected);
}

void ViewAccessibility::SetIsExpanded() {
  // Check to see if the expanded state is already set, if already set no need
  // to add the state again.
  if (data_.HasState(ax::mojom::State::kExpanded)) {
    // The expanded and collapsed state must be mutually exclusive.
    CHECK(!data_.HasState(ax::mojom::State::kCollapsed));
    return;
  }

  bool should_notify = data_.HasState(ax::mojom::State::kCollapsed);
  SetState(ax::mojom::State::kExpanded, true);
  SetState(ax::mojom::State::kCollapsed, false);

  // We should not notify when initial state (expanded = false, collapsed =
  // false) changes. As changes to initial state generally stands for when the
  // accessibility properties are being set by a view constructor or when the
  // view author explicitly resets the value of expanded and collapsed state. In
  // both these cases we dont wont to fire the accessibility event.
  if (should_notify) {
    NotifyEvent(ax::mojom::Event::kExpandedChanged, true);
  }
}

void ViewAccessibility::SetIsCollapsed() {
  // Check to see if the collapsed state is already set, if already set no need
  // to add the state again.
  if (data_.HasState(ax::mojom::State::kCollapsed)) {
    // The expanded and collapsed state must be mutually exclusive.
    CHECK(!data_.HasState(ax::mojom::State::kExpanded));
    return;
  }

  bool should_notify = data_.HasState(ax::mojom::State::kExpanded);
  SetState(ax::mojom::State::kCollapsed, true);
  SetState(ax::mojom::State::kExpanded, false);

  // We should not notify when initial state (expanded = false, collapsed =
  // false) changes. As changes to initial state generally stands for when the
  // accessibility properties are being set by a view constructor or when the
  // view author explicitly resets the value of expanded and collapsed state. In
  // both these cases we dont wont to fire the accessibility event.
  if (should_notify) {
    NotifyEvent(ax::mojom::Event::kExpandedChanged, true);
  }
}

void ViewAccessibility::RemoveExpandCollapseState() {
  SetState(ax::mojom::State::kExpanded, false);
  SetState(ax::mojom::State::kCollapsed, false);
}

void ViewAccessibility::SetIsVertical(bool vertical) {
  CHECK(!data_.HasState(ax::mojom::State::kHorizontal));
  if (data_.HasState(ax::mojom::State::kVertical)) {
    return;
  }

  SetState(ax::mojom::State::kVertical, vertical);
}

void ViewAccessibility::SetTextSelStart(int32_t text_sel_start) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kTextSelStart, text_sel_start);
  NotifyDataChanged();
}

void ViewAccessibility::SetTextSelEnd(int32_t text_sel_end) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, text_sel_end);
  NotifyDataChanged();
}

void ViewAccessibility::SetLiveAtomic(bool live_atomic) {
  data_.AddBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic, live_atomic);
  NotifyDataChanged();
}

void ViewAccessibility::SetLiveStatus(const std::string& live_status) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                           live_status);
  NotifyDataChanged();
}

void ViewAccessibility::SetLiveRelevant(const std::string& live_relevant) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kLiveRelevant,
                           live_relevant);
  NotifyDataChanged();
}

void ViewAccessibility::RemoveLiveRelevant() {
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kLiveRelevant);
  NotifyDataChanged();
}

void ViewAccessibility::SetContainerLiveRelevant(
    const std::string& live_relevant) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kContainerLiveRelevant,
                           live_relevant);
  NotifyDataChanged();
}

void ViewAccessibility::RemoveContainerLiveRelevant() {
  data_.RemoveStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveRelevant);
  NotifyDataChanged();
}

ui::AXAttributeChangedCallbacks*
ViewAccessibility::GetOrCreateAXAttributeChangedCallbacks() {
  if (!attribute_changed_callbacks_) {
    attribute_changed_callbacks_ =
        std::make_unique<ui::AXAttributeChangedCallbacks>();
  }

  return attribute_changed_callbacks_.get();
}

void ViewAccessibility::NotifyDataChanged() {
  CHECK(view_);
  AXUpdateNotifier::Get()->NotifyViewDataChanged(view_);
}

}  // namespace views
