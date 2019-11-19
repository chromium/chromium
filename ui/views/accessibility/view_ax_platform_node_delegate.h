// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_H_
#define UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_H_

#include <stdint.h>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_base.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/accessibility/view_accessibility.h"
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
  gfx::NativeViewAccessible GetNativeObject() override;
  void NotifyAccessibilityEvent(ax::mojom::Event event_type) override;
#if defined(OS_MACOSX)
  void AnnounceText(base::string16& text) override;
#endif

  // ui::AXPlatformNodeDelegate
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
  ui::AXPlatformNode* GetFromTreeIDAndNodeID(const ui::AXTreeID& ax_tree_id,
                                             int32_t id) override;
  bool AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool ShouldIgnoreHoveredStateForTesting() override;
  bool IsOffscreen() const override;
  base::string16 GetAuthorUniqueId() const override;
  bool IsMinimized() const override;
  // Also in |ViewAccessibility|.
  const ui::AXUniqueId& GetUniqueId() const override;

  // Ordered-set-like and item-like nodes.
  bool IsOrderedSetItem() const override;
  bool IsOrderedSet() const override;
  base::Optional<int> GetPosInSet() const override;
  base::Optional<int> GetSetSize() const override;

 protected:
  explicit ViewAXPlatformNodeDelegate(View* view);

  ui::AXPlatformNode* ax_platform_node() { return ax_platform_node_; }

 private:
  // Uses Views::GetViewsInGroup to find nearby Views in the same group.
  // Searches from the View's parent to include siblings within that group.
  void GetViewsInGroupForSet(std::vector<View*>* views_in_group) const;

  struct ChildWidgetsResult;

  ChildWidgetsResult GetChildWidgets() const;

  void OnMenuItemActive();
  void OnMenuStart();
  void OnMenuEnd();

  // We own this, but it is reference-counted on some platforms so we can't use
  // a unique_ptr. It is destroyed in the destructor.
  ui::AXPlatformNode* ax_platform_node_;

  mutable ui::AXNodeData data_;

  // Levels of menu are currently open, e.g. 0: none, 1: top, 2: submenu ...
  static int32_t menu_depth_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_H_
