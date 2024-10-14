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
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/widget_ax_tree_id_map.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
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

#if !BUILDFLAG_INTERNAL_HAS_NATIVE_ACCESSIBILITY()
// static
std::unique_ptr<ViewAccessibility> ViewAccessibility::Create(View* view) {
  // Cannot use std::make_unique because constructor is protected.
  return base::WrapUnique(new ViewAccessibility(view));
}
#endif

ViewAccessibility::ViewAccessibility(View* view)
    : view_(view), focused_virtual_child_(nullptr) {}

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
  data->id = GetUniqueId();
  data->AddStringAttribute(ax::mojom::StringAttribute::kClassName,
                           view_->GetClassName());

  if (is_widget_closed_) {
    // Views may misbehave if their widget is closed; set "null-like" attributes
    // rather than possibly crashing.
    SetDataForClosedWidget(data);
    return;
  }

  data->role = data_.role;
  data->SetNameFrom(GetCachedNameFrom());
  if (!GetCachedName().empty()) {
    data->SetName(GetCachedName());
  }

  view_->GetAccessibleNodeData(data);

  DCHECK(!data->HasChildTreeID()) << "Please annotate child tree ids using "
                                     "ViewAccessibility::SetChildTreeID.";

  // Copy the attributes that are in the cache (`data_`) into the computed
  // `data` object. This is done after the `data` object was initialized with
  // the attributes computed by `View::GetAccessibleNodeData` to ensure that the
  // cached attributes take precedence.
  views::ViewAccessibilityUtils::Merge(/*source*/ data_, /*destination*/ *data);

  // TODO(crbug.com/325137417): This next check should be added to SetRole.
  if (data->role == ax::mojom::Role::kAlertDialog) {
    // When an alert dialog is used, indicate this with xml-roles. This helps
    // JAWS understand that it's a dialog and not just an ordinary alert, even
    // though xml-roles is normally used to expose ARIA roles in web content.
    // Specifically, this enables the JAWS Insert+T read window title command.
    // Note: if an alert has focusable descendants such as buttons, it should
    // use kAlertDialog, not kAlert.
    data->AddStringAttribute(ax::mojom::StringAttribute::kRole, "alertdialog");
  }

  data->relative_bounds.bounds = gfx::RectF(view_->bounds());

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
}

bool ViewAccessibility::GetIsPruned() const {
  return pruned_;
}

void ViewAccessibility::SetCharacterOffsets(
    const std::vector<int32_t>& offsets) {
  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets,
                            offsets);
}

const std::vector<int32_t>& ViewAccessibility::GetCharacterOffsets() const {
  return data_.GetIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets);
}

void ViewAccessibility::SetWordStarts(const std::vector<int32_t>& offsets) {
  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts, offsets);
}

const std::vector<int32_t>& ViewAccessibility::GetWordStarts() const {
  return data_.GetIntListAttribute(ax::mojom::IntListAttribute::kWordStarts);
}

void ViewAccessibility::SetWordEnds(const std::vector<int32_t>& offsets) {
  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds, offsets);
}

const std::vector<int32_t>& ViewAccessibility::GetWordEnds() const {
  return data_.GetIntListAttribute(ax::mojom::IntListAttribute::kWordEnds);
}

void ViewAccessibility::ClearTextOffsets() {
  data_.RemoveIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets);
  data_.RemoveIntListAttribute(ax::mojom::IntListAttribute::kWordStarts);
  data_.RemoveIntListAttribute(ax::mojom::IntListAttribute::kWordEnds);
}

void ViewAccessibility::SetClipsChildren(bool clips_children) {
  data_.AddBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren,
                         clips_children);
}

void ViewAccessibility::SetClassName(const std::string& class_name) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kClassName, class_name);
}

void ViewAccessibility::SetHasPopup(const ax::mojom::HasPopup has_popup) {
  data_.SetHasPopup(has_popup);
}

void ViewAccessibility::SetRole(const ax::mojom::Role role) {
  RETURN_IF_UNAVAILABLE();
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
  RETURN_IF_UNAVAILABLE();

  SetRole(role);
  SetRoleDescription(role_description);
}

void ViewAccessibility::SetName(std::u16string name,
                                ax::mojom::NameFrom name_from) {
  RETURN_IF_UNAVAILABLE();

  // TODO(crbug.com/325137417): Remove once we initialize the cache when a
  // platform accessibility API is used.
  InitializeRoleIfNeeded();

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
    // TODO(crbug.com/325137417): This is a temporary workaround to avoid a
    // DCHECK, once we have migrated all Views to use the new setters and we
    // always set a role in the constructors for views, we can remove this.
    if (data_.role == ax::mojom::Role::kUnknown) {
      ui::AXNodeData data;
      view_->GetAccessibleNodeData(&data);
      data_.role = data.role;
    }

    data_.SetNameChecked(name);
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
  // TODO(crbug.com/325137417): Remove once we initialize the cache when a
  // platform accessibility API is used.
  InitializeRoleIfNeeded();

  // TODO(crbug.com/325137417): This is a temporary workaround to avoid the
  // DCHECK below in the scenario where the View's accessible name is being set
  // through either the GetAccessibleNodeData override pipeline or the
  // SetAccessibleName pipeline, which would make the call to `GetCachedName`
  // return an empty string. (this is the case for `Label` view). Once these are
  // migrated we can remove this `if`, otherwise we must retrieve the name from
  // there if needed.
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

  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds,
                            {naming_view.GetViewAccessibility().GetUniqueId()});
}

void ViewAccessibility::RemoveName() {
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kName);
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kNameFrom);
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
}

void ViewAccessibility::SetRoleDescription(
    const std::string& role_description) {
  SetRoleDescription(base::UTF8ToUTF16(role_description));
}

void ViewAccessibility::RemoveRoleDescription() {
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kRoleDescription);
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

void ViewAccessibility::SetScrollX(int scroll_x) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kScrollX, scroll_x);
}

void ViewAccessibility::SetScrollXMin(int scroll_x_min) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kScrollXMin, scroll_x_min);
}

void ViewAccessibility::SetScrollXMax(int scroll_x_max) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kScrollXMax, scroll_x_max);
}

void ViewAccessibility::SetScrollY(int scroll_y) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kScrollY, scroll_y);
}

void ViewAccessibility::SetScrollYMin(int scroll_y_min) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kScrollYMin, scroll_y_min);
}

void ViewAccessibility::SetScrollYMax(int scroll_y_max) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kScrollYMax, scroll_y_max);
}

void ViewAccessibility::SetIsScrollable(bool is_scrollable) {
  data_.AddBoolAttribute(ax::mojom::BoolAttribute::kScrollable, is_scrollable);
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
  NotifyEvent(ax::mojom::Event::kActiveDescendantChanged, true);
}

void ViewAccessibility::ClearActiveDescendant() {
  if (!data_.HasIntAttribute(ax::mojom::IntAttribute::kActivedescendantId)) {
    return;
  }
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kActivedescendantId);
  NotifyEvent(ax::mojom::Event::kActiveDescendantChanged, true);
}

void ViewAccessibility::SetIsInvisible(bool is_invisible) {
  SetState(ax::mojom::State::kInvisible, is_invisible);
}

void ViewAccessibility::SetIsDefault(bool is_default) {
  if (data_.HasState(ax::mojom::State::kDefault) == is_default) {
    return;
  }
  SetState(ax::mojom::State::kDefault, is_default);
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

  UpdateFocusableState();

  // TODO(crbug.com/40896388): We need a specific enabled-changed event for
  // this. Some platforms have specific state-changed events and this generic
  // event does not suggest what changed.
  NotifyEvent(ax::mojom::Event::kStateChanged, true);
}

bool ViewAccessibility::GetIsEnabled() const {
  return data_.GetRestriction() != ax::mojom::Restriction::kDisabled;
}

void ViewAccessibility::SetTableRowCount(int row_count) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kTableRowCount, row_count);
}

void ViewAccessibility::SetTableColumnCount(int column_count) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kTableColumnCount,
                        column_count);
}

void ViewAccessibility::ClearDescriptionAndDescriptionFrom() {
  data_.SetDescriptionExplicitlyEmpty();
}

void ViewAccessibility::RemoveDescription() {
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kDescription);
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kDescriptionFrom);
}

void ViewAccessibility::SetDescription(
    const std::string& description,
    const ax::mojom::DescriptionFrom description_from) {
  // TODO(crbug.com/325137417): Remove once we initialize the cache when a
  // platform accessibility API is used.
  InitializeRoleIfNeeded();
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
  // TODO(crbug.com/325137417): Remove once we initialize the cache when a
  // platform accessibility API is used.
  InitializeRoleIfNeeded();

  std::u16string name = describing_view.GetViewAccessibility().GetCachedName();
  DCHECK(!name.empty())
      << "The describing view must have an accessible name set.";
  SetDescription(name, ax::mojom::DescriptionFrom::kRelatedElement);
  data_.AddIntListAttribute(
      ax::mojom::IntListAttribute::kDescribedbyIds,
      {describing_view.GetViewAccessibility().GetUniqueId()});
}

std::u16string ViewAccessibility::GetCachedDescription() const {
  if (data_.HasStringAttribute(ax::mojom::StringAttribute::kDescription)) {
    return base::UTF8ToUTF16(
        data_.GetStringAttribute(ax::mojom::StringAttribute::kDescription));
  }
  return std::u16string();
}

void ViewAccessibility::SetPlaceholder(const std::string& placeholder) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kPlaceholder,
                           placeholder);
}

void ViewAccessibility::AddAction(ax::mojom::Action action) {
  if (data_.HasAction(action)) {
    return;
  }

  data_.AddAction(action);
}

void ViewAccessibility::SetCheckedState(ax::mojom::CheckedState checked_state) {
  if (checked_state == data_.GetCheckedState()) {
    return;
  }
  data_.SetCheckedState(checked_state);
  NotifyEvent(ax::mojom::Event::kCheckedStateChanged, true);
}

void ViewAccessibility::RemoveCheckedState() {
  if (data_.HasCheckedState()) {
    data_.RemoveIntAttribute(ax::mojom::IntAttribute::kCheckedState);
  }
}

void ViewAccessibility::SetKeyShortcuts(const std::string& key_shortcuts) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts,
                           key_shortcuts);
}

void ViewAccessibility::RemoveKeyShortcuts() {
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts);
}

void ViewAccessibility::SetAccessKey(const std::string& access_key) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kAccessKey, access_key);
}

void ViewAccessibility::RemoveAccessKey() {
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kAccessKey);
}

void ViewAccessibility::SetChildTreeNodeAppId(const std::string& app_id) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kChildTreeNodeAppId,
                           app_id);
}

void ViewAccessibility::RemoveChildTreeNodeAppId() {
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kChildTreeNodeAppId);
}

void ViewAccessibility::SetIsSelected(bool selected) {
  if (data_.HasBoolAttribute(ax::mojom::BoolAttribute::kSelected) &&
      selected == data_.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
    return;
  }

  data_.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, selected);

  // We only want to send the notification if the view gets selected,
  // this is since the event serves to notify of a selection being made, not of
  // a selection being unmade.
  if (selected) {
    NotifyEvent(ax::mojom::Event::kSelection, true);
  }
}

void ViewAccessibility::SetIsMultiselectable(bool multiselectable) {
  SetState(ax::mojom::State::kMultiselectable, multiselectable);
}

void ViewAccessibility::SetIsModal(bool modal) {
  data_.AddBoolAttribute(ax::mojom::BoolAttribute::kModal, modal);
}

void ViewAccessibility::AddHTMLAttributes(
    std::pair<std::string, std::string> attribute) {
  data_.html_attributes.push_back(attribute);
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

void ViewAccessibility::SetContainerLiveStatus(const std::string& status) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kContainerLiveStatus,
                           status);
}

void ViewAccessibility::RemoveContainerLiveStatus() {
  if (!data_.HasStringAttribute(
          ax::mojom::StringAttribute::kContainerLiveStatus)) {
    return;
  }
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kContainerLiveStatus);
}

void ViewAccessibility::SetValue(const std::string& value) {
  if (value == data_.GetStringAttribute(ax::mojom::StringAttribute::kValue)) {
    return;
  }
  data_.AddStringAttribute(ax::mojom::StringAttribute::kValue, value);
  NotifyEvent(ax::mojom::Event::kValueChanged, true);
}

void ViewAccessibility::SetValue(const std::u16string& value) {
  SetValue(base::UTF16ToUTF8(value));
}

void ViewAccessibility::RemoveValue() {
  if (!data_.HasStringAttribute(ax::mojom::StringAttribute::kValue)) {
    return;
  }
  data_.RemoveStringAttribute(ax::mojom::StringAttribute::kValue);
  NotifyEvent(ax::mojom::Event::kValueChanged, true);
}

std::u16string ViewAccessibility::GetValue() const {
  return base::UTF8ToUTF16(
      data_.GetStringAttribute(ax::mojom::StringAttribute::kValue));
}

void ViewAccessibility::SetDefaultActionVerb(
    const ax::mojom::DefaultActionVerb default_action_verb) {
  data_.SetDefaultActionVerb(default_action_verb);
}

void ViewAccessibility::RemoveDefaultActionVerb() {
  data_.RemoveIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb);
}

void ViewAccessibility::SetAutoComplete(const std::string& autocomplete) {
  data_.AddStringAttribute(ax::mojom::StringAttribute::kAutoComplete,
                           autocomplete);
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

void ViewAccessibility::UpdateStatesForViewAndDescendants() {
  internal::ScopedChildrenLock lock(view_);
  UpdateFocusableState();
  UpdateReadyToNotifyEvents();
  for (auto& child : view_->children()) {
    child->GetViewAccessibility().UpdateStatesForViewAndDescendants();
  }
}

void ViewAccessibility::SetRootViewIsReadyToNotifyEvents() {
  CHECK(!view_->parent())
      << "This method should only be called on the RootView.";
  ready_to_notify_events_ = true;
}

void ViewAccessibility::UpdateInvisibleState() {
  bool is_invisible =
      !view_->GetVisible() && data_.role != ax::mojom::Role::kAlert;
  SetState(ax::mojom::State::kInvisible, is_invisible);
}

void ViewAccessibility::SetChildTreeID(ui::AXTreeID tree_id) {
  if (tree_id != ui::AXTreeIDUnknown()) {
    data_.AddChildTreeId(tree_id);

    const views::Widget* widget = view_->GetWidget();
    if (widget && widget->GetNativeView() && display::Screen::GetScreen()) {
      // TODO(accessibility): There potentially could be an issue where the
      // device scale factor changes from the time the tree ID is set to the
      // time `GetAccessibleNodeData` is queried. If this ever pops up, a
      // potential solution could be to make ViewAccessibility a DisplayObserver
      // and add `this` as an observer when the tree ID is set. Then, when the
      // display changes, we can update the scale factor in the cache, probably
      // by implementing `OnDisplayMetricsChanged`.
      const float scale_factor =
          display::Screen::GetScreen()
              ->GetDisplayNearestView(widget->GetNativeView())
              .device_scale_factor();
      SetChildTreeScaleFactor(scale_factor);
    }
  }
}

ui::AXTreeID ViewAccessibility::GetChildTreeID() const {
  std::optional<ui::AXTreeID> child_tree_id = data_.GetChildTreeID();
  return child_tree_id ? child_tree_id.value() : ui::AXTreeIDUnknown();
}

void ViewAccessibility::SetChildTreeScaleFactor(float scale_factor) {
  if (data_.HasChildTreeID()) {
    data_.AddFloatAttribute(ax::mojom::FloatAttribute::kChildTreeScale,
                            scale_factor);
  }
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

ui::AXPlatformNodeId ViewAccessibility::GetUniqueId() const {
  return unique_id_;
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

void ViewAccessibility::CompleteCacheInitializationRecursive() {
  internal::ScopedChildrenLock lock(view_);
  initialization_state_ = State::kInitializing;

  ui::AXNodeData data;
  view_->OnAccessibilityInitializing(&data);

#if DCHECK_IS_ON()
  views::ViewAccessibilityUtils::ValidateAttributesNotSet(data, data_);
#endif

  // Merge it with the cache.
  views::ViewAccessibilityUtils::Merge(/*source*/ data, /*destination*/ data_);

  initialization_state_ = State::kInitialized;

  for (auto& child : view_->children()) {
    child->GetViewAccessibility().CompleteCacheInitializationRecursive();
  }
}

void ViewAccessibility::InitializeRoleIfNeeded() {
  RETURN_IF_UNAVAILABLE();
  if (data_.role != ax::mojom::Role::kUnknown) {
    return;
  }

  // TODO(crbug.com/325137417): We should initialize the id and class name
  // attributes right here, but cannot do it at the moment because there are
  // setters called from views' constructors. Once all constructors are cleared
  // from accessibility setters (the initial state should be set from
  // `View::GetAccessibleNodeData`), add those missing attributes.
  ui::AXNodeData data;
  view_->GetAccessibleNodeData(&data);

  data_.role = data.role;

  UpdateIgnoredState();
  UpdateInvisibleState();
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

  // If we have already marked `is_widget_closed_` as true, then there's a
  // chance that the view was reparented to a non-closed widget. If so, we must
  // update `is_widget_closed_` in case the new widget is not closed.
  SetWidgetClosedRecursive(widget, widget->IsClosed());
}

void ViewAccessibility::CompleteCacheInitialization() {
  if (initialization_state_ == State::kInitialized) {
    return;
  }

  CompleteCacheInitializationRecursive();
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

void ViewAccessibility::UpdateReadyToNotifyEvents() {
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

  internal::ScopedChildrenLock lock(view_);
  for (auto& child : view_->children()) {
    child->GetViewAccessibility().SetWidgetClosedRecursive(widget, value);
  }
}

void ViewAccessibility::SetDataForClosedWidget(ui::AXNodeData* data) const {
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
}

void ViewAccessibility::SetTextDirection(int text_direction) {
  CHECK_GE(text_direction,
           static_cast<int32_t>(ax::mojom::WritingDirection::kMinValue));
  CHECK_LE(text_direction,
           static_cast<int32_t>(ax::mojom::WritingDirection::kMaxValue));
  data_.AddIntAttribute(ax::mojom::IntAttribute::kTextDirection,
                        text_direction);
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
}

void ViewAccessibility::SetTextSelEnd(int32_t text_sel_end) {
  data_.AddIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, text_sel_end);
}

}  // namespace views
