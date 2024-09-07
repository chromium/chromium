// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/accessibility/platform/ax_platform_node_mac.h"

#include "base/strings/sys_string_conversions.h"
#include "ui/accessibility/platform/ax_platform_node_cocoa.h"

namespace {

using RoleMap = std::map<ax::mojom::Role, NSString*>;
using EventMap = std::map<ax::mojom::Event, NSString*>;
using ActionList = std::vector<std::pair<ax::mojom::Action, NSString*>>;

void PostAnnouncementNotification(NSString* announcement,
                                  NSWindow* window,
                                  bool is_polite) {
  NSAccessibilityPriorityLevel priority =
      is_polite ? NSAccessibilityPriorityMedium : NSAccessibilityPriorityHigh;
  NSDictionary* notification_info = @{
    NSAccessibilityAnnouncementKey : announcement,
    NSAccessibilityPriorityKey : @(priority)
  };
  // On Mojave, announcements from an inactive window aren't spoken.
  NSAccessibilityPostNotificationWithUserInfo(
      window, NSAccessibilityAnnouncementRequestedNotification,
      notification_info);
}
void NotifyMacEvent(AXPlatformNodeCocoa* target, ax::mojom::Event event_type) {
  if (![target AXWindow]) {
    // A child tree is not attached to the window. Return early, otherwise
    // AppKit will hang trying to reach the root, resulting in a bug where
    // VoiceOver keeps repeating "[appname] is not responding".
    return;
  }
  NSString* notification =
      [AXPlatformNodeCocoa nativeNotificationFromAXEvent:event_type];
  if (notification)
    NSAccessibilityPostNotification(target, notification);
}

}  // namespace

namespace ui {

// static
AXPlatformNode* AXPlatformNode::Create(AXPlatformNodeDelegate* delegate) {
  AXPlatformNode* node = new AXPlatformNodeMac();
  node->Init(delegate);
  return node;
}

// static
AXPlatformNode* AXPlatformNode::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  if ([accessible isKindOfClass:[AXPlatformNodeCocoa class]])
    return [accessible node];
  return nullptr;
}

struct AXPlatformNodeMac::ObjCStorage {
  AXPlatformNodeCocoa* __strong native_node;
};

AXPlatformNodeMac::AXPlatformNodeMac()
    : objc_storage_(std::make_unique<ObjCStorage>()) {}
AXPlatformNodeMac::~AXPlatformNodeMac() = default;

void AXPlatformNodeMac::Destroy() {
  if (objc_storage_->native_node) {
    [objc_storage_->native_node detach];
    // Also, clear the pointer to make accidental use-after-free impossible.
    objc_storage_->native_node = nil;
  }
  AXPlatformNodeBase::Destroy();
}

// On Mac, the checked state is mapped to AXValue.
bool AXPlatformNodeMac::IsPlatformCheckable() const {
  if (GetRole() == ax::mojom::Role::kTab) {
    // On Mac, tabs are exposed as radio buttons, and are treated as checkable.
    // Also, the internal State::kSelected is be mapped to checked via AXValue.
    return true;
  }

  return AXPlatformNodeBase::IsPlatformCheckable();
}

AXPlatformNodeCocoa* AXPlatformNodeMac::GetNativeWrapper() const {
  return objc_storage_->native_node;
}

AXPlatformNodeCocoa* AXPlatformNodeMac::ReleaseNativeWrapper() {
  AXPlatformNodeCocoa* native_node = objc_storage_->native_node;
  objc_storage_->native_node = nil;
  return native_node;
}

void AXPlatformNodeMac::SetNativeWrapper(AXPlatformNodeCocoa* native_node) {
  objc_storage_->native_node = native_node;
}

gfx::NativeViewAccessible AXPlatformNodeMac::GetNativeViewAccessible() {
  if (!objc_storage_->native_node) {
    objc_storage_->native_node =
        [[AXPlatformNodeCocoa alloc] initWithNode:this];
  }
  return objc_storage_->native_node;
}

void AXPlatformNodeMac::NotifyAccessibilityEvent(ax::mojom::Event event_type) {
  AXPlatformNodeBase::NotifyAccessibilityEvent(event_type);
  GetNativeViewAccessible();
  // Handle special cases.

  // Alerts and live regions go through the announcement API instead of the
  // regular NSAccessibility notification system.
  if (event_type == ax::mojom::Event::kAlert ||
      event_type == ax::mojom::Event::kLiveRegionChanged) {
    if (AXAnnouncementSpec* announcement =
            [objc_storage_->native_node announcementForEvent:event_type]) {
      [objc_storage_->native_node scheduleLiveRegionAnnouncement:announcement];
    }
    return;
  }
  if (event_type == ax::mojom::Event::kSelection) {
    ax::mojom::Role role = GetRole();
    if (IsMenuItem(role)) {
      // On Mac, map menu item selection to a focus event.
      NotifyMacEvent(objc_storage_->native_node, ax::mojom::Event::kFocus);
      return;
    } else if (IsListItem(role)) {
      if (const AXPlatformNodeBase* container = GetSelectionContainer()) {
        if (container->GetRole() == ax::mojom::Role::kListBox &&
            !container->HasState(ax::mojom::State::kMultiselectable) &&
            GetDelegate()->GetFocus() == GetNativeViewAccessible()) {
          NotifyMacEvent(objc_storage_->native_node, ax::mojom::Event::kFocus);
          return;
        }
      }
    }
  }

  // Otherwise, use mappings between ax::mojom::Event and NSAccessibility
  // notifications from the EventMap above.
  NotifyMacEvent(objc_storage_->native_node, event_type);
}

void AXPlatformNodeMac::AnnounceTextAs(const std::u16string& text,
                                       AnnouncementType announcement_type) {
  PostAnnouncementNotification(base::SysUTF16ToNSString(text),
                               [objc_storage_->native_node AXWindow],
                               announcement_type == AnnouncementType::kPolite);
}

bool IsNameExposedInAXValueForRole(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kListMarker:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kStaticText:
    case ax::mojom::Role::kTitleBar:
      return true;
    default:
      return false;
  }
}

void AXPlatformNodeMac::AddAttributeToList(const char* name,
                                           const char* value,
                                           PlatformAttributeList* attributes) {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace ui
