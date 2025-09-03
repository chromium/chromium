// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_VIRTUAL_VIEW_H_
#define UI_VIEWS_ACCESSIBILITY_AX_VIRTUAL_VIEW_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/views_export.h"

#if defined(USE_AURA)
#include "ui/views/accessibility/ax_virtual_view_wrapper.h"
#endif

namespace ui {

struct AXActionData;

}  // namespace ui

namespace views {

class AXAuraObjCache;
class AXVirtualViewWrapper;
class View;
class ViewAXPlatformNodeDelegate;

namespace test {
class AXVirtualViewTest;
}  // namespace test

// Implements a virtual view that is used only for accessibility.
//
// Some composite widgets such as tree and table views may utilize lightweight
// UI objects instead of actual views for displaying and managing their
// contents. We need a corresponding virtual accessibility view to expose
// information about these lightweight Ui objects to accessibility. An
// AXVirtualView is owned by its parent, which could either be a
// ViewAccessibility or an AXVirtualView.
class VIEWS_EXPORT AXVirtualView : public ViewAccessibility,
                                   public ui::AXPlatformNodeDelegate {
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
  void AddChildViewAt(std::unique_ptr<AXVirtualView> view, size_t index);

  // Moves |view| to the specified |index|. A too-large value for |index| moves
  // |view| to the end.
  void ReorderChildView(AXVirtualView* view, size_t index);

  // Removes this virtual view from its parent, which could either be a virtual
  // or a real view. Hands ownership of this view back to the caller.
  std::unique_ptr<AXVirtualView> RemoveFromParentView();

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

  ui::AXPlatformNode* ax_platform_node() { return ax_platform_node_.get(); }

  // Returns true if |view| is contained within the hierarchy of this
  // AXVirtualView, even as an indirect descendant. Will return true if |view|
  // is also this AXVirtualView.
  bool Contains(const AXVirtualView* view) const;

  // Returns the index of |view|, or nullopt if |view| is not a child of this
  // virtual view.
  std::optional<size_t> GetIndexOf(const AXVirtualView* view) const;

  //
  // Other methods.
  //

  const char* GetViewClassName() const;
  gfx::NativeViewAccessible GetNativeObject() const override;

  Widget* GetWidget() const override;
  // TODO(crbug.com/40672441): Rename to GetParent once ViewsAX is completed and
  // AXVirtualView no longer needs to extend AXPlatformNodeDelegate.
  ViewAccessibility* GetViewAccessibilityParent() const override;

  std::string GetDebugString() const override;

  // ui::AXPlatformNodeDelegate. Note that
  // - Some of these functions have Mac-specific implementations in
  //   ax_virtual_view_mac.mm.
  // - GetChildCount(), ChildAtIndex(), and GetParent() are used by assistive
  //   technologies to access the unignored accessibility tree, which doesn't
  //   necessarily reflect the internal descendant tree. (An ignored node means
  //   that the node should not be exposed to the platform.)
  const ui::AXNodeData& GetData() const override;
  size_t GetChildCount() const override;
  gfx::NativeViewAccessible ChildAtIndex(size_t index) const override;
  gfx::NativeViewAccessible GetNSWindow() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  gfx::NativeViewAccessible GetParent() const override;
  using AXPlatformNodeDelegate::GetBoundsRect;
  gfx::Rect GetBoundsRect(
      const ui::AXCoordinateSystem coordinate_system,
      const ui::AXClippingBehavior clipping_behavior,
      ui::AXOffscreenResult* offscreen_result) const override;
  gfx::NativeViewAccessible HitTestSync(
      int screen_physical_pixel_x,
      int screen_physical_pixel_y) const override;
  gfx::NativeViewAccessible GetFocus() const override;
  ui::AXPlatformNode* GetFromNodeID(int32_t id) override;
  bool AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool ShouldIgnoreHoveredStateForTesting() override;
  bool IsOffscreen() const override;
  ui::AXPlatformNodeId GetUniqueId() const override;
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;
  std::vector<int32_t> GetColHeaderNodeIds() const override;
  std::vector<int32_t> GetColHeaderNodeIds(int col_index) const override;
  std::optional<int32_t> GetCellId(int row_index, int col_index) const override;

  // Gets the real View that owns our shallowest virtual ancestor,, if any.
  View* GetOwnerView() const;

  // Gets the delegate for our owning View; if we are on a platform that exposes
  // Views directly to platform APIs instead of serializing them into an AXTree.
  // Otherwise, returns nullptr.
  ViewAXPlatformNodeDelegate* GetDelegate() const;

  // Gets or creates a wrapper suitable for use with tree sources.
  AXVirtualViewWrapper* GetOrCreateWrapper(views::AXAuraObjCache* cache);

  // Handle a request from assistive technology to perform an action on this
  // virtual view. Returns true on success, but note that the success/failure is
  // not propagated to the client that requested the action, since the
  // request is sometimes asynchronous. The right way to send a response is
  // via NotifyAccessibilityEventDeprecated().
  virtual bool HandleAccessibleAction(const ui::AXActionData& action_data);

  // Prune/Unprune all descendant virtual views from the tree. As of right now,
  // these should only be called by their ViewAccessibility counterparts. This
  // is for a scenario such as the following: ViewAccessibility A has a child
  // AXVirtualView B, which has a child AXVirtualView C:
  // A
  //  B
  //   C
  // A->SetIsLeaf(true) is called. B and C then should be pruned from the tree
  // and marked as ignored.
  void PruneVirtualSubtree();
  void UnpruneVirtualSubtree();

  // Warning: This method will overwrite the focusable state. In most
  // cases, we compute the focusable state automatically in
  // `UpdateFocusableState`, however, AXVirtualViews are not Views, so we need
  // to provide a way to set their focusable state manually, similar to
  // the `FocusBehavior` in Views.
  void ForceSetIsFocusable(bool focusable);
  void ResetIsFocusable();

  // This is also called by `ViewAccessibility::OnViewHasNewAncestor`, to update
  // the relevant attributes on its AXVirtualView subtree.
  void OnViewHasNewAncestor(bool ancestor_focusable);

  void OnViewHasNewAncestor(const AXVirtualView* new_ancestor);

  // `ViewAccessibility` overrides.
  void NotifyEvent(ax::mojom::Event event_type,
                   bool send_native_event) override;
  void NotifyDataChanged() override;
  void UpdateFocusableState() override;
  void UpdateInvisibleState() override;
  void OnWidgetClosing(Widget* widget) override;
  void OnWidgetDestroyed(Widget* widget) override;
  void OnWidgetUpdated(Widget* widget, Widget* old_widget) override;
  void UpdateReadyToNotifyEvents() override;
  void UpdateIgnoredState() override;
  void SetIsEnabled(bool enabled) override;
  void SetShowContextMenu(bool show_context_menu) override;

  bool parent_view_is_drawn() const { return parent_view_is_drawn_; }

 protected:
  // Forwards a request from assistive technology to perform an action on this
  // virtual view to the owner view's accessible action handler.
  bool HandleAccessibleActionInOwnerView(const ui::AXActionData& action_data);

 private:
  // Needed in order to access set_cache(), so that AXAuraObjCache can
  // track when an AXVirtualViewWrapper is deleted.
  friend class AXAuraObjCache;
  friend class AXVirtualViewWrapper;
  friend class views::test::AXVirtualViewTest;

  // Internal class name.
  static const char kViewClassName[];

  // The AXAuraObjCache associated with our wrapper, if any. This is
  // called by friend classes AXAuraObjCache and AXVirtualViewWrapper.
  void set_cache(AXAuraObjCache* cache);

  // Sets the parent ViewAccessibility if the parent is a real View and not an
  // AXVirtualView. It is invalid to set both |parent_view_| and
  // |virtual_parent_view_|.
  void set_parent_view(ViewAccessibility* view_accessibility) {
    DCHECK(!virtual_parent_view_);
    parent_view_ = view_accessibility;
  }

  // AXVirtualViews should be marked as disabled if their
  // owner View is disabled.
  void SetIsEnabledRecursive(bool enabled);

  void SetShowContextMenuRecursive(bool show_context_menu);

  void UpdateParentViewIsDrawnRecursive(const views::View* initial_view,
                                        bool parent_view_is_drawn);

  ui::AXPlatformNode::Pointer ax_platform_node_;

  // Weak. Owns us if not nullptr.
  // Either |parent_view_| or |virtual_parent_view_| should be set but not both.
  raw_ptr<ViewAccessibility> parent_view_ = nullptr;

  // Weak. Owns us if not nullptr.
  // Either |parent_view_| or |virtual_parent_view_| should be set but not both.
  raw_ptr<AXVirtualView> virtual_parent_view_ = nullptr;

  // We own our children.
  AXVirtualViews children_;

  // The AXAuraObjCache that owns the AXVirtualViewWrapper associated with
  // this object, if any.
  raw_ptr<AXAuraObjCache> ax_aura_obj_cache_ = nullptr;

  std::optional<bool> should_be_focusable_ = std::nullopt;

  bool parent_view_is_drawn_ = true;

  friend class ViewAccessibility;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_VIRTUAL_VIEW_H_
