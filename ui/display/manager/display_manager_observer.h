// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_MANAGER_OBSERVER_H_
#define UI_DISPLAY_MANAGER_DISPLAY_MANAGER_OBSERVER_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "base/observer_list_types.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager_export.h"

namespace display {

class DISPLAY_MANAGER_EXPORT DisplayManagerObserver
    : public base::CheckedObserver {
 public:
  using Displays = std::vector<Display>;

  // Wraps updated metrics for `display`. `changed_metrics` is a bitmask of
  // DisplayObserver::DisplayMetric types.
  struct DISPLAY_EXPORT DisplayMetricsChange {
    DisplayMetricsChange(const Display& display, uint32_t changed_metrics);
    const raw_ref<const Display> display;
    const uint32_t changed_metrics;
  };

  // Represents an atomic change in the display configuration of the user's
  // desktop environment maintained by DisplayManager.
  struct DISPLAY_EXPORT DisplayConfigurationChange {
    DisplayConfigurationChange(
        Displays added_displays,
        Displays removed_displays,
        std::vector<DisplayMetricsChange> display_metrics_changes);
    ~DisplayConfigurationChange();
    const Displays added_displays;
    const Displays removed_displays;
    const std::vector<DisplayMetricsChange> display_metrics_changes;
  };

  // Invoked only once after all displays are initialized after startup in
  // DisplayManager delegate.
  virtual void OnDisplaysInitialized() {}

  // Called before the DisplayManager begins processing a change / update to
  // the current display configuration.
  virtual void OnWillProcessDisplayChanges() {}

  // Called after the display configuration changes processed by the
  // DisplayManager have completed.
  virtual void OnDidProcessDisplayChanges(
      const DisplayConfigurationChange& configuration_change) {}

  // Called before the DisplayManager delegate starts applying the display
  // configuration changes.
  virtual void OnWillApplyDisplayChanges() {}

  // Called after the DisplayManager delegate has applied the display
  // configuration changes.
  virtual void OnDidApplyDisplayChanges() {}
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DISPLAY_MANAGER_OBSERVER_H_
