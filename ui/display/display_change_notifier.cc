// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_change_notifier.h"

#include <stdint.h>

#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"

namespace display {

DisplayChangeNotifier::DisplayChangeNotifier() {}

DisplayChangeNotifier::~DisplayChangeNotifier() {}

void DisplayChangeNotifier::AddObserver(DisplayObserver* obs) {
  observer_list_.AddObserver(obs);
}

void DisplayChangeNotifier::RemoveObserver(DisplayObserver* obs) {
  observer_list_.RemoveObserver(obs);
}

void DisplayChangeNotifier::NotifyDisplaysChanged(
    const std::vector<Display>& old_displays,
    const std::vector<Display>& new_displays) {
  std::vector<Display> removed_displays;
  // Display present in old_displays but not in new_displays has been removed.
  for (auto old_it = old_displays.begin(); old_it != old_displays.end();
       ++old_it) {
    if (!base::Contains(new_displays, old_it->id(), &Display::id)) {
      removed_displays.push_back(*old_it);
    }
  }

  if (!removed_displays.empty()) {
    observer_list_.Notify(&DisplayObserver::OnDisplaysRemoved,
                          removed_displays);
  }

  // Display present in new_displays but not in old_displays has been added.
  // Display present in both might have been modified.
  for (auto new_it = new_displays.begin(); new_it != new_displays.end();
       ++new_it) {
    auto old_it = base::ranges::find(old_displays, new_it->id(), &Display::id);

    if (old_it == old_displays.end()) {
      observer_list_.Notify(&DisplayObserver::OnDisplayAdded, *new_it);
      continue;
    }

    uint32_t metrics = DisplayObserver::DISPLAY_METRIC_NONE;

    if (new_it->bounds() != old_it->bounds()) {
      metrics |= DisplayObserver::DISPLAY_METRIC_BOUNDS;
    }

    if (new_it->rotation() != old_it->rotation()) {
      metrics |= DisplayObserver::DISPLAY_METRIC_ROTATION;
    }

    if (new_it->work_area() != old_it->work_area()) {
      metrics |= DisplayObserver::DISPLAY_METRIC_WORK_AREA;
    }

    if (new_it->device_scale_factor() != old_it->device_scale_factor()) {
      metrics |= DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
    }

    if (new_it->GetColorSpaces() != old_it->GetColorSpaces()) {
      metrics |= DisplayObserver::DISPLAY_METRIC_COLOR_SPACE;
    }

    if (metrics != DisplayObserver::DISPLAY_METRIC_NONE) {
      observer_list_.Notify(&DisplayObserver::OnDisplayMetricsChanged, *new_it,
                            metrics);
    }
  }
}

void DisplayChangeNotifier::NotifyCurrentWorkspaceChanged(
    const std::string& workspace) {
  observer_list_.Notify(&DisplayObserver::OnCurrentWorkspaceChanged, workspace);
}

}  // namespace display
