// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_FUCHSIA_ACCESSIBILITY_BRIDGE_FUCHSIA_H_
#define UI_ACCESSIBILITY_PLATFORM_FUCHSIA_ACCESSIBILITY_BRIDGE_FUCHSIA_H_

#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/platform/fuchsia/fuchsia_types.h"

namespace ui {

// Interface for clients to interact with fuchsia's platform accessibility
// framework.
class AX_EXPORT AccessibilityBridgeFuchsia {
 public:
  virtual ~AccessibilityBridgeFuchsia() = default;

  // Translates AXNodeDescriptorFuchsias to fuchsia IDs, fills the
  // corresponding fields in |node_update.node_data|, and sends the update
  // to fuchsia.
  //
  // Note that |node_update.node_data| should not have any node ID fields
  // (node_id, child_ids, offset_container_id, etc.) filled initially.
  virtual void UpdateNode(AXNodeUpdateFuchsia node_update) = 0;

  // Translates |node_id| to a fuchsia node ID, and sends the deletion to
  // fuchsia.
  virtual void DeleteSemanticNodes(AXNodeDescriptorFuchsia node_id) = 0;

  // Method to notify the accessibility bridge when a hit test result is
  // received. The accessibility bridge will convert |result| to a fuchsia node
  // ID, build a fuchsia Hit object using that ID, and invoke the callback
  // corresponding to hit_test_request_id.
  virtual void OnAccessibilityHitTestResult(int hit_test_request_id,
                                            AXNodeDescriptorFuchsia result) = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_FUCHSIA_ACCESSIBILITY_BRIDGE_FUCHSIA_H_
