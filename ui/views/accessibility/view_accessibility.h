// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_H_
#define UI_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/views_export.h"

namespace ui {

class AXPlatformNodeDelegate;

}  // namespace ui

namespace views {

class View;
class ViewsAXTreeManager;
class Widget;

// An object that manages the accessibility interface for a View.
//
// The default accessibility properties of a View is determined by calling
// |View::GetAccessibleNodeData()|, which is overridden by many |View|
// subclasses. |ViewAccessibility| lets you override these for a particular
// view.
//
// In most cases, subclasses of |ViewAccessibility| own the |AXPlatformNode|
// that implements the native accessibility APIs on a specific platform.
class VIEWS_EXPORT ViewAccessibility {
 public:
  using AccessibilityEventsCallback =
      base::RepeatingCallback<void(const ui::AXPlatformNodeDelegate*,
                                   const ax::mojom::Event)>;
  using AXVirtualViews = AXVirtualView::AXVirtualViews;

  static std::unique_ptr<ViewAccessibility> Create(View* view);

  ViewAccessibility(const ViewAccessibility&) = delete;
  ViewAccessibility& operator=(const ViewAccessibility&) = delete;
  virtual ~ViewAccessibility();

  // Modifies |node_data| to reflect the current accessible state of the
  // associated View, taking any custom overrides into account
  // (see OverrideFocus, OverrideRole, etc. below).
  virtual void GetAccessibleNodeData(ui::AXNodeData* node_data) const;

  //
  // The following methods get or set accessibility attributes (in the owning
  // View's AXNodeData), overrideing any identical attributes which might have
  // been set by the owning View in its View::GetAccessibleNodeData() method.
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

  void OverrideRole(const ax::mojom::Role role);
  void OverrideName(const std::string& name);
  void OverrideName(const base::string16& name);
  void OverrideDescription(const std::string& description);
  void OverrideDescription(const base::string16& description);

  // Sets whether this View hides all its descendants from the accessibility
  // tree that is exposed to platform APIs. This is similar, but not exactly
  // identical to aria-hidden="true".
  //
  // Note that this attribute does not cross widget boundaries, i.e. if a sub
  // widget is a descendant of this View, it will not be marked hidden. This
  // should not happen in practice as widgets are not children of Views.
  void OverrideIsLeaf(bool value);
  virtual bool IsLeaf() const;

  // Returns true if an ancestor of this node (not including itself) is a
  // leaf node, meaning that this node is not actually exposed to any
  // platform's accessibility layer.
  virtual bool IsChildOfLeaf() const;

  // Hides this View from the accessibility tree that is exposed to platform
  // APIs.
  void OverrideIsIgnored(bool value);
  virtual bool IsIgnored() const;

  // Marks this View either as enabled or disabled (grayed out) in the
  // accessibility tree and ignores the View's real enabled state. Does not
  // affect the View's focusable state (see "IsAccessibilityFocusable()").
  // Screen readers make a special announcement when an item is disabled.
  //
  // It might not be advisable to mark a View as enabled in the accessibility
  // tree, whilst the real View is actually disabled, because such a View will
  // not respond to user actions.
  void OverrideIsEnabled(bool enabled);
  virtual bool IsAccessibilityEnabled() const;

  void OverrideBounds(const gfx::RectF& bounds);
  void OverrideDescribedBy(View* described_by_view);
  void OverrideHasPopup(const ax::mojom::HasPopup has_popup);

  // Override information provided to users by screen readers when describing
  // elements in a menu, listbox, or another set-like item. For example, "New
  // tab, menu item 1 of 5". If not specified, a view's index in its parent and
  // its parent's number of children provide the values for |pos_in_set| and
  // |set_size| respectively.
  //
  // Note that |pos_in_set| is one-based, i.e. it starts from 1 not 0.
  void OverridePosInSet(int pos_in_set, int set_size);

  // Override the next or previous focused widget. Some assistive technologies,
  // such as screen readers, may utilize this information to transition focus
  // from the beginning or end of one widget to another when navigating by its
  // default navigation method.
  void OverrideNextFocus(Widget* widget);
  void OverridePreviousFocus(Widget* widget);
  Widget* GetNextFocus() const;
  Widget* GetPreviousFocus() const;

  // Returns the accessibility object that represents the View whose
  // accessibility is managed by this instance. This may be an AXPlatformNode or
  // it may be a native accessible object implemented by another class.
  virtual gfx::NativeViewAccessible GetNativeObject() const;

  virtual void NotifyAccessibilityEvent(ax::mojom::Event event_type);

  // Causes the screen reader to announce |text|. If the current user is not
  // using a screen reader, has no effect.
  virtual void AnnounceText(const base::string16& text);

  virtual const ui::AXUniqueId& GetUniqueId() const;

  View* view() const { return view_; }
  AXVirtualView* FocusedVirtualChild() const { return focused_virtual_child_; }
  ViewsAXTreeManager* AXTreeManager() const;

  //
  // Methods for managing virtual views.
  //

  // Adds |virtual_view| as a child of this View. We take ownership of our
  // virtual children.
  void AddVirtualChildView(std::unique_ptr<AXVirtualView> virtual_view);

  // Adds |virtual_view| as a child of this View at an index.
  // We take ownership of our virtual children.
  void AddVirtualChildViewAt(std::unique_ptr<AXVirtualView> virtual_view,
                             int index);

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

  // Returns the index of |virtual_view|, or -1 if |virtual_view| is not a child
  // of this View.
  int GetIndexOf(const AXVirtualView* virtual_view) const;

  // Returns the native accessibility object associated with the AXVirtualView
  // descendant that is currently focused. If no virtual descendants are
  // present, or no virtual descendant has been marked as focused, returns the
  // native accessibility object associated with this view.
  gfx::NativeViewAccessible GetFocusedDescendant();

  // Used for testing. Allows a test to watch accessibility events.
  const AccessibilityEventsCallback& accessibility_events_callback() const;
  void set_accessibility_events_callback(AccessibilityEventsCallback callback);

 protected:
  explicit ViewAccessibility(View* view);

  // Used for testing. Called every time an accessibility event is fired.
  AccessibilityEventsCallback accessibility_events_callback_;

 private:
  // Weak. Owns this.
  View* const view_;

  // If there are any virtual children, they override any real children.
  // We own our virtual children.
  AXVirtualViews virtual_children_;

  // The virtual child that is currently focused.
  // This is nullptr if no virtual child is focused.
  // See also OverrideFocus() and GetFocusedDescendant().
  AXVirtualView* focused_virtual_child_;

  const ui::AXUniqueId unique_id_;

  // Contains data set explicitly via OverrideRole, OverrideName, etc. that
  // overrides anything provided by GetAccessibleNodeData().
  ui::AXNodeData custom_data_;

  // If set to true, anything that is a descendant of this view will be hidden
  // from accessibility.
  bool is_leaf_;

  // When true the view is ignored when generating the AX node hierarchy, but
  // its children are included. For example, if you created a custom table with
  // the digits 1 - 9 arranged in a 3 x 3 grid, marking the table and rows
  // "ignored" would mean that the digits 1 - 9 would appear as if they were
  // immediate children of the root. Likewise "internal" container views can be
  // ignored, like a Widget's RootView, ClientView, etc.
  // Similar to setting the role of an ARIA widget to "none" or
  // "presentational".
  bool is_ignored_;

  // Used to override the View's enabled state in case we need to mark the View
  // as enabled or disabled only in the accessibility tree.
  base::Optional<bool> is_enabled_ = base::nullopt;

  // Used by the Views system to help some assistive technologies, such as
  // screen readers, transition focus from one widget to another.
  Widget* next_focus_ = nullptr;
  Widget* previous_focus_ = nullptr;

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // Each instance of ViewAccessibility that's associated with a root View
  // owns an ViewsAXTreeManager. For other Views, this should be nullptr.
  std::unique_ptr<views::ViewsAXTreeManager> ax_tree_manager_;
#endif
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_H_
