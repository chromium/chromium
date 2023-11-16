// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_manager_observer.h"

namespace display {

DisplayManagerObserver::DisplayMetricsChange::DisplayMetricsChange(
    const Display& display,
    uint32_t changed_metrics)
    : display(display), changed_metrics(changed_metrics) {}

DisplayManagerObserver::DisplayConfigurationChange::DisplayConfigurationChange(
    Displays added_displays,
    Displays removed_displays,
    std::vector<DisplayMetricsChange> display_metrics_changes)
    : added_displays(added_displays),
      removed_displays(removed_displays),
      display_metrics_changes(display_metrics_changes) {}

DisplayManagerObserver::DisplayConfigurationChange::
    ~DisplayConfigurationChange() = default;

}  // namespace display
