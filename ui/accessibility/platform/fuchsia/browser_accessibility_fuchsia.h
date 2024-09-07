// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_FUCHSIA_BROWSER_ACCESSIBILITY_FUCHSIA_H_
#define UI_ACCESSIBILITY_PLATFORM_FUCHSIA_BROWSER_ACCESSIBILITY_FUCHSIA_H_

#include <fidl/fuchsia.accessibility.semantics/cpp/fidl.h>

#include <vector>

#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/fuchsia/browser_accessibility_manager_fuchsia.h"
#include "base/component_export.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia.h"
#include "ui/accessibility/platform/fuchsia/ax_platform_node_fuchsia.h"

namespace ui {

// Fuchsia-specific wrapper for a AXPlatformNode. Each
// BrowserAccessibilityFuchsia object is owned by a
// BrowserAccessibilityManagerFuchsia.
class COMPONENT_EXPORT(AX_PLATFORM) BrowserAccessibilityFuchsia : public BrowserAccessibility {
 public:
  BrowserAccessibilityFuchsia(BrowserAccessibilityManager* manager,
                              AXNode* node);
  ~BrowserAccessibilityFuchsia() override;

  // Disallow copy and assign.
  BrowserAccessibilityFuchsia(const BrowserAccessibilityFuchsia&) = delete;
  BrowserAccessibilityFuchsia& operator=(const BrowserAccessibilityFuchsia&) =
      delete;

  // Returns the fuchsia representation of the AXNode to which this
  // BrowserAccessibility object refers.
  fuchsia_accessibility_semantics::Node ToFuchsiaNodeData() const;

  // Returns the fuchsia ID of this node's offset container if the offset
  // container ID is valid. Otherwise, returns the ID of this tree's root node.
  uint32_t GetOffsetContainerOrRootNodeID() const;

  // BrowserAccessibility overrides.
  void OnDataChanged() override;
  void OnLocationChanged() override;
  AXPlatformNode* GetAXPlatformNode() const override;
  bool AccessibilityPerformAction(const AXActionData& action_data) override;

  // Returns this object's AXUniqueID as a uint32_t.
  uint32_t GetFuchsiaNodeID() const;

 protected:
  friend class BrowserAccessibility;  // Needs access to our constructor.

 private:
  AccessibilityBridgeFuchsia* GetAccessibilityBridge() const;

  void UpdateNode();
  void DeleteNode();
  std::vector<fuchsia_accessibility_semantics::Action> GetFuchsiaActions()
      const;
  fuchsia_accessibility_semantics::Role GetFuchsiaRole() const;
  fuchsia_accessibility_semantics::States GetFuchsiaStates() const;
  fuchsia_accessibility_semantics::Attributes GetFuchsiaAttributes() const;
  fuchsia_ui_gfx::BoundingBox GetFuchsiaLocation() const;
  fuchsia_ui_gfx::Mat4 GetFuchsiaTransform() const;
  std::vector<uint32_t> GetFuchsiaChildIDs() const;

  // Returns true if this AXNode has role AXRole::kList.
  // This may need to be expanded later to include more roles, maybe using
  // IsList
  // (https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/ax_role_properties.cc;l=399;drc=2c712b0d61f0788c0ed1b05176ae7430e8c705e5;bpv=1;bpt=1).
  bool IsList() const;

  // Returns true if this AXNode has role AXRole::klistItem.
  // This may need to be expanded later to include more roles, maybe using
  // IsListItem
  // (https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/ax_role_properties.cc;drc=2c712b0d61f0788c0ed1b05176ae7430e8c705e5;l=413).
  bool IsListElement() const;

  // Returns true if this AXNode should be mapped to Fuchsia's Default action.
  bool IsFuchsiaDefaultAction() const;

  // Fuchsia-specific representation of this node.
  AXPlatformNodeFuchsia* platform_node_;
};

BrowserAccessibilityFuchsia* COMPONENT_EXPORT(AX_PLATFORM)
ToBrowserAccessibilityFuchsia(BrowserAccessibility* obj);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_FUCHSIA_BROWSER_ACCESSIBILITY_FUCHSIA_H_
