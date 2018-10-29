// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_VIRTUAL_VIEW_H_
#define UI_VIEWS_ACCESSIBILITY_AX_VIRTUAL_VIEW_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_base.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace ui {

struct AXActionData;
class AXUniqueId;

}  // namespace ui

namespace views {

class View;
class ViewAccessibility;

// Implements a virtual view that is used only for accessibility.
//
// Some composite widgets such as tree and table views may utilize lightweight
// UI objects instead of actual views for displaying and managing their
// contents. We need a corresponding virtual accessibility view to expose
// information about these lightweight Ui objects to accessibility. An
// AXVirtualView is owned by its parent, which could either be a
// ViewAccessibility or an AXVirtualView.
class VIEWS_EXPORT AXVirtualView : public ui::AXPlatformNodeDelegateBase {
 public:
  AXVirtualView();
  ~AXVirtualView() override;

  //
  // Methods for managing parent - child relationships.
  //

  // Adds |view| as a child of this virtual view, optionally at |index|.
  // We take ownership of our children.
  void AddChildView(std::unique_ptr<AXVirtualView> view);
  void AddChildViewAt(std::unique_ptr<AXVirtualView> view, int index);

  // Moves |view| to the specified |index|. A negative value for |index| moves
  // |view| to the end.
  void ReorderChildView(AXVirtualView* view, int index);

  // Removes |view| from this virtual view. The view's parent will change to
  // nullptr. Hands ownership back to the caller.
  std::unique_ptr<AXVirtualView> RemoveChildView(AXVirtualView* view);

  // Removes all the children from this virtual view.
  // The virtual views are deleted.
  void RemoveAllChildViews();

  bool has_children() const { return !children_.empty(); }

  const AXVirtualView* child_at(int index) const;
  AXVirtualView* child_at(int index);

  // Returns the parent view if the parent is a real View and not an
  // AXVirtualView. Returns nullptr otherwise.
  const View* parent_view() const { return parent_view_; }
  View* parent_view() { return parent_view_; }

  // Returns the parent view if the parent is an AXVirtualView and not a real
  // View. Returns nullptr otherwise.
  const AXVirtualView* virtual_parent_view() const {
    return virtual_parent_view_;
  }
  AXVirtualView* virtual_parent_view() { return virtual_parent_view_; }

  // Returns true if |view| is contained within the hierarchy of this
  // AXVirtualView, even as an indirect descendant. Will return true if |view|
  // is also this AXVirtualView.
  bool Contains(const AXVirtualView* view) const;

  // Returns the index of |view|, or -1 if |view| is not a child of this virtual
  // view.
  int GetIndexOf(const AXVirtualView* view) const;

  //
  // Methods also found in ViewAccessibility.
  //

  gfx::NativeViewAccessible GetNativeObject() const;
  void NotifyAccessibilityEvent(ax::mojom::Event event_type);
  void OverrideRole(const ax::mojom::Role role);
  void OverrideState(ax::mojom::State state);
  void OverrideName(const std::string& name);
  void OverrideName(const base::string16& name);
  void OverrideDescription(const std::string& description);
  void OverrideDescription(const base::string16& description);
  void OverrideBoundsRect(const gfx::RectF& location);

  // ui::AXPlatformNodeDelegate
  const ui::AXNodeData& GetData() const override;
  int GetChildCount() override;
  gfx::NativeViewAccessible ChildAtIndex(int index) override;
  gfx::NativeWindow GetTopLevelWidget() override;
  gfx::NativeViewAccessible GetParent() override;
  gfx::Rect GetClippedScreenBoundsRect() const override;
  gfx::Rect GetUnclippedScreenBoundsRect() const override;
  gfx::NativeViewAccessible HitTestSync(int x, int y) override;
  gfx::NativeViewAccessible GetFocus() override;
  ui::AXPlatformNode* GetFromNodeID(int32_t id) override;
  bool AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool ShouldIgnoreHoveredStateForTesting() override;
  bool IsOffscreen() const override;
  const ui::AXUniqueId& GetUniqueId() const override;

 private:
  // Sets the parent view if the parent is a real View and not an AXVirtualView.
  // It is invalid to set both |parent_view_| and |virtual_parent_view_|.
  void set_parent_view(View* view) {
    DCHECK(!virtual_parent_view_);
    parent_view_ = view;
  }

  bool IsParentVisible() const;

  // We own this, but it is reference-counted on some platforms so we can't use
  // a unique_ptr. It is destroyed in the destructor.
  ui::AXPlatformNode* ax_platform_node_;

  // Weak. Owns us if not nullptr.
  // Either |parent_view_| or |virtual_parent_view_| should be set but not both.
  View* parent_view_;

  // Weak. Owns us if not nullptr.
  // Either |parent_view_| or |virtual_parent_view_| should be set but not both.
  AXVirtualView* virtual_parent_view_;

  // We own our children.
  std::vector<std::unique_ptr<AXVirtualView>> children_;

  ui::AXUniqueId unique_id_;
  mutable ui::AXNodeData custom_data_;

  friend class ViewAccessibility;
  DISALLOW_COPY_AND_ASSIGN(AXVirtualView);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_VIRTUAL_VIEW_H_
