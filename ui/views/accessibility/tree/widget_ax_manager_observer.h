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
//
// Observers must unregister before they or the owning Widget are destroyed.
class WidgetAXManagerObserver : public base::CheckedObserver {
 public:
  ~WidgetAXManagerObserver() override = default;

  // Called after the initial serialization attempt, before Enable() returns.
  // is_enabled() is true. Disabling or rehosting the manager during this
  // callback CHECKs. Observers may remove themselves.
  virtual void OnWidgetAXManagerEnabled() {}
  // Called once per enabled-to-disabled transition, with is_enabled() false.
  // The old native tree has not yet been destroyed: observers must clear their
  // native accessible references before returning. Observers may remove
  // themselves or request enablement, which runs after teardown finishes,
  // before Disable() returns. Nested Disable() calls do not cancel pending
  // enablement.
  virtual void OnWidgetAXManagerDisabled() {}
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_OBSERVER_H_
