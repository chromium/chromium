// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/widget_ax_manager.h"

#include "ui/accessibility/accessibility_features.h"

namespace views {

WidgetAXManager::WidgetAXManager(Widget* widget) : widget_(widget) {
  CHECK(::features::IsAccessibilityTreeForViewsEnabled())
      << "WidgetAXManager should only be created when the "
         "accessibility tree feature is enabled.";
}

WidgetAXManager::~WidgetAXManager() = default;

void WidgetAXManager::Enable() {
  is_enabled_ = true;
}

void WidgetAXManager::Disable() {
  is_enabled_ = false;
}

}  // namespace views
