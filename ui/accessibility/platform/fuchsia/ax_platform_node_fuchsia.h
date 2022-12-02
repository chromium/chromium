// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_FUCHSIA_AX_PLATFORM_NODE_FUCHSIA_H_
#define UI_ACCESSIBILITY_PLATFORM_FUCHSIA_AX_PLATFORM_NODE_FUCHSIA_H_

#include "base/component_export.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"

namespace ui {

// Backing entity for chrome objects that want to be accessible on fuchsia.
// This class bridges between the chrome and fuchsia node representations.
class COMPONENT_EXPORT(AX_PLATFORM) AXPlatformNodeFuchsia
    : public AXPlatformNodeBase {
 public:
  AXPlatformNodeFuchsia();
  ~AXPlatformNodeFuchsia() override;

  AXPlatformNodeFuchsia(const AXPlatformNodeFuchsia&) = delete;
  AXPlatformNodeFuchsia& operator=(const AXPlatformNodeFuchsia&) = delete;

  // AXPlatformNode overrides.
  gfx::NativeViewAccessible GetNativeViewAccessible() override;

  // Requests that the delegate perform the specified action.
  virtual void PerformAction(const AXActionData& data);

  // TODO(fxb.dev/89485): Implement fuchsia node conversion.
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_FUCHSIA_AX_PLATFORM_NODE_FUCHSIA_H_
