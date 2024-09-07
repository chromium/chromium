// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/accessibility/platform/browser_accessibility_manager_ios.h"

#import <UIKit/UIKit.h>

namespace ui {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const AXTreeUpdate& initial_tree,
    AXNodeIdDelegate& node_id_delegate,
    AXPlatformTreeManagerDelegate* delegate) {
  return new BrowserAccessibilityManagerIOS(initial_tree, node_id_delegate,
                                            delegate);
}

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    AXNodeIdDelegate& node_id_delegate,
    AXPlatformTreeManagerDelegate* delegate) {
  return new BrowserAccessibilityManagerIOS(
      BrowserAccessibilityManagerIOS::GetEmptyDocument(), node_id_delegate,
      delegate);
}

BrowserAccessibilityManagerIOS*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerIOS() {
  return static_cast<BrowserAccessibilityManagerIOS*>(this);
}

BrowserAccessibilityManagerIOS::BrowserAccessibilityManagerIOS(
    const AXTreeUpdate& initial_tree,
    AXNodeIdDelegate& node_id_delegate,
    AXPlatformTreeManagerDelegate* delegate)
    : BrowserAccessibilityManager(node_id_delegate, delegate) {
  Initialize(initial_tree);
}

BrowserAccessibilityManagerIOS::~BrowserAccessibilityManagerIOS() = default;

// static
AXTreeUpdate BrowserAccessibilityManagerIOS::GetEmptyDocument() {
  AXNodeData empty_document;
  empty_document.id = kInitialEmptyDocumentRootNodeID;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

void BrowserAccessibilityManagerIOS::OnAtomicUpdateFinished(
    AXTree* tree,
    bool root_changed,
    const std::vector<Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(tree, root_changed,
                                                      changes);
  BrowserAccessibility* root = GetBrowserAccessibilityRoot();
  if (!root) {
    return;
  }

  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  root->GetNativeViewAccessible());
}

gfx::Rect BrowserAccessibilityManagerIOS::GetViewBoundsInScreenCoordinates()
    const {
  AXPlatformTreeManagerDelegate* delegate = GetDelegateFromRootManager();
  if (!delegate) {
    return gfx::Rect();
  }

  UIView* view = delegate->AccessibilityGetNativeViewAccessible();
  if (!view) {
    return gfx::Rect();
  }
  gfx::Rect bounds = delegate->AccessibilityGetViewBounds();
  bounds = gfx::Rect(
      UIAccessibilityConvertFrameToScreenCoordinates(bounds.ToCGRect(), view));
  return ScaleToEnclosingRect(bounds, device_scale_factor());
}

}  // namespace ui
