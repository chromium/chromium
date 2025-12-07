// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_H_
#define UI_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_id.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/accessibility/ax_attribute_changed_callbacks.h"
#include "ui/views/accessibility/view_accessibility_utils.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {

class AXPlatformNodeDelegate;

}  // namespace ui

namespace views {

class AtomicViewAXTreeManager;
class AXVirtualView;
class ScopedAccessibilityEventBlocker;
class View;
class Widget;

using RoleCallbackList = base::RepeatingCallbackList<void(ax::mojom::Role)>;
using IntAttributeCallbackList =
    base::RepeatingCallbackList<void(ax::mojom::IntAttribute,
                                     std::optional<int>)>;
using StringAttributeCallbackList =
    base::RepeatingCallbackList<void(ax::mojom::StringAttribute,
                                     const std::optional<std::string>&)>;
using BoolAttributeCallbackList =
    base::RepeatingCallbackList<void(ax::mojom::BoolAttribute,
                                     std::optional<bool>)>;
using StateCallbackList =
    base::RepeatingCallbackList<void(ax::mojom::State, bool)>;
using IntListAttributeCallbackList = base::RepeatingCallbackList<void(
    ax::mojom::IntListAttribute,
    const std::optional<std::vector<int32_t>>&)>;

// An object that manages the accessibility interface for a View.
//
// The accessibility attributes of a View are set by calling the various setters
// on ViewAccessibility.
//
// In most cases, subclasses of `ViewAccessibility` own the `AXPlatformNode`
// that implements the native accessibility APIs on a specific platform.
//
// TODO(crbug.com/40672441): Update the comment about AXPlatformNode once
// ViewsAX is completed.
class VIEWS_EXPORT ViewAccessibility : public WidgetObserver {
 public:
  using AccessibilityEventsCallback =
      base::RepeatingCallback<void(const ui::AXPlatformNodeDelegate*,
                                   const ax::mojom::Event)>;
  using AXVirtualViews = std::vector<std::unique_ptr<AXVirtualView>>;

  enum class State { kUninitialized, kInitializing, kInitialized };

  static std::unique_ptr<ViewAccessibility> Create(View* view);

  ViewAccessibility(const ViewAccessibility&) = delete;
  ViewAccessibility& operator=(const ViewAccessibility&) = delete;
  ~ViewAccessibility() override;

  // Retrieves the accessibility data in the cache.
  void GetAccessibleNodeData(ui::AXNodeData* node_data) const;

  virtual void NotifyEvent(ax::mojom::Event event_type, bool send_native_event);

  // Made to be overridden on platforms that need the temporary
  // `AtomicViewAXTreeManager` to enable more accessibility functionalities for
  // Views. See crbug.com/1468416 for more info.
  virtual void EnsureAtomicViewAXTreeManager() {}

  void SetIgnoreMissingWidgetForTesting(bool value) {
    ignore_missing_widget_for_testing_ = value;
  }

  //
  // The following methods get or set accessibility attributes.
  //
  // Note that accessibility string attributes are only used if non-empty, so
  // you can't override a string with the empty string.
  //

  // Sets one of our virtual descendants as having the accessibility focus. This
  // means that if this view has the system focus, it will set the accessibility
  // focus to the provided descendant virtual view instead. Set this to nullptr
  // if none of our virtual descendants should have the accessibility focus. It
  // is illegal to set this to any virtual view that is currently not one of our
  // descendants and this is enforced by a DCHECK.
  void OverrideFocus(AXVirtualView* virtual_view);

  // Returns whether this view is focusable when the user uses an accessibility
  // aid or the keyboard, even though it may not be normally focusable. Note
  // that if using the keyboard, on macOS the preference "Full Keyboard Access"
  // needs to be turned on.
  virtual bool IsAccessibilityFocusable() const;

  // Used for testing. Returns true if this view is considered focused.
  virtual bool IsFocusedForTesting() const;

  // Call when this is the active descendant of a popup view that temporarily
  // takes over focus. It is only necessary to use this for menus like autofill,
  // where the actual focus is in content.
  // When the popup closes, call EndPopupFocusOverride().
  virtual void SetPopupFocusOverride();

  // Call when popup closes, if it used SetPopupFocusOverride().
  virtual void EndPopupFocusOverride();

  // Call when a menu closes, to restore focus to where it was previously.
  virtual void FireFocusAfterMenuClose();

  // Sets/gets whether or not this view's descendants should be included in
  // the accessibility tree. It is the functional equivalent of calling
  // `SetAccessibleIsIgnored` on each and every view descendant of this
  // view. The default value is false, which is appropriate for most views.
  // Note that you should not set this property if a view has no descendants.
  // It is essential that you do not set this property to true on containers
  // which have one or more descendants which are focusable or otherwise
  // interactive as this would make those descendants completely inaccessible.
  // If no value has been set, the "leafiness" of this view will be based on
  // the type of children (if any).
  void SetIsLeaf(bool value);
  virtual bool IsLeaf() const;

  // Returns true if an ancestor of this node (not including itself) is a
  // leaf node, meaning that this node is not actually exposed to any
  // platform's accessibility layer.
  virtual bool IsChildOfLeaf() const;

  void SetReadOnly(bool read_only);

  // Returns true if we heuristically pruned (ignored) this view from the
  // accessibility tree.
  bool GetIsPruned() const;

  void SetCharacterOffsets(const std::vector<int32_t>& offsets);

  const std::vector<int32_t>& GetCharacterOffsets() const;

  void SetWordStarts(const std::vector<int32_t>& offsets);

  const std::vector<int32_t>& GetWordStarts() const;

  void SetWordEnds(const std::vector<int32_t>& offsets);

  const std::vector<int32_t>& GetWordEnds() const;

  void ClearTextOffsets();

  void SetControlIds(const std::vector<int32_t>& ids);

  void RemoveControlIds();

  void SetClipsChildren(bool clips_children);

  void SetClassName(const std::string& class_name);

  void SetHasPopup(const ax::mojom::HasPopup has_popup);

  void SetRole(const ax::mojom::Role role);

  // Sets the accessible role along with a customized string to be used by
  // assistive technologies to present the role. When there is no role
  // description provided, assistive technologies will use either the default
  // role descriptions we provide (which are currently located in a number of
  // places. See crbug.com/1290866) or the value provided by their platform. As
  // a general rule, it is preferable to not override the role string. Please
  // seek review from accessibility OWNERs when using this function.
  void SetRole(ax::mojom::Role role, const std::u16string& role_description);

  // This function cannot follow the established pattern and be named GetRole()
  // because of a function of the same name in AXPlatformNodeDelegate.
  // ViewAXPlatformNodeDelegate extends both ViewAccessibility and
  // AXPlatformNodeDelegate, which would lead to conflicts and confusion.
  // TODO(crbug.com/325137417): Rename to GetRole once the ViewsAX project is
  // completed and we don't have ViewAXPlatformNodeDelegate anymore.
  ax::mojom::Role GetCachedRole() const;

  void SetRoleDescription(const std::u16string& role_description);
  void SetRoleDescription(const std::string& role_description);

  std::u16string GetRoleDescription() const;

  void RemoveRoleDescription();

  // For the same reasons as GetCachedRole, this function cannot
  // follow the established pattern and be named GetName()
  // TODO(crbug.com/325137417): Rename to GetName once the ViewsAX project is
  // completed and we don't have ViewAXPlatformNodeDelegate anymore.
  std::u16string GetCachedName() const;

  // Returns the source type of the accessible name.
  //
  // This function cannot currently be named GetNameFrom() because of a function
  // of the same name in AXPlatformNodeDelegate. ViewAXPlatformNodeDelegate
  // extends both ViewAccessibility and AXPlatformNodeDelegate.
  // TODO(crbug.com/325137417): Rename to GetNameFrom once the ViewsAX project
  // is completed and we don't have ViewAXPlatformNodeDelegate anymore.
  ax::mojom::NameFrom GetCachedNameFrom() const;

  // Sets the accessible name to the specified string and source type.
  // To indicate that this view should never have an accessible name, e.g. to
  // prevent screen readers from speaking redundant information, set the type to
  // `kAttributeExplicitlyEmpty`. NOTE: Do not use `kAttributeExplicitlyEmpty`
  // on a view which may or may not have a name depending on circumstances. Also
  // please seek review from accessibility OWNERs when removing the name,
  // especially for views which are focusable or otherwise interactive.
  // The source type options are:
  //
  // * kNone: No name provided.
  // * kAttribute: Name from a flat string (e.g. aria-label or View). This is
  //   the default value.
  // * kAttributeExplicitlyEmpty: Name removed for accessibility reasons.
  // * kCaption: Name from a table caption.
  // * kContents: Name from the displayed text (e.g. label or link).
  // * kPlaceholder: Name from a textfield placeholder.
  // * kRelatedElement: Name from another object in the UI(e.g. figcaption or
  //   View).
  // * kTitle: Name from a title attribute or element (HTML or SVG).
  // * kValue: Name from a value attribute (e.g. button).
  // * kPopoverAttribute: Name from a tooltip-style popover.
  void SetName(std::u16string name, ax::mojom::NameFrom name_from);
  void SetName(std::string_view name, ax::mojom::NameFrom name_from);
  void SetName(std::u16string name);
  void SetName(std::string_view name);

  // Sets the accessible name of this view to that of `naming_view`. Often
  // `naming_view` is a `views::Label`, but any view with an accessible name
  // will work.
  void SetName(View& naming_view);

  // Removes kName and KNameFrom attributes from accessibility cache.
  void RemoveName();

  void SetIsEditable(bool editable);

  void SetBounds(const gfx::RectF& bounds);

  void SetIsSelected(bool selected);

  void SetIsMultiselectable(bool multiselectable);

  void SetIsModal(bool modal);

  void AddHTMLAttributes(std::pair<std::string, std::string> attribute);

  void SetIsHovered(bool is_hovered);
  bool GetIsHovered() const;

  void SetPopupForId(ui::AXPlatformNodeId popup_for_id);

  void SetTextDirection(int text_direction);

  void SetIsProtected(bool is_protected);

  void SetTextSelStart(int32_t text_sel_start);
  void SetTextSelEnd(int32_t text_sel_end);

  void SetLiveAtomic(bool live_atomic);

  void SetLiveStatus(const std::string& status);

  void SetLiveRelevant(const std::string& live_relevant);
  void RemoveLiveRelevant();

  void SetContainerLiveRelevant(const std::string& live_relevant);
  void RemoveContainerLiveRelevant();

  // Hides this view from the accessibility APIs. Keep in mind that this is not
  // the sole determinant of whether the ignored state is set. See
  // `UpdateIgnoredState`.
  void SetIsIgnored(bool is_ignored);
  bool GetIsIgnored() const;

  // Note that `pos_in_set` starts from 1 not 0.
  void SetPosInSet(int pos_in_set);
  void SetSetSize(int set_size);
  void ClearPosInSet();
  void ClearSetSize();

  void SetScrollX(int scroll_x);
  void SetScrollXMin(int scroll_x_min);
  void SetScrollXMax(int scroll_x_max);
  void SetScrollY(int scroll_y);
  void SetScrollYMin(int scroll_y_min);
  void SetScrollYMax(int scroll_y_max);
  void SetIsScrollable(bool scrollable);

  void SetActiveDescendant(views::View& view);
  void SetActiveDescendant(ui::AXPlatformNodeId id);
  void ClearActiveDescendant();

  void SetIsInvisible(bool is_invisible);
  void SetIsExpanded();
  void SetIsCollapsed();

  // Sets the view's expanded and collapsed states back to false. Expanded and
  // collapsed states are typically mutually exclusive; however, certain views,
  // such as the notification header view, offer an extra feature wherein the
  // view itself cannot be either expanded or collapsed. This occurs in
  // situations where the view is considered invisible, and therefore not
  // interactable. Therefore, in such situations, it was necessary to explicitly
  // remove both expanded and collapsed states from the view accessibility
  // cache.
  void RemoveExpandCollapseState();

  void SetIsVertical(bool vertical);

  void SetIsDefault(bool is_default);
  bool GetIsDefault() const;

  // Sets/gets whether or not this view should be marked as "enabled" for the
  // purpose exposing this state in the accessibility tree. As a general rule,
  // it is not advisable to mark a View as enabled in the accessibility tree,
  // while the real View is actually disabled, because such a View will not
  // respond to user actions.
  virtual void SetIsEnabled(bool is_enabled);
  bool GetIsEnabled() const;

  void SetTableRowCount(int row_count);
  void SetTableColumnCount(int column_count);
  void SetAriaTableRowCount(int row_count);
  void SetAriaTableColumnCount(int column_count);
  void ClearTableRowCount();
  void ClearTableColumnCount();
  void ClearAriaTableRowCount();
  void ClearAriaTableColumnCount();

  void SetTableRowIndex(int row_index);
  int GetTableRowIndex() const;
  void SetTableCellRowIndex(int cell_index);
  void SetTableCellColumnIndex(int cell_index);

  void SetTableCellRowSpan(int row_span);
  void SetTableCellColumnSpan(int column_span);

  void SetSortDirection(ax::mojom::SortDirection sort_direction);

  void ClearDescriptionAndDescriptionFrom();
  void RemoveDescription();

  void SetDescription(const std::string& description,
                      const ax::mojom::DescriptionFrom description_from =
                          ax::mojom::DescriptionFrom::kAriaDescription);
  void SetDescription(const std::u16string& description,
                      const ax::mojom::DescriptionFrom description_from =
                          ax::mojom::DescriptionFrom::kAriaDescription);
  void SetDescription(View& describing_view);
  // This function cannot follow the established pattern and be named
  // GetDescription() because of a function of the same name in
  // AXPlatformNodeDelegate. ViewAXPlatformNodeDelegate extends both
  // ViewAccessibility and AXPlatformNodeDelegate, which would lead to conflicts
  // and confusion.
  // TODO(crbug.com/325137417): Rename to GetDescription once the ViewsAX
  // project is completed and we don't have ViewAXPlatformNodeDelegate anymore.
  std::u16string GetCachedDescription() const;

  void OnTooltipTextChanged(
      std::optional<std::u16string> old_tooltip_text = std::nullopt);

  void OnViewAddedToWidget();
  void OnViewRemovedFromWidget();

  void SetPlaceholder(const std::string& placeholder);

  void AddAction(ax::mojom::Action action);

  void SetCheckedState(ax::mojom::CheckedState checked_state);
  ax::mojom::CheckedState GetCheckedState() const;
  void RemoveCheckedState();

  void SetKeyShortcuts(const std::string& key_shortcuts);
  void RemoveKeyShortcuts();

  void SetAccessKey(const std::string& access_key);
  void RemoveAccessKey();

  void SetChildTreeNodeAppId(const std::string& app_id);
  void RemoveChildTreeNodeAppId();

  // Sets the platform-specific accessible name/title property of the
  // NativeViewAccessible window. This is needed on platforms where the name
  // of the NativeViewAccessible window is automatically calculated by the
  // platform's accessibility API. For instance on the Mac, the label of the
  // NativeWidgetMacNSWindow of a JavaScript alert is taken from the name of
  // the child RootView. Note: the first function does the string conversion
  // and calls the second, thus only the latter needs to be implemented by
  // interested platforms.
  void OverrideNativeWindowTitle(const std::u16string& title);
  virtual void OverrideNativeWindowTitle(const std::string& title);

  // Override the next or previous focused widget. Some assistive technologies,
  // such as screen readers, may utilize this information to transition focus
  // from the beginning or end of one widget to another when navigating by its
  // default navigation method.
  void SetNextFocus(Widget* widget);
  void SetPreviousFocus(Widget* widget);
  Widget* GetNextWindowFocus() const;
  Widget* GetPreviousWindowFocus() const;

  virtual void SetShowContextMenu(bool show_context_menu);

  void SetContainerLiveStatus(const std::string& status);
  void RemoveContainerLiveStatus();

  // Sets the kValue attribute of the accessible object.
  // In case of ProgressBar, if progressBarIndicator value is negative,
  // then kValue attribute should not be set.
  void SetValue(const std::string& value);
  void SetValue(std::u16string_view value);
  void RemoveValue();
  std::u16string GetValue() const;

  void SetDefaultActionVerb(
      const ax::mojom::DefaultActionVerb default_action_verb);
  ax::mojom::DefaultActionVerb GetDefaultActionVerb() const;
  void RemoveDefaultActionVerb();

  void SetAutoComplete(const std::string& autocomplete);

  void SetHierarchicalLevel(int hierarchical_level);

  void SetHasFocusableAncestorRecursive(bool ancestor_focusable);

  // Updates the focusable state of the `data_` object.
  // The view is considered focusable if it is not set to never receive focus
  // This function must be called whenever an attribute that can affect the
  // focusable state changes
  virtual void UpdateFocusableState();

  // Updates has_focusable_ancestor_ and the view's ignored state, which depends
  // on this variable.
  void SetHasFocusableAncestor(bool ancestor_focusable);

  // Recursively updates the focusable and invisible states of the view and its
  // children. If a parent is invisible, non-explicitly invisible views inherit
  // invisibility, affecting their final state and focusability.
  void UpdateInvisibleByInheritanceRecursive(const View* initial_view,
                                             bool invisible_by_inheritance);

  // This updates some shared state for the view and all its descendants.
  // Called when `view_` gets added as a child of another View.
  void OnViewHasNewAncestor(const View* new_ancestor);

  // This should only ever be called on the RootView.
  void SetRootViewIsReadyToNotifyEvents();

  // Sets the displayed, top-level URL for the active document. This should
  // generally correspond to what would be shown in the Omnibox. Setting the
  // value should only be done on the `RootView`.
  void SetRootViewURL(const std::string& url);

  // Updates the invisible state of the `data_` object. The view is considered
  // invisible if it is not visible and its role is not kAlert, or if it is
  // "invisible by inheritance", meaning one of its ancestors was set to be
  // invisible.
  virtual void UpdateInvisibleState();

  bool should_be_invisible() const { return should_be_invisible_; }

  // Override the child tree id.
  void SetChildTreeID(ui::AXTreeID tree_id);
  ui::AXTreeID GetChildTreeID() const;
  void RemoveChildTreeID();

  void SetChildTreeScaleFactor(float scale_factor);

  // Returns the accessibility object that represents the View whose
  // accessibility is managed by this instance. This may be an AXPlatformNode or
  // it may be a native accessible object implemented by another class.
  virtual gfx::NativeViewAccessible GetNativeObject() const;

  // Causes the screen reader to announce |text|. If the current user is not
  // using a screen reader, has no effect. AnnouncePolitely() will speak
  // the given string. AnnounceAlert() may make a stronger attempt to be
  // noticeable; the screen reader may say something like "Alert: hello"
  // instead of just "hello", and may interrupt any existing text being spoken.
  // However, the screen reader may also treat the two calls the same.
  // AnnounceText() is a deprecated alias for AnnounceAlert().
  // TODO(crbug.com/40287811) - Migrate all callers of AnnounceText() to
  // one of the other two methods.
  virtual void AnnounceAlert(std::u16string_view text);
  virtual void AnnouncePolitely(std::u16string_view text);
  virtual void AnnounceText(std::u16string_view text);

  virtual ui::AXPlatformNodeId GetUniqueId() const;

  View* view() const { return view_; }
  AXVirtualView* FocusedVirtualChild() const { return focused_virtual_child_; }

  virtual AtomicViewAXTreeManager* GetAtomicViewAXTreeManagerForTesting() const;

  virtual Widget* GetWidget() const;

  // Returns the ViewAccessibility object associated with the parent view (or
  // virtual view). Returns nullptr if this is the root view or the parent is
  // not set yet.
  // TODO(crbug.com/40672441): Rename to GetParent once ViewsAX is completed and
  // AXVirtualView no longer needs to extend AXPlatformNodeDelegate.
  virtual ViewAccessibility* GetViewAccessibilityParent() const;

  // Returns the ViewAccessibility object associated with the first ancestor
  // view (or virtual view) that is not ignored. Returns nullptr if this is the
  // root view or the parent is not set yet.
  ViewAccessibility* GetUnignoredParent() const;

  //
  // Methods for managing virtual views.
  //

  // Adds |virtual_view| as a child of this View. We take ownership of our
  // virtual children.
  void AddVirtualChildView(std::unique_ptr<AXVirtualView> virtual_view);

  // Adds |virtual_view| as a child of this View at an index.
  // We take ownership of our virtual children.
  void AddVirtualChildViewAt(std::unique_ptr<AXVirtualView> virtual_view,
                             size_t index);

  // Removes |virtual_view| from this View. The virtual view's parent will
  // change to nullptr. Hands ownership back to the caller.
  std::unique_ptr<AXVirtualView> RemoveVirtualChildView(
      AXVirtualView* virtual_view);

  // Removes all the virtual children from this View.
  // The virtual views are deleted.
  void RemoveAllVirtualChildViews();

  const AXVirtualViews& virtual_children() const { return virtual_children_; }

  // Returns true if |virtual_view| is contained within the hierarchy of this
  // View, even as an indirect descendant.
  bool Contains(const AXVirtualView* virtual_view) const;

  // Returns the index of |virtual_view|, or nullopt if |virtual_view| is not a
  // child of this View.
  std::optional<size_t> GetIndexOf(const AXVirtualView* virtual_view) const;

  // Returns the native accessibility object associated with the AXVirtualView
  // descendant that is currently focused. If no virtual descendants are
  // present, or no virtual descendant has been marked as focused, returns the
  // native accessibility object associated with this view.
  gfx::NativeViewAccessible GetFocusedDescendant();

  // Returns the ViewAccessibility children. Since virtual children have a
  // higher priority than real children (views), this function returns them
  // first if any. If there are no virtual children, it returns the
  // ViewAccessibility objects associated with the children of the `view_`.
  std::vector<raw_ptr<ViewAccessibility>> GetChildren() const;

  virtual std::string GetDebugString() const;

  // If true, moves accessibility focus to an ancestor.
  void set_propagate_focus_to_ancestor(bool value) {
    propagate_focus_to_ancestor_ = value;
  }

  bool is_invisible_by_inheritance() const {
    return is_invisible_by_inheritance_;
  }

  bool has_focusable_ancestor() const { return has_focusable_ancestor_; }

  bool propagate_focus_to_ancestor() { return propagate_focus_to_ancestor_; }

  // If true, ensures an AtomicViewAXTreeManager is created for this view.
  void set_needs_ax_tree_manager(bool value) { needs_ax_tree_manager_ = value; }

  bool needs_ax_tree_manager() { return needs_ax_tree_manager_; }

  // Used for testing. Allows a test to watch accessibility events.
  const AccessibilityEventsCallback& accessibility_events_callback() const;
  void set_accessibility_events_callback(AccessibilityEventsCallback callback);

  // WidgetObserver overrides.
  void OnWidgetClosing(Widget* widget) override;
  void OnWidgetDestroyed(Widget* widget) override;

  virtual void OnWidgetUpdated(Widget* widget, Widget* old_widget);

  void CompleteCacheInitialization();

  bool IsAccessibilityEnabled() const;

  bool IsReadyToNotifyEvents() const { return ready_to_notify_events_; }

  bool is_initialized() const {
    return initialization_state_ == State::kInitialized;
  }

  // This mechanism allows views to listen for changes in the accessibility
  // attributes of other views. It facilitates communication between views that
  // depend on each other's accessibility attributes, ensuring they can respond
  // to updates effectively. For examples of how to do this, see
  // view_accessibility_unittest.cc.
  // Here's an example:
  //
  // class MyLabel : public View {
  // public:
  //  void SetAccessibleName(const std::string& name) {
  //    GetViewAccessibility().SetName(name);
  //  }
  // };
  //
  // class MyButton : public View {
  // public:
  // explicit MyButton(MyLabel* label) {
  // // Subscribe to label's accessible name changes.
  //  name_changed_subscription_ =
  //    label->GetViewAccessibility().AddStringAttributeChangedCallback(
  //    ax::mojom::StringAttribute::kName,
  //    base::BindRepeating(&MyButton::OnLabelNameChanged,
  //    base::Unretained(this)));
  // }
  //
  // private:
  // void OnLabelNameChanged(ax::mojom::StringAttribute attribute,
  // const std::optional<std::string>& name) {
  //   // The call to SetName()/RemoveName() below will trigger an
  //   // accessibility event. In most cases, this would be the expected
  //   // behavior. However, if we don't want to trigger another one, we can
  //   // block the event temporarily by uncommenting the following line.
  //   // ScopedAccessibilityEventBlocker blocker(GetViewAccessibility());
  //   if (name.has_value()) {
  //     GetViewAccessibility().SetName(name.value());
  //   } else {
  //     GetViewAccessibility().RemoveName();
  //   }
  // }
  //
  // base::CallbackListSubscription name_changed_subscription_;
  // };
  base::CallbackListSubscription AddRoleChangedCallback(
      RoleCallbackList::CallbackType callback);
  base::CallbackListSubscription AddIntAttributeChangedCallback(
      ax::mojom::IntAttribute attribute,
      IntAttributeCallbackList::CallbackType callback);
  base::CallbackListSubscription AddStringAttributeChangedCallback(
      ax::mojom::StringAttribute attribute,
      StringAttributeCallbackList::CallbackType callback);
  base::CallbackListSubscription AddBoolAttributeChangedCallback(
      ax::mojom::BoolAttribute attribute,
      BoolAttributeCallbackList::CallbackType callback);
  base::CallbackListSubscription AddStateChangedCallback(
      ax::mojom::State state,
      StateCallbackList::CallbackType callback);
  base::CallbackListSubscription AddIntListAttributeChangedCallback(
      ax::mojom::IntListAttribute attribute,
      IntListAttributeCallbackList::CallbackType callback);

 protected:
  explicit ViewAccessibility(View* view);

  virtual void FireNativeEvent(ax::mojom::Event event_type);

  const ui::AXNodeData& data() const { return data_; }

  // Updates the ignored state of the `data_` object.
  // The view is considered ignored if it should be ignored as per
  // `should_be_ignored_`, or if it has been pruned (`pruned_`), or if its role
  // is 'kNone'.
  virtual void UpdateIgnoredState();

  void SetState(ax::mojom::State state, bool is_enabled);

  // We don't want to fire accessibility events when the view is being
  // initialized and any setters are called from their respective constructors.
  // We only want to fire events of any subtree of views when that subtree of
  // views is connected to a RootView. This way we ensure that we don't fire
  // events for views that are not connected to a valid tree. See
  // `SetRootViewIsReadyToNotifyEvents`.
  virtual void UpdateReadyToNotifyEvents();

  void SetReadyToNotifyEvents();

  void SetWidgetClosedRecursive(Widget* widget, bool value);

  void SetDataForClosedWidget(ui::AXNodeData* data) const;

  // Contains data that is populated by the accessibility attributes setters.
  ui::AXNodeData data_;

  // Used to determine if a View should be ignored by accessibility clients by
  // being a non-focusable child of a focusable ancestor.
  // E.g., LabelButtons contain Labels, but a11y should just show that there's a
  // button. This helps us make sure this element is excluded from the a11y tree
  // if there's a focusable parent. All focusable elements should be leaf nodes.
  // Exceptions to this rule will themselves be accessibility focusable.
  // TODO(crbug.com/371237539): Eventually this should be standardized across
  // platforms.
  bool has_focusable_ancestor_ = false;

  bool pruned_ = false;

  // This keeps track of whether the view is invisible by an ancestor being set
  // to be invisible.
  bool is_invisible_by_inheritance_ = false;

  // This is set to true when the view is explicitly marked as ignored by
  // `SetIsIgnored`. It is not the only condition that will cause a view to have
  // the ignored accessible state, as `pruned_` and `is_leaf_` can also cause
  // this. See `UpdateIgnoredState`.
  bool should_be_ignored_ = false;

  // This is set to true when the view is explicitly marked as invisible by
  // `SetIsInvisible`. It is not the only condition that will cause a view to
  // have the invisible accessible state. See `UpdateInvisibleState`.
  bool should_be_invisible_ = false;

  // Prevents accessibility events from being fired during initialization of
  // the owning View.
  // True once a View is connected to a RootView.
  bool ready_to_notify_events_ = false;

  // Used for testing. Called every time an accessibility event is fired.
  AccessibilityEventsCallback accessibility_events_callback_;

  bool ignore_missing_widget_for_testing_ = false;

 private:
  FRIEND_TEST_ALL_PREFIXES(ViewTest, ViewAccessibilityReadyToNotifyEvents);
  FRIEND_TEST_ALL_PREFIXES(ViewTest,
                           WidgetObserverViewWidgetClosedViewReparented);
  friend class ScopedAccessibilityEventBlocker;

  // Fully initialize the cache.
  void CompleteCacheInitializationRecursive();

  void OnWidgetUpdatedRecursive(Widget* widget, Widget* old_widget);

  // Prune/Unprune all descendant views from the accessibility tree. We prune
  // for two reasons: 1) The view has been explicitly marked as a leaf node, 2)
  // The view is focusable and lacks focusable descendants (e.g. a button with a
  // label and/or an image).
  void PruneSubtree();
  void UnpruneSubtree();

  void OnRoleChanged(ax::mojom::Role role);
  void OnIntAttributeChanged(ax::mojom::IntAttribute attribute,
                             std::optional<int> value);
  void OnStringAttributeChanged(ax::mojom::StringAttribute attribute,
                                const std::optional<std::string>& value);
  void OnBoolAttributeChanged(ax::mojom::BoolAttribute attribute,
                              std::optional<bool> value);
  void OnStateChanged(ax::mojom::State state, bool is_enabled);
  void OnIntListAttributeChanged(ax::mojom::IntListAttribute attribute,
                                 const std::optional<std::vector<int>>& value);

  void SetBlockNotifyEvents(bool block);

  ui::AXAttributeChangedCallbacks* GetOrCreateAXAttributeChangedCallbacks();

  virtual void NotifyDataChanged();

  // Weak. Owns this.
  const raw_ptr<View> view_;

  // If there are any virtual children, they override any real children.
  // We own our virtual children.
  AXVirtualViews virtual_children_;

  // The virtual child that is currently focused.
  // This is nullptr if no virtual child is focused.
  // See also OverrideFocus() and GetFocusedDescendant().
  raw_ptr<AXVirtualView> focused_virtual_child_;

  const ui::AXUniqueId unique_id_{ui::AXUniqueId::Create()};


  // If set to true, anything that is a descendant of this view will be hidden
  // from accessibility by 'pruning' it from the tree, and setting `pruned_` to
  // true.
  bool is_leaf_ = false;

  // Used by the Views system to help some assistive technologies, such as
  // screen readers, transition focus from one widget to another.
  base::WeakPtr<Widget> next_focus_ = nullptr;
  base::WeakPtr<Widget> previous_focus_ = nullptr;

  // Whether to move accessibility focus to an ancestor.
  bool propagate_focus_to_ancestor_ = false;

  // Whether we need to ensure an AtomicViewAXTreeManager is created for this
  // View.
  bool needs_ax_tree_manager_ = false;

  bool is_widget_closed_ = false;

  std::unique_ptr<ui::AXAttributeChangedCallbacks>
      attribute_changed_callbacks_ = nullptr;

  State initialization_state_ = State::kUninitialized;

  base::ScopedObservation<Widget, WidgetObserver> observation_{this};
};

class IgnoreMissingWidgetForTestingScopedSetter {
 public:
  explicit IgnoreMissingWidgetForTestingScopedSetter(
      ViewAccessibility& view_accessibility)
      : view_accessibility_(&view_accessibility) {
    view_accessibility_->SetIgnoreMissingWidgetForTesting(true);
  }
  ~IgnoreMissingWidgetForTestingScopedSetter() {
    view_accessibility_->SetIgnoreMissingWidgetForTesting(false);
  }

 private:
  raw_ptr<ViewAccessibility> view_accessibility_;
};

class ScopedAccessibilityEventBlocker {
 public:
  explicit ScopedAccessibilityEventBlocker(
      ViewAccessibility& view_accessibility)
      : view_accessibility_(&view_accessibility) {
    CHECK(view_accessibility_);
    view_accessibility_->SetBlockNotifyEvents(true);
  }
  ~ScopedAccessibilityEventBlocker() {
    CHECK(view_accessibility_);
    view_accessibility_->SetBlockNotifyEvents(false);
  }

 private:
  raw_ptr<ViewAccessibility> view_accessibility_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_H_
