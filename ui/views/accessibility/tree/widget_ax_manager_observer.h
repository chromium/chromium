// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_OBSERVER_H_
#define UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_OBSERVER_H_

#include "base/observer_list_types.h"

namespace views {

class WidgetAXManager;

// Lightweight observer interface for WidgetAXManager lifecycle events. Kept in
// its own header to avoid dragging the full WidgetAXManager definition into
// widely used headers.
class WidgetAXManagerObserver : public base::CheckedObserver {
 public:
  ~WidgetAXManagerObserver() override = default;

  virtual void OnWidgetAXManagerEnabled() {}
  // TODO(crbug.com/467102963): Add OnWidgetAXManagerDisabled() once we support
  // turning off the accessibility engine.
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_OBSERVER_H_
