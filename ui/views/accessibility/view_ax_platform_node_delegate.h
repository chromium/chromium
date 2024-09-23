// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_H_
#define UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {

struct AXActionData;

}  // namespace ui

namespace views {

class AtomicViewAXTreeManager;
class View;

// Shared base class for platforms that require an implementation of
// |ViewAXPlatformNodeDelegate| to interface with the native accessibility
// toolkit. This class owns the |AXPlatformNode|, which implements those native
// APIs.
class VIEWS_EXPORT ViewAXPlatformNodeDelegate
    : public ViewAccessibility,
      public ui::AXPlatformNodeDelegate {
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
  bool GetIsIgnored() const override;
  gfx::NativeViewAccessible GetNativeObject() const override;
  void FireNativeEvent(ax::mojom::Event event_type) override;
#if BUILDFLAG(IS_MAC)
  void AnnounceTextAs(const std::u16string& text,
                      ui::AXPlatformNode::AnnouncementType announcement_type);
#endif

  // ui::AXPlatformNodeDelegate.
  const ui::AXNodeData& GetData() const override;
  size_t GetChildCount() const override;
  gfx::NativeViewAccessible ChildAtIndex(size_t index) const override;
  bool HasModalDialog() const override;
  std::wstring ComputeListItemNameFromContent() const override;
  // Also in |ViewAccessibility|.
  bool IsChildOfLeaf() const override;
  const ui::AXSelection GetUnignoredSelection() const override;
  ui::AXNodePosition::AXPositionInstance CreatePositionAt(
      int offset,
      ax::mojom::TextAffinity affinity =
          ax::mojom::TextAffinity::kDownstream) const override;
  ui::AXNodePosition::AXPositionInstance CreateTextPositionAt(
      int offset,
      ax::mojom::TextAffinity affinity) const override;
  gfx::NativeViewAccessible GetNSWindow() override;
  // TODO(nektar): Make "GetNativeViewAccessible" a const method throughout the
  // codebase.
  gfx::NativeViewAccessible GetNativeViewAccessible() const;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  gfx::NativeViewAccessible GetParent() const override;
  bool IsLeaf() const override;
  bool IsInvisibleOrIgnored() const override;
  bool IsFocused() const override;
  bool IsToplevelBrowserWindow() override;
  gfx::Rect GetBoundsRect(
      const ui::AXCoordinateSystem coordinate_system,
      const ui::AXClippingBehavior clipping_behavior,
      ui::AXOffscreenResult* offscreen_result) const override;
  gfx::Rect GetInnerTextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
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
  std::u16string GetAuthorUniqueId() const override;
  bool IsMinimized() const override;
  bool IsReadOnlySupported() const override;
  bool IsReadOnlyOrDisabled() const override;

  // Also in |ViewAccessibility|.
  ui::AXPlatformNodeId GetUniqueId() const override;
  std::vector<int32_t> GetColHeaderNodeIds() const override;
  std::vector<int32_t> GetColHeaderNodeIds(int col_index) const override;
  std::optional<int32_t> GetCellId(int row_index, int col_index) const override;
  bool IsOrderedSetItem() const override;
  bool IsOrderedSet() const override;
  std::optional<int> GetPosInSet() const override;
  std::optional<int> GetSetSize() const override;

  bool TableHasColumnOrRowHeaderNodeForTesting() const;

  // Return the bounds of inline text in this node's coordinate system.
  gfx::RectF GetInlineTextRect(const int start_offset,
                               const int end_offset) const;

  // Return the bounds relative to the container bounds. This functions applies
  // the horizontal scroll offset and clips the bounds to the container bounds.
  // TODO(accessibility): Add support for vertical scroll offsets if needed.
  // There's no known use case for this yet.
  gfx::RectF RelativeToContainerBounds(
      const gfx::RectF& bounds,
      ui::AXOffscreenResult* offscreen_result) const;

  AtomicViewAXTreeManager* GetAtomicViewAXTreeManagerForTesting()
      const override;

  virtual gfx::Point ScreenToDIPPoint(const gfx::Point& screen_point) const;

 protected:
  explicit ViewAXPlatformNodeDelegate(View* view);

  friend class ViewAccessibility;
  // Called by ViewAccessibility::Create immediately after
  // construction. Used to avoid issues with calling virtual functions
  // during the constructor.
  virtual void Init();

  ui::AXNodeData data() { return data_; }
  ui::AXPlatformNode* ax_platform_node() { return ax_platform_node_; }

  // Manager for the accessibility tree for this view. The tree will only have
  // one node, which contains the AXNodeData for this view. It's a temporary
  // solution to enable the ITextRangeProvider in Views: crbug.com/1468416.
  std::unique_ptr<AtomicViewAXTreeManager> atomic_view_ax_tree_manager_;

 private:
  friend class AtomicViewAXTreeManagerTest;

  struct ChildWidgetsResult final {
    ChildWidgetsResult();
    ChildWidgetsResult(
        std::vector<raw_ptr<Widget, VectorExperimental>> child_widgets,
        bool is_tab_modal_showing);
    ChildWidgetsResult(const ChildWidgetsResult& other);
    virtual ~ChildWidgetsResult();
    ChildWidgetsResult& operator=(const ChildWidgetsResult& other);

    std::vector<raw_ptr<Widget, VectorExperimental>> child_widgets;

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
  void GetViewsInGroupForSet(
      std::vector<raw_ptr<View, VectorExperimental>>* views_in_group) const;

  // If this delegate is attached to the root view, returns all the child
  // widgets of this view's owning widget.
  ChildWidgetsResult GetChildWidgets() const;

  // Gets the real (non-virtual) TableView, otherwise nullptr.
  TableView* GetAncestorTableView() const;

  // We own this, but it is reference-counted on some platforms so we can't use
  // a unique_ptr. It is destroyed in the destructor.
  raw_ptr<ui::AXPlatformNode> ax_platform_node_ = nullptr;

  mutable ui::AXNodeData data_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_H_
