// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEST_HELPER_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEST_HELPER_H_

#include <string>

#include "base/component_export.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

class COMPONENT_EXPORT(AX_PLATFORM) AXPlatformNodeTestHelper {
 public:
  static int GetTreeSize(AXPlatformNode* ax_node);
  static AXPlatformNode* FindChildByName(AXPlatformNode* ax_node,
                                         const std::string& name);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEST_HELPER_H_
