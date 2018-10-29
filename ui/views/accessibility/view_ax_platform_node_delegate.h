// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_H_
#define UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
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
class Widget;

// Shared base class for platforms that require an implementation of
// |ViewAXPlatformNodeDelegate| to interface with the native accessibility
// toolkit. This class owns the |AXPlatformNode|, which implements those native
// APIs.
class ViewAXPlatformNodeDelegate : public ViewAccessibility,
                                   public ui::AXPlatformNodeDelegateBase {
 public:
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
  // Also in |ViewAccessibility|.
  const ui::AXUniqueId& GetUniqueId() const override;

 protected:
  explicit ViewAXPlatformNodeDelegate(View* view);

 private:
  // |is_tab_modal_showing| is set to true if, instead of populating
  // |result_child_widgets| normally, a single child widget was returned (e.g. a
  // dialog that should be read instead of the rest of the page contents).
  void PopulateChildWidgetVector(std::vector<Widget*>* result_child_widgets,
                                 bool* is_tab_modal_showing);

  void OnMenuItemActive();
  void OnMenuStart();
  void OnMenuEnd();

  // We own this, but it is reference-counted on some platforms so we can't use
  // a unique_ptr. It is destroyed in the destructor.
  ui::AXPlatformNode* ax_node_;

  mutable ui::AXNodeData data_;

  // Levels of menu are currently open, e.g. 0: none, 1: top, 2: submenu ...
  static int32_t menu_depth_;

  DISALLOW_COPY_AND_ASSIGN(ViewAXPlatformNodeDelegate);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_H_
