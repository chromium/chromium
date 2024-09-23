// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/test_ax_platform_tree_manager_delegate.h"

namespace ui {

TestAXPlatformTreeManagerDelegate::TestAXPlatformTreeManagerDelegate()
    : is_root_frame_(true), accelerated_widget_(gfx::kNullAcceleratedWidget) {}

void TestAXPlatformTreeManagerDelegate::AccessibilityPerformAction(
    const AXActionData& data) {}

bool TestAXPlatformTreeManagerDelegate::AccessibilityViewHasFocus() {
  return false;
}

void TestAXPlatformTreeManagerDelegate::AccessibilityViewSetFocus() {}

gfx::Rect TestAXPlatformTreeManagerDelegate::AccessibilityGetViewBounds() {
  return gfx::Rect();
}

float TestAXPlatformTreeManagerDelegate::AccessibilityGetDeviceScaleFactor() {
  return 1.0f;
}

void TestAXPlatformTreeManagerDelegate::UnrecoverableAccessibilityError() {}

gfx::AcceleratedWidget
TestAXPlatformTreeManagerDelegate::AccessibilityGetAcceleratedWidget() {
  return accelerated_widget_;
}

gfx::NativeViewAccessible
TestAXPlatformTreeManagerDelegate::AccessibilityGetNativeViewAccessible() {
  return nullptr;
}

gfx::NativeViewAccessible TestAXPlatformTreeManagerDelegate::
    AccessibilityGetNativeViewAccessibleForWindow() {
  return nullptr;
}

void TestAXPlatformTreeManagerDelegate::AccessibilityHitTest(
    const gfx::Point& point_in_frame_pixels,
    const ax::mojom::Event& opt_event_to_fire,
    int opt_request_id,
    base::OnceCallback<void(AXPlatformTreeManager* hit_manager,
                            AXNodeID hit_node_id)> opt_callback) {}

gfx::NativeWindow TestAXPlatformTreeManagerDelegate::GetTopLevelNativeWindow() {
  return gfx::NativeWindow();
}

bool TestAXPlatformTreeManagerDelegate::CanFireAccessibilityEvents() const {
  return true;
}

bool TestAXPlatformTreeManagerDelegate::AccessibilityIsRootFrame() const {
  return is_root_frame_;
}

bool TestAXPlatformTreeManagerDelegate::ShouldSuppressAXLoadComplete() {
  return false;
}

content::WebContentsAccessibility*
TestAXPlatformTreeManagerDelegate::AccessibilityGetWebContentsAccessibility() {
  return nullptr;
}

}  // namespace ui
