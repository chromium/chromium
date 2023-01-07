// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_HEURISTIC_STYLUS_PALM_DETECTION_FILTER_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_HEURISTIC_STYLUS_PALM_DETECTION_FILTER_H_

#include <bitset>
#include <vector>

#include "base/time/time.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"

namespace ui {

// A heuristic implementation of PalmDetectionFilter.
// Relies on firmware palm detection, but modifies behavior _after_ a stylus
// touch since our mutual-exclusion of stylus/touch means that we do not trust
// the device right after stylus.
// Configured with 3 inputs:
// 1. How many strokes to hold on to when holding.
// 2. TimeDelta for cancellation: any strokes started within this delta are
// cancelled automatically.
// 3. TimeDelta for hold: any strokes started after the cancellation and before
// this are held for the stroke count (as above). If they're cancelled
// externally, we never report them. If they terminate before the count, we
// output all items.
//
// NOTE: This filter is only intended for certain boards of hardware that have
// poor interaction between a mutually exclusive stylus and finger input:
// Turning it on for devices where is not intended will probably degrade
// performance and create a poor UX.
class COMPONENT_EXPORT(EVDEV) HeuristicStylusPalmDetectionFilter
    : public PalmDetectionFilter {
 public:
  HeuristicStylusPalmDetectionFilter(
      SharedPalmDetectionFilterState* shared_palm_state,
      int hold_stroke_count,
      base::TimeDelta hold,
      base::TimeDelta cancel);

  HeuristicStylusPalmDetectionFilter(
      const HeuristicStylusPalmDetectionFilter&) = delete;
  HeuristicStylusPalmDetectionFilter& operator=(
      const HeuristicStylusPalmDetectionFilter&) = delete;

  ~HeuristicStylusPalmDetectionFilter() override;

  void Filter(const std::vector<InProgressTouchEvdev>& touches,
              base::TimeTicks time,
              std::bitset<kNumTouchEvdevSlots>* slots_to_hold,
              std::bitset<kNumTouchEvdevSlots>* slots_to_suppress) override;

  static const char kFilterName[];
  std::string FilterNameForTesting() const override;

  base::TimeDelta HoldTime() const;
  base::TimeDelta CancelTime() const;

  static bool CompatibleWithHeuristicStylusPalmDetectionFilter(
      const EventDeviceInfo& device_info);

 private:
  const int hold_stroke_count_;
  const base::TimeDelta time_after_stylus_to_hold_;
  const base::TimeDelta time_after_stylus_to_cancel_;

  std::vector<base::TimeTicks> touch_started_time_;

  // How many items have we seen in this stroke so far?
  std::vector<int> stroke_length_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_HEURISTIC_STYLUS_PALM_DETECTION_FILTER_H_
