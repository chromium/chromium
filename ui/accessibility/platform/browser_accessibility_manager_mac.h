// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MANAGER_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MANAGER_MAC_H_

#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "ui/accessibility/ax_updates_and_events.h"
#import "ui/accessibility/platform/browser_accessibility_cocoa.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"

namespace ui {
class AXPlatformTreeManagerDelegate;
}

namespace content {
class BrowserAccessibilityCocoaBrowserTest;
}

namespace ui {

class COMPONENT_EXPORT(AX_PLATFORM) BrowserAccessibilityManagerMac
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerMac(const AXTreeUpdate& initial_tree,
                                 AXNodeIdDelegate& node_id_delegate,
                                 AXPlatformTreeManagerDelegate* delegate);

  BrowserAccessibilityManagerMac(const BrowserAccessibilityManagerMac&) =
      delete;
  BrowserAccessibilityManagerMac& operator=(
      const BrowserAccessibilityManagerMac&) = delete;

  ~BrowserAccessibilityManagerMac() override;

  static AXTreeUpdate GetEmptyDocument();

  // AXTreeManager overrides.
  void FireFocusEvent(AXNode* node) override;

  // BrowserAccessibilityManager overrides.
  void FireBlinkEvent(ax::mojom::Event event_type,
                      BrowserAccessibility* node,
                      int action_request_id) override;
  void FireGeneratedEvent(AXEventGenerator::Event event_type,
                          const AXNode* node) override;

  void FireAriaNotificationEvent(
      BrowserAccessibility* node,
      const std::string& announcement,
      const std::string& notification_id,
      ax::mojom::AriaNotificationInterrupt interrupt_property,
      ax::mojom::AriaNotificationPriority priority_property) override;

  bool OnAccessibilityEvents(const AXUpdatesAndEvents& details) override;

  void FireSentinelEventForTesting() override;

  id GetParentView();
  id GetWindow();

 private:
  void FireNativeMacNotification(NSString* mac_notification,
                                 BrowserAccessibility* node);

  // AXTreeObserver methods.
  void OnAtomicUpdateFinished(AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override;

  NSDictionary* GetUserInfoForSelectedTextChangedNotification();

  NSDictionary* GetUserInfoForValueChangedNotification(
      const BrowserAccessibilityCocoa* native_node,
      const std::u16string& deleted_text,
      const std::u16string& inserted_text,
      id edit_text_marker) const;

  bool IsInGeneratedEventBatch(AXEventGenerator::Event event_type) const;

  bool ShouldFireLoadCompleteNotification();

  // Keeps track of any edits that have been made by the user during a tree
  // update. Used by NSAccessibilityValueChangedNotification.
  // Maps AXNode IDs to value attribute changes.
  std::map<int32_t, AXTextEdit> text_edits_;

  // This gives BrowserAccessibilityManager::Create access to the class
  // constructor.
  friend class BrowserAccessibilityManager;

  friend class content::BrowserAccessibilityCocoaBrowserTest;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MANAGER_MAC_H_
