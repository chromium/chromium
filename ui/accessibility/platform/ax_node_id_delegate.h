// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_NODE_ID_DELEGATE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_NODE_ID_DELEGATE_H_

#include "base/component_export.h"
#include "base/types/pass_key.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/platform/ax_platform_node_id.h"

namespace ui {

// A delegate for a tree manager that is responsible for assigning identifiers
// to nodes suitable for transmission to an accessibility tool. Generally, such
// identifiers must be unique within their containing native window.
class COMPONENT_EXPORT(AX_PLATFORM) AXNodeIdDelegate {
 public:
  AXNodeIdDelegate(const AXNodeIdDelegate&) = delete;
  AXNodeIdDelegate& operator=(const AXNodeIdDelegate&) = delete;
  virtual ~AXNodeIdDelegate() = default;

  // Returns the platform node identifier for a given blink node identifier.
  virtual AXPlatformNodeId GetOrCreateAXNodeUniqueId(AXNodeID ax_node_id) = 0;

  // Notifies the delegate that a blink node has been removed from the tree.
  virtual void OnAXNodeDeleted(AXNodeID ax_node_id) = 0;

 protected:
  using PassKey = base::PassKey<AXNodeIdDelegate>;

  AXNodeIdDelegate() = default;

  // Returns a PassKey to be used by implementations so that they may create
  // AXPlatformNodeId instances with values of their own choosing.
  static constexpr PassKey MakePassKey() { return PassKey(); }
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_NODE_ID_DELEGATE_H_
