// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/widget_ax_manager.h"

#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform.h"

namespace views {

WidgetAXManager::WidgetAXManager(Widget* widget) : widget_(widget) {
  CHECK(::features::IsAccessibilityTreeForViewsEnabled())
      << "WidgetAXManager should only be created when the "
         "accessibility tree feature is enabled.";

  ui::AXPlatform::GetInstance().AddModeObserver(this);

  if (ui::AXPlatform::GetInstance().GetMode() == ui::AXMode::kNativeAPIs) {
    Enable();
  }
}

WidgetAXManager::~WidgetAXManager() {
  ui::AXPlatform::GetInstance().RemoveModeObserver(this);
}

void WidgetAXManager::Enable() {
  is_enabled_ = true;
}

void WidgetAXManager::OnEvent(ViewAccessibility& view_ax,
                              ax::mojom::Event event_type) {
  // TODO(accessibility): Implement data change handling.
}

void WidgetAXManager::OnDataChanged(ViewAccessibility& view_ax) {
  // TODO(accessibility): Implement data change handling.
}

void WidgetAXManager::OnAXModeAdded(ui::AXMode mode) {
  if (mode.has_mode(ui::AXMode::kNativeAPIs)) {
    Enable();
  }
}

}  // namespace views
