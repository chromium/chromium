// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_TEST_AX_NODE_ID_DELEGATE_H_
#define UI_ACCESSIBILITY_PLATFORM_TEST_AX_NODE_ID_DELEGATE_H_

#include "ui/accessibility/platform/ax_node_id_delegate.h"

namespace ui {

class TestAXNodeIdDelegate : public AXNodeIdDelegate {
 public:
  TestAXNodeIdDelegate() = default;

  // AXNodeIdDelegate:
  AXPlatformNodeId GetOrCreateAXNodeUniqueId(AXNodeID ax_node_id) override;
  void OnAXNodeDeleted(AXNodeID ax_node_id) override {}
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_TEST_AX_NODE_ID_DELEGATE_H_
