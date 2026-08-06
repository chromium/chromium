// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/browser_accessibility_auralinux.h"

#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/browser_accessibility_manager_auralinux.h"

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
  UpdatePlatformNode();
}

BrowserAccessibilityAuraLinux::~BrowserAccessibilityAuraLinux() = default;

AXPlatformNodeAuraLinux* BrowserAccessibilityAuraLinux::GetNode() const {
  return static_cast<AXPlatformNodeAuraLinux*>(platform_node_.get());
}

gfx::NativeViewAccessible
BrowserAccessibilityAuraLinux::GetNativeViewAccessible() {
  return platform_node_ ? platform_node_->GetNativeViewAccessible()
                        : gfx::NativeViewAccessible();
}

void BrowserAccessibilityAuraLinux::UpdatePlatformAttributes() {
  if (GetNode()) {
    GetNode()->UpdateHypertext();
  }
}

void BrowserAccessibilityAuraLinux::UpdatePlatformNode() {
  if (!ShouldHavePlatformNode()) {
    platform_node_.reset();
    return;
  }
  if (!platform_node_) {
    platform_node_ = AXPlatformNode::Create(*this);
    // ATK gives an object only after this call, and a new platform node has
    // none yet.
    GetNode()->EnsureAtkObjectIsValid();
  }
}

void BrowserAccessibilityAuraLinux::OnDataChanged() {
  BrowserAccessibility::OnDataChanged();
  if (GetNode()) {
    GetNode()->EnsureAtkObjectIsValid();
  }
}

AXPlatformNode* BrowserAccessibilityAuraLinux::GetAXPlatformNode() const {
  return GetNode();
}

std::u16string BrowserAccessibilityAuraLinux::GetHypertext() const {
  if (!GetNode()) {
    return std::u16string();
  }
  return GetNode()->AXPlatformNodeAuraLinux::GetHypertext();
}

TextAttributeList BrowserAccessibilityAuraLinux::ComputeTextAttributes() const {
  if (!GetNode()) {
    return TextAttributeList();
  }
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
