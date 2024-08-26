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
    : BrowserAccessibility(manager, node) {
  platform_node_ =
      static_cast<AXPlatformNodeAuraLinux*>(AXPlatformNode::Create(this));
}

BrowserAccessibilityAuraLinux::~BrowserAccessibilityAuraLinux() {
  DCHECK(platform_node_);
  // Clear platform_node_ and return another raw_ptr instance
  // that is allowed to dangle.
  platform_node_.ExtractAsDangling()->Destroy();
}

AXPlatformNodeAuraLinux* BrowserAccessibilityAuraLinux::GetNode() const {
  return platform_node_;
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
  platform_node_->EnsureAtkObjectIsValid();
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
