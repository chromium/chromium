// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_change_notifier.h"

#include <stdint.h>

#include "ui/display/display.h"
#include "ui/display/display_observer.h"

namespace display {

namespace {

class DisplayComparator {
 public:
  explicit DisplayComparator(const Display& display)
      : display_id_(display.id()) {}

  bool operator()(const Display& display) const {
    return display.id() == display_id_;
  }

 private:
  int64_t display_id_;
};

}  // anonymous namespace

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
  // Display present in old_displays but not in new_displays has been removed.
  auto old_it = old_displays.begin();
  for (; old_it != old_displays.end(); ++old_it) {
    if (std::find_if(new_displays.begin(), new_displays.end(),
                     DisplayComparator(*old_it)) == new_displays.end()) {
      for (DisplayObserver& observer : observer_list_)
        observer.OnDisplayRemoved(*old_it);
    }
  }

  // Display present in new_displays but not in old_displays has been added.
  // Display present in both might have been modified.
  for (auto new_it = new_displays.begin(); new_it != new_displays.end();
       ++new_it) {
    auto old_it = std::find_if(old_displays.begin(), old_displays.end(),
                               DisplayComparator(*new_it));

    if (old_it == old_displays.end()) {
      for (DisplayObserver& observer : observer_list_)
        observer.OnDisplayAdded(*new_it);
      continue;
    }

    uint32_t metrics = DisplayObserver::DISPLAY_METRIC_NONE;

    if (new_it->bounds() != old_it->bounds())
      metrics |= DisplayObserver::DISPLAY_METRIC_BOUNDS;

    if (new_it->rotation() != old_it->rotation())
      metrics |= DisplayObserver::DISPLAY_METRIC_ROTATION;

    if (new_it->work_area() != old_it->work_area())
      metrics |= DisplayObserver::DISPLAY_METRIC_WORK_AREA;

    if (new_it->device_scale_factor() != old_it->device_scale_factor())
      metrics |= DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;

    if (new_it->color_space() != old_it->color_space() ||
        new_it->sdr_white_level() != old_it->sdr_white_level()) {
      metrics |= DisplayObserver::DISPLAY_METRIC_COLOR_SPACE;
    }

    if (metrics != DisplayObserver::DISPLAY_METRIC_NONE) {
      for (DisplayObserver& observer : observer_list_)
        observer.OnDisplayMetricsChanged(*new_it, metrics);
    }
  }
}

}  // namespace display
