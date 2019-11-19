// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_VIRTUAL_VIEW_H_
#define UI_VIEWS_ACCESSIBILITY_AX_VIRTUAL_VIEW_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_base.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

#if defined(USE_AURA)
#include "ui/views/accessibility/ax_virtual_view_wrapper.h"
#else
// Currently unused.
class AXVirtualViewWrapper {};
#endif

namespace ui {

struct AXActionData;
class AXUniqueId;

}  // namespace ui

namespace views {

class AXAuraObjCache;
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
  using AXVirtualViews = std::vector<std::unique_ptr<AXVirtualView>>;

  static AXVirtualView* GetFromId(int32_t id);

  AXVirtualView();
  AXVirtualView(const AXVirtualView&) = delete;
  AXVirtualView& operator=(const AXVirtualView&) = delete;
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

  const AXVirtualViews& children() const { return children_; }

  // Returns the parent ViewAccessibility if the parent is a real View and not
  // an AXVirtualView. Returns nullptr otherwise.
  const ViewAccessibility* parent_view() const { return parent_view_; }
  ViewAccessibility* parent_view() { return parent_view_; }

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
  // Other methods.
  //

  const char* GetViewClassName() const;
  gfx::NativeViewAccessible GetNativeObject() const;
  void NotifyAccessibilityEvent(ax::mojom::Event event_type);
  // Allows clients to modify the AXNodeData for this virtual view. This should
  // be used for attributes that are relatively stable and do not change
  // dynamically.
  ui::AXNodeData& GetCustomData();
  // Allows clients to modify the AXNodeData for this virtual view dynamically
  // via a callback. This should be used for attributes that change often and
  // would be queried every time a client accesses this view's AXNodeData.
  void SetPopulateDataCallback(
      base::RepeatingCallback<void(const View&, ui::AXNodeData*)> callback);
  void UnsetPopulateDataCallback();

  // ui::AXPlatformNodeDelegate. Note that some of these functions have
  // Mac-specific implementations in ax_virtual_view_mac.mm.
  const ui::AXNodeData& GetData() const override;
  int GetChildCount() override;
  gfx::NativeViewAccessible ChildAtIndex(int index) override;
  gfx::NativeViewAccessible GetNSWindow() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  gfx::NativeViewAccessible GetParent() override;
  gfx::Rect GetBoundsRect(
      const ui::AXCoordinateSystem coordinate_system,
      const ui::AXClippingBehavior clipping_behavior,
      ui::AXOffscreenResult* offscreen_result) const override;
  gfx::NativeViewAccessible HitTestSync(int x, int y) override;
  gfx::NativeViewAccessible GetFocus() override;
  ui::AXPlatformNode* GetFromNodeID(int32_t id) override;
  bool AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool ShouldIgnoreHoveredStateForTesting() override;
  bool IsOffscreen() const override;
  const ui::AXUniqueId& GetUniqueId() const override;
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;

  // Gets the real View that owns our shallowest virtual ancestor,, if any.
  View* GetOwnerView() const;

  // Gets or creates a wrapper suitable for use with tree sources.
  AXVirtualViewWrapper* GetOrCreateWrapper(views::AXAuraObjCache* cache);

  // Handle a request from assistive technology to perform an action on this
  // virtual view. Returns true on success, but note that the success/failure is
  // not propagated to the client that requested the action, since the
  // request is sometimes asynchronous. The right way to send a response is
  // via NotifyAccessibilityEvent().
  virtual bool HandleAccessibleAction(const ui::AXActionData& action_data);

 private:
  // Internal class name.
  static const char kViewClassName[];

  // Sets the parent ViewAccessibility if the parent is a real View and not an
  // AXVirtualView. It is invalid to set both |parent_view_| and
  // |virtual_parent_view_|.
  void set_parent_view(ViewAccessibility* view_accessibility) {
    DCHECK(!virtual_parent_view_);
    parent_view_ = view_accessibility;
  }

  // We own this, but it is reference-counted on some platforms so we can't use
  // a unique_ptr. It is destroyed in the destructor.
  ui::AXPlatformNode* ax_platform_node_;

  // Weak. Owns us if not nullptr.
  // Either |parent_view_| or |virtual_parent_view_| should be set but not both.
  ViewAccessibility* parent_view_ = nullptr;

  // Weak. Owns us if not nullptr.
  // Either |parent_view_| or |virtual_parent_view_| should be set but not both.
  AXVirtualView* virtual_parent_view_ = nullptr;

  // We own our children.
  AXVirtualViews children_;

  ui::AXUniqueId unique_id_;
  ui::AXNodeData custom_data_;
  base::RepeatingCallback<void(const View&, ui::AXNodeData*)>
      populate_data_callback_;

  std::unique_ptr<AXVirtualViewWrapper> wrapper_;

  friend class ViewAccessibility;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_VIRTUAL_VIEW_H_
