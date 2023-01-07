// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_OPEN_PALM_DETECTION_FILTER_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_OPEN_PALM_DETECTION_FILTER_H_

#include <bitset>
#include <vector>

#include "base/time/time.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"

namespace ui {

// A simple implementation of PalmDetectionFilter.
// Does not delay or set anything to palm.
class COMPONENT_EXPORT(EVDEV) OpenPalmDetectionFilter
    : public PalmDetectionFilter {
 public:
  explicit OpenPalmDetectionFilter(
      SharedPalmDetectionFilterState* shared_palm_state);

  OpenPalmDetectionFilter(const OpenPalmDetectionFilter&) = delete;
  OpenPalmDetectionFilter& operator=(const OpenPalmDetectionFilter&) = delete;

  ~OpenPalmDetectionFilter() override;

  void Filter(const std::vector<InProgressTouchEvdev>& touches,
              base::TimeTicks time,
              std::bitset<kNumTouchEvdevSlots>* slots_to_hold,
              std::bitset<kNumTouchEvdevSlots>* slots_to_suppress) override;

  static const char kFilterName[];
  std::string FilterNameForTesting() const override;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_OPEN_PALM_DETECTION_FILTER_H_
