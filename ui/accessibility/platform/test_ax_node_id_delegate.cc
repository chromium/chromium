// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/test_ax_node_id_delegate.h"

namespace ui {

AXPlatformNodeId TestAXNodeIdDelegate::GetOrCreateAXNodeUniqueId(
    AXNodeID ax_node_id) {
  // Per-tab uniqueness is not necessary in tests, so return the blink node id.
  return AXPlatformNodeId(MakePassKey(), ax_node_id);
}

}  // namespace ui
