// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_H_
#define UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_H_

#include <stdint.h>

#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_base.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {

struct AXActionData;
class AXUniqueId;

}  // namespace ui

namespace views {

class View;

// Shared base class for platforms that require an implementation of
// |ViewAXPlatformNodeDelegate| to interface with the native accessibility
// toolkit. This class owns the |AXPlatformNode|, which implements those native
// APIs.
class ViewAXPlatformNodeDelegate : public ViewAccessibility,
                                   public ui::AXPlatformNodeDelegateBase {
 public:
  ViewAXPlatformNodeDelegate(const ViewAXPlatformNodeDelegate&) = delete;
  ViewAXPlatformNodeDelegate& operator=(const ViewAXPlatformNodeDelegate&) =
      delete;
  ~ViewAXPlatformNodeDelegate() override;

  // ViewAccessibility:
  bool IsAccessibilityFocusable() const override;
  bool IsFocusedForTesting() const override;
  void SetPopupFocusOverride() override;
  void EndPopupFocusOverride() override;
  void FireFocusAfterMenuClose() override;
  bool IsIgnored() const override;
  bool IsAccessibilityEnabled() const override;
  gfx::NativeViewAccessible GetNativeObject() const override;
  void NotifyAccessibilityEvent(ax::mojom::Event event_type) override;
#if defined(OS_APPLE)
  void AnnounceText(const base::string16& text) override;
#endif

  // ui::AXPlatformNodeDelegate.
  const ui::AXNodeData& GetData() const override;
  int GetChildCount() const override;
  gfx::NativeViewAccessible ChildAtIndex(int index) override;
  bool HasModalDialog() const override;
  // Also in |ViewAccessibility|.
  bool IsChildOfLeaf() const override;
  gfx::NativeViewAccessible GetNSWindow() override;
  // TODO(nektar): Make "GetNativeViewAccessible" a const method throughout the
  // codebase.
  gfx::NativeViewAccessible GetNativeViewAccessible() const;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  gfx::NativeViewAccessible GetParent() override;
  bool IsLeaf() const override;
  bool IsInvisibleOrIgnored() const override;
  bool IsFocused() const override;
  bool IsToplevelBrowserWindow() override;
  gfx::Rect GetBoundsRect(
      const ui::AXCoordinateSystem coordinate_system,
      const ui::AXClippingBehavior clipping_behavior,
      ui::AXOffscreenResult* offscreen_result) const override;
  gfx::NativeViewAccessible HitTestSync(
      int screen_physical_pixel_x,
      int screen_physical_pixel_y) const override;
  gfx::NativeViewAccessible GetFocus() const override;
  ui::AXPlatformNode* GetFromNodeID(int32_t id) override;
  ui::AXPlatformNode* GetFromTreeIDAndNodeID(const ui::AXTreeID& ax_tree_id,
                                             int32_t id) override;
  bool AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool ShouldIgnoreHoveredStateForTesting() override;
  bool IsOffscreen() const override;
  base::string16 GetAuthorUniqueId() const override;
  bool IsMinimized() const override;
  // Also in |ViewAccessibility|.
  const ui::AXUniqueId& GetUniqueId() const override;
  base::Optional<bool> GetTableHasColumnOrRowHeaderNode() const override;
  std::vector<int32_t> GetColHeaderNodeIds() const override;
  std::vector<int32_t> GetColHeaderNodeIds(int col_index) const override;
  base::Optional<int32_t> GetCellId(int row_index,
                                    int col_index) const override;
  bool IsOrderedSetItem() const override;
  bool IsOrderedSet() const override;
  base::Optional<int> GetPosInSet() const override;
  base::Optional<int> GetSetSize() const override;

 protected:
  explicit ViewAXPlatformNodeDelegate(View* view);

  friend class ViewAccessibility;
  // Called by ViewAccessibility::Create immediately after
  // construction. Used to avoid issues with calling virtual functions
  // during the constructor.
  virtual void Init();

  ui::AXPlatformNode* ax_platform_node() { return ax_platform_node_; }

 private:
  struct ChildWidgetsResult final {
    ChildWidgetsResult();
    ChildWidgetsResult(std::vector<Widget*> child_widgets,
                       bool is_tab_modal_showing);
    ChildWidgetsResult(const ChildWidgetsResult& other);
    virtual ~ChildWidgetsResult();
    ChildWidgetsResult& operator=(const ChildWidgetsResult& other);

    std::vector<Widget*> child_widgets;

    // When the focus is within a child widget, |child_widgets| contains only
    // that widget. Otherwise, |child_widgets| contains all child widgets.
    //
    // The former arises when a modal dialog is showing. In order to support the
    // "read title (NVDAKey+T)" and "read window (NVDAKey+B)" commands in the
    // NVDA screen reader, we need to hide the rest of the UI from the
    // accessibility tree for these commands to work properly.
    bool is_tab_modal_showing = false;
  };

  // Uses Views::GetViewsInGroup to find nearby Views in the same group.
  // Searches from the View's parent to include siblings within that group.
  void GetViewsInGroupForSet(std::vector<View*>* views_in_group) const;

  // If this delegate is attached to the root view, returns all the child
  // widgets of this view's owning widget.
  ChildWidgetsResult GetChildWidgets() const;

  // Gets the real (non-virtual) TableView, otherwise nullptr.
  TableView* GetAncestorTableView() const;

  // We own this, but it is reference-counted on some platforms so we can't use
  // a unique_ptr. It is destroyed in the destructor.
  ui::AXPlatformNode* ax_platform_node_ = nullptr;

  mutable ui::AXNodeData data_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_H_
