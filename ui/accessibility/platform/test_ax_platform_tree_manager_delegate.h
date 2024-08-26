// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_TEST_AX_PLATFORM_TREE_MANAGER_DELEGATE_H_
#define UI_ACCESSIBILITY_PLATFORM_TEST_AX_PLATFORM_TREE_MANAGER_DELEGATE_H_

#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
namespace ui {

class TestAXPlatformTreeManagerDelegate : public AXPlatformTreeManagerDelegate {
 public:
  TestAXPlatformTreeManagerDelegate();

  void AccessibilityPerformAction(const AXActionData& data) override;
  bool AccessibilityViewHasFocus() override;
  void AccessibilityViewSetFocus() override;
  gfx::Rect AccessibilityGetViewBounds() override;
  float AccessibilityGetDeviceScaleFactor() override;
  void UnrecoverableAccessibilityError() override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessibleForWindow()
      override;
  void AccessibilityHitTest(
      const gfx::Point& point_in_frame_pixels,
      const ax::mojom::Event& opt_event_to_fire,
      int opt_request_id,
      base::OnceCallback<void(AXPlatformTreeManager* hit_manager,
                              AXNodeID hit_node_id)> opt_callback) override;
  gfx::NativeWindow GetTopLevelNativeWindow() override;
  bool CanFireAccessibilityEvents() const override;
  bool AccessibilityIsRootFrame() const override;
  bool ShouldSuppressAXLoadComplete() override;
  content::WebContentsAccessibility*
    AccessibilityGetWebContentsAccessibility() override;

  bool is_root_frame_;
  gfx::AcceleratedWidget accelerated_widget_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_TEST_AX_PLATFORM_TREE_MANAGER_DELEGATE_H_
