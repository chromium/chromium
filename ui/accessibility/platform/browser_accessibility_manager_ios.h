// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MANAGER_IOS_H_
#define UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MANAGER_IOS_H_

#include "base/component_export.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"

namespace ui {

class COMPONENT_EXPORT(AX_PLATFORM) BrowserAccessibilityManagerIOS
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerIOS(const AXTreeUpdate& initial_tree,
                                 AXNodeIdDelegate& node_id_delegate,
                                 AXPlatformTreeManagerDelegate* delegate);

  BrowserAccessibilityManagerIOS(const BrowserAccessibilityManagerIOS&) =
      delete;
  BrowserAccessibilityManagerIOS& operator=(
      const BrowserAccessibilityManagerIOS&) = delete;

  ~BrowserAccessibilityManagerIOS() override;

  static AXTreeUpdate GetEmptyDocument();

  // BrowserAccessibilityManager methods.
  gfx::Rect GetViewBoundsInScreenCoordinates() const override;

 private:
  // AXTreeObserver methods.
  void OnAtomicUpdateFinished(AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MANAGER_IOS_H_
