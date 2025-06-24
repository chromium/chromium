// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/browser_accessibility_auralinux.h"

#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/browser_accessibility_manager_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

namespace ui {

BrowserAccessibilityAuraLinux* ToBrowserAccessibilityAuraLinux(
    BrowserAccessibility* obj) {
  return static_cast<BrowserAccessibilityAuraLinux*>(obj);
}

// static
std::unique_ptr<BrowserAccessibility> BrowserAccessibility::Create(
    BrowserAccessibilityManager* manager,
    AXNode* node) {
  return std::make_unique<BrowserAccessibilityAuraLinux>(manager, node);
}

BrowserAccessibilityAuraLinux::BrowserAccessibilityAuraLinux(
    BrowserAccessibilityManager* manager,
    AXNode* node)
    : BrowserAccessibility(manager, node),
      platform_node_(AXPlatformNode::Create(*this)) {}

BrowserAccessibilityAuraLinux::~BrowserAccessibilityAuraLinux() = default;

AXPlatformNodeAuraLinux* BrowserAccessibilityAuraLinux::GetNode() const {
  return static_cast<AXPlatformNodeAuraLinux*>(platform_node_.get());
}

BrowserAccessibilityAuraLinux*
BrowserAccessibilityAuraLinux::GetExtraAnnouncementNode(
    ax::mojom::AriaNotificationPriority priority_property) const {
  if (!manager() || !manager()->GetBrowserAccessibilityRoot()) {
    return nullptr;
  }

  const AXNode* root_node = manager()->GetBrowserAccessibilityRoot()->node();
  if (!root_node) {
    return nullptr;
  }

  const AXNode* announcement_node =
      root_node->GetExtraAnnouncementNode(priority_property);

  return ToBrowserAccessibilityAuraLinux(
      manager()->GetFromAXNode(announcement_node));
}

bool BrowserAccessibilityAuraLinux::HasExtraAnnouncementNodes() const {
  return this == manager_->GetBrowserAccessibilityRoot() && node()->tree() &&
         node()->tree()->extra_announcement_nodes();
}

gfx::NativeViewAccessible
BrowserAccessibilityAuraLinux::GetNativeViewAccessible() {
  DCHECK(platform_node_);
  return platform_node_->GetNativeViewAccessible();
}

void BrowserAccessibilityAuraLinux::UpdatePlatformAttributes() {
  GetNode()->UpdateHypertext();
}

void BrowserAccessibilityAuraLinux::OnDataChanged() {
  BrowserAccessibility::OnDataChanged();
  DCHECK(platform_node_);
  static_cast<AXPlatformNodeAuraLinux*>(platform_node_.get())
      ->EnsureAtkObjectIsValid();
}

size_t BrowserAccessibilityAuraLinux::PlatformChildCount() const {
  if (!HasExtraAnnouncementNodes()) {
    return BrowserAccessibility::PlatformChildCount();
  }

  return BrowserAccessibility::PlatformChildCount() +
         node()->tree()->extra_announcement_nodes()->Count();
}

BrowserAccessibility* BrowserAccessibilityAuraLinux::PlatformGetChild(
    size_t child_index) const {
  if (!HasExtraAnnouncementNodes() || child_index < InternalChildCount()) {
    return BrowserAccessibility::PlatformGetChild(child_index);
  }

  if (child_index >= PlatformChildCount()) {
    return nullptr;
  }

  child_index -= BrowserAccessibility::PlatformChildCount();

  if (child_index == ExtraAnnouncementNodes::kHighPriorityIndex) {
    return manager_->GetFromAXNode(node()->GetExtraAnnouncementNode(
        ax::mojom::AriaNotificationPriority::kHigh));
  } else if (child_index == ExtraAnnouncementNodes::kNormalPriorityIndex) {
    return manager_->GetFromAXNode(node()->GetExtraAnnouncementNode(
        ax::mojom::AriaNotificationPriority::kNormal));
  }

  return nullptr;
}

BrowserAccessibility* BrowserAccessibilityAuraLinux::PlatformGetLastChild()
    const {
  if (!HasExtraAnnouncementNodes()) {
    return BrowserAccessibility::PlatformGetLastChild();
  }

  CHECK(node()->GetExtraAnnouncementNode(
      ax::mojom::AriaNotificationPriority::kNormal));

  return manager_->GetFromAXNode(node()->GetExtraAnnouncementNode(
      ax::mojom::AriaNotificationPriority::kNormal));
}

BrowserAccessibility* BrowserAccessibilityAuraLinux::PlatformGetNextSibling()
    const {
  BrowserAccessibility* parent = PlatformGetParent();
  size_t next_child_index = node()->GetUnignoredIndexInParent() + 1;
  if (!node()->tree()->extra_announcement_nodes() || !parent ||
      next_child_index < parent->InternalChildCount()) {
    return BrowserAccessibility::PlatformGetNextSibling();
  }

  // The InternalChildCount() will not include extra announcement nodes, but
  // the PlatformChildCount() will. Therefore if the next sibling is at one of
  // the extra node indices, we'll need to get it via PlatformGetChild().
  if (next_child_index < parent->PlatformChildCount()) {
    return parent->PlatformGetChild(next_child_index);
  }

  return nullptr;
}

BrowserAccessibility*
BrowserAccessibilityAuraLinux::PlatformGetPreviousSibling() const {
  BrowserAccessibility* parent = PlatformGetParent();
  size_t child_index = node()->GetUnignoredIndexInParent();
  if (!node()->tree()->extra_announcement_nodes() || !parent ||
      child_index < parent->InternalChildCount()) {
    return BrowserAccessibility::PlatformGetPreviousSibling();
  }

  // The InternalChildCount() will not include extra announcement nodes, but
  // the PlatformChildCount() will. Therefore if the previous sibling is at
  // one of the extra node indices, we'll need to get it via
  // PlatformGetChild().
  if (child_index < parent->PlatformChildCount()) {
    return parent->PlatformGetChild(child_index - 1);
  }

  return nullptr;
}

AXPlatformNode* BrowserAccessibilityAuraLinux::GetAXPlatformNode() const {
  return GetNode();
}

std::u16string BrowserAccessibilityAuraLinux::GetHypertext() const {
  return GetNode()->AXPlatformNodeAuraLinux::GetHypertext();
}

TextAttributeList BrowserAccessibilityAuraLinux::ComputeTextAttributes() const {
  return GetNode()->ComputeTextAttributes();
}

void BrowserAccessibilityAuraLinux::SetIsPrimaryWebContentsForWindow() {
  manager()
      ->ToBrowserAccessibilityManagerAuraLinux()
      ->SetPrimaryWebContentsForWindow(GetId());
}

bool BrowserAccessibilityAuraLinux::IsPrimaryWebContentsForWindow() const {
  auto primary_id = manager()
                        ->ToBrowserAccessibilityManagerAuraLinux()
                        ->GetPrimaryWebContentsForWindow();
  return primary_id != kInvalidAXNodeID && primary_id == GetId();
}

}  // namespace ui
