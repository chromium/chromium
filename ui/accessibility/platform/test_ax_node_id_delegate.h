// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_TEST_AX_NODE_ID_DELEGATE_H_
#define UI_ACCESSIBILITY_PLATFORM_TEST_AX_NODE_ID_DELEGATE_H_

#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/accessibility/platform/ax_node_id_delegate.h"
#include "ui/accessibility/platform/ax_unique_id.h"

namespace ui {

class TestAXNodeIdDelegate : public AXNodeIdDelegate {
 public:
  TestAXNodeIdDelegate();
  ~TestAXNodeIdDelegate() override;

  // AXNodeIdDelegate:
  AXPlatformNodeId GetOrCreateAXNodeUniqueId(AXNodeID ax_node_id) override;
  void OnAXNodeDeleted(AXNodeID ax_node_id) override;

 private:
  absl::flat_hash_map<ui::AXNodeID, ui::AXUniqueId> ax_unique_ids_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_TEST_AX_NODE_ID_DELEGATE_H_
