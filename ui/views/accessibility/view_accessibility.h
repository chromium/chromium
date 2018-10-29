// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_H_
#define UI_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/views_export.h"

namespace views {

class View;

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
  static std::unique_ptr<ViewAccessibility> Create(View* view);

  virtual ~ViewAccessibility();

  // Modifies |node_data| to reflect the current accessible state of the
  // associated View, taking any custom overrides into account
  // (see OverrideRole, etc. below).
  virtual void GetAccessibleNodeData(ui::AXNodeData* node_data) const;

  //
  // These override accessibility information, including properties returned
  // from View::GetAccessibleNodeData().
  // Note that string attributes are only used if non-empty, so you can't
  // override a string with the empty string.
  //
  void OverrideRole(const ax::mojom::Role role);
  void OverrideName(const std::string& name);
  void OverrideName(const base::string16& name);
  void OverrideDescription(const std::string& description);
  void OverrideDescription(const base::string16& description);
  void OverrideIsLeaf();  // Force this node to be treated as a leaf node.

  virtual gfx::NativeViewAccessible GetNativeObject();
  virtual void NotifyAccessibilityEvent(ax::mojom::Event event_type) {}
#if defined(OS_MACOSX)
  virtual void AnnounceText(base::string16& text) {}
#endif

  virtual const ui::AXUniqueId& GetUniqueId() const;

  bool IsLeaf() const;

  bool is_ignored() const { return is_ignored_; }
  void set_is_ignored(bool ignored) { is_ignored_ = ignored; }

  //
  // Methods for managing virtual views.
  //

  // Adds |virtual_view| as a child of this View, optionally at |index|.
  // We take ownership of our virtual children.
  void AddVirtualChildView(std::unique_ptr<AXVirtualView> virtual_view);
  void AddVirtualChildViewAt(std::unique_ptr<AXVirtualView> virtual_view,
                             int index);

  // Removes |virtual_view| from this View. The virtual view's parent will
  // change to nullptr. Hands ownership back to the caller.
  std::unique_ptr<AXVirtualView> RemoveVirtualChildView(
      AXVirtualView* virtual_view);

  // Removes all the virtual children from this View.
  // The virtual views are deleted.
  void RemoveAllVirtualChildViews();

  int virtual_child_count() const {
    return static_cast<int>(virtual_children_.size());
  }

  const AXVirtualView* virtual_child_at(int index) const {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, virtual_child_count());
    return virtual_children_[index].get();
  }

  // Returns the index of |virtual_view|, or -1 if |virtual_view| is not a child
  // of this View.
  int GetIndexOf(const AXVirtualView* virtual_view) const;

 protected:
  explicit ViewAccessibility(View* view);

  View* view() const { return owner_view_; }

 private:
  // Weak. Owns this.
  View* const owner_view_;

  // If there are any virtual children, they override any real children.
  // We own our virtual children.
  std::vector<std::unique_ptr<AXVirtualView>> virtual_children_;

  const ui::AXUniqueId unique_id_;

  // Contains data set explicitly via SetRole, SetName, etc. that overrides
  // anything provided by GetAccessibleNodeData().
  ui::AXNodeData custom_data_;

  bool is_leaf_;

  // When true the view is ignored when generating the AX node hierarchy, but
  // its children are included. For example, if you created a custom table with
  // the digits 1 - 9 arranged in a 3 x 3 grid, marking the table and rows
  // "ignored" would mean that the digits 1 - 9 would appear as if they were
  // immediate children of the root. Likewise "internal" container views can be
  // ignored, like a Widget's RootView, ClientView, etc.
  bool is_ignored_ = false;

  DISALLOW_COPY_AND_ASSIGN(ViewAccessibility);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_H_
