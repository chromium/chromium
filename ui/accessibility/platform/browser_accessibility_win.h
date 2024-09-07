// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_WIN_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/win/atl.h"
#include "base/component_export.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_com_win.h"
#include "ui/accessibility/ax_node.h"

namespace ui {

class COMPONENT_EXPORT(AX_PLATFORM) BrowserAccessibilityWin : public BrowserAccessibility {
 public:
  ~BrowserAccessibilityWin() override;
  BrowserAccessibilityWin(const BrowserAccessibilityWin&) = delete;
  BrowserAccessibilityWin& operator=(const BrowserAccessibilityWin&) = delete;

  // This is used to call UpdateStep1ComputeWinAttributes, ... above when
  // a node needs to be updated for some other reason other than via
  // OnAtomicUpdateFinished.
  void UpdatePlatformAttributes() override;

  //
  // AXPlatformNodeDelegate overrides.
  //

  std::wstring ComputeListItemNameFromContent() const override;

  //
  // BrowserAccessibility overrides.
  //

  bool CanFireEvents() const override;
  AXPlatformNode* GetAXPlatformNode() const override;
  void OnLocationChanged() override;
  std::u16string GetHypertext() const override;

  const std::vector<gfx::NativeViewAccessible> GetUIADirectChildrenInRange(
      AXPlatformNodeDelegate* start,
      AXPlatformNodeDelegate* end) override;

  gfx::NativeViewAccessible GetNativeViewAccessible() override;

  class BrowserAccessibilityComWin* GetCOM() const;

 protected:
  BrowserAccessibilityWin(BrowserAccessibilityManager* manager, AXNode* node);

  TextAttributeList ComputeTextAttributes() const override;

  bool ShouldHideChildrenForUIA() const;

  friend class BrowserAccessibility;  // Needs access to our constructor.

 private:
  raw_ptr<CComObject<BrowserAccessibilityComWin>> browser_accessibility_com_;
};

COMPONENT_EXPORT(AX_PLATFORM) BrowserAccessibilityWin* ToBrowserAccessibilityWin(
    BrowserAccessibility* obj);

COMPONENT_EXPORT(AX_PLATFORM) const BrowserAccessibilityWin* ToBrowserAccessibilityWin(
    const BrowserAccessibility* obj);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_WIN_H_
