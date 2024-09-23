// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MANAGER_AURALINUX_H_
#define UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MANAGER_AURALINUX_H_

#include <vector>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"

namespace ui {
class AXPlatformTreeManagerDelegate;
}

namespace ui {

class BrowserAccessibilityAuraLinux;

// Manages a tree of BrowserAccessibilityAuraLinux objects.
class COMPONENT_EXPORT(AX_PLATFORM) BrowserAccessibilityManagerAuraLinux
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerAuraLinux(const AXTreeUpdate& initial_tree,
                                       AXNodeIdDelegate& node_id_delegate,
                                       AXPlatformTreeManagerDelegate* delegate);

  BrowserAccessibilityManagerAuraLinux(
      const BrowserAccessibilityManagerAuraLinux&) = delete;
  BrowserAccessibilityManagerAuraLinux& operator=(
      const BrowserAccessibilityManagerAuraLinux&) = delete;

  ~BrowserAccessibilityManagerAuraLinux() override;

  static AXTreeUpdate GetEmptyDocument();

  void SetPrimaryWebContentsForWindow(AXNodeID node_id);
  AXNodeID GetPrimaryWebContentsForWindow() const;

  // AXTreeManager overrides.
  void FireFocusEvent(AXNode* node) override;

  // BrowserAccessibilityManager overrides.
  void FireBlinkEvent(ax::mojom::Event event_type,
                      BrowserAccessibility* node,
                      int action_request_id) override;
  void FireGeneratedEvent(AXEventGenerator::Event event_type,
                          const AXNode* node) override;

  void FireSelectedEvent(BrowserAccessibility* node);
  void FireEnabledChangedEvent(BrowserAccessibility* node);
  void FireExpandedEvent(BrowserAccessibility* node, bool is_expanded);
  void FireShowingEvent(BrowserAccessibility* node, bool is_showing);
  void FireInvalidStatusChangedEvent(BrowserAccessibility* node);
  void FireAriaCurrentChangedEvent(BrowserAccessibility* node);
  void FireBusyChangedEvent(BrowserAccessibility* node, bool is_busy);
  void FireLoadingEvent(BrowserAccessibility* node, bool is_loading);
  void FireNameChangedEvent(BrowserAccessibility* node);
  void FireDescriptionChangedEvent(BrowserAccessibility* node);
  void FireParentChangedEvent(BrowserAccessibility* node);
  void FireReadonlyChangedEvent(BrowserAccessibility* node);
  void FireSortDirectionChangedEvent(BrowserAccessibility* node);
  void FireTextAttributesChangedEvent(BrowserAccessibility* node);
  void FireSubtreeCreatedEvent(BrowserAccessibility* node);
  void OnFindInPageResult(int request_id,
                          int match_index,
                          int start_id,
                          int start_offset,
                          int end_id,
                          int end_offset) override;
  void OnFindInPageTermination() override;

 protected:
  FRIEND_TEST_ALL_PREFIXES(BrowserAccessibilityManagerAuraLinuxTest,
                           TestEmitChildrenChanged);
  // AXTreeObserver methods.
  void OnNodeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnIgnoredWillChange(
      AXTree* tree,
      AXNode* node,
      bool is_ignored_new_value,
      bool is_changing_unignored_parents_children) override;
  void OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) override;
  void OnAtomicUpdateFinished(
      AXTree* tree,
      bool root_changed,
      const std::vector<AXTreeObserver::Change>& changes) override;

 private:
  bool CanEmitChildrenChanged(BrowserAccessibility* node) const;
  void FireEvent(BrowserAccessibility* node, ax::mojom::Event event);

  // Give BrowserAccessibilityManager::Create access to our constructor.
  friend class BrowserAccessibilityManager;

  AXNodeID primary_web_contents_for_window_id_ = kInvalidAXNodeID;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MANAGER_AURALINUX_H_
