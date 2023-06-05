// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/heuristic_stylus_palm_detection_filter.h"

#include <linux/input.h>

#include <ostream>

namespace ui {

void HeuristicStylusPalmDetectionFilter::Filter(
    const std::vector<InProgressTouchEvdev>& touches,
    base::TimeTicks time,
    std::bitset<kNumTouchEvdevSlots>* slots_to_hold,
    std::bitset<kNumTouchEvdevSlots>* slots_to_suppress) {
  slots_to_hold->reset();
  slots_to_suppress->reset();
  const size_t events_size =
      std::min(touches.size(), static_cast<size_t>(kNumTouchEvdevSlots));
  DCHECK_LE(touches.size(), static_cast<size_t>(kNumTouchEvdevSlots))
      << "heuristic filtering only expected to work on devices with "
         "kNumTouchEvdevSlots or fewer slots. Proceeding safely anyway, but "
         "unexpected.";
  for (size_t i = 0; i < events_size; ++i) {
    const auto& touch = touches[i];
    if (touch.tool_code == BTN_TOOL_PEN) {
      return;
    }
    if (!touch.touching) {
      stroke_length_[i] = 0;
      continue;
    }
    if (stroke_length_[i] == 0) {
      // new touch!
      touch_started_time_[i] = time;
    }
    stroke_length_[i]++;
    base::TimeDelta time_since_stylus_for_touch_start =
        touch_started_time_[i] - shared_palm_state_->latest_stylus_touch_time;
    if (time_since_stylus_for_touch_start < time_after_stylus_to_cancel_) {
      slots_to_suppress->set(i, true);
    } else if (time_since_stylus_for_touch_start < time_after_stylus_to_hold_ &&
               stroke_length_[i] <= hold_stroke_count_) {
      slots_to_hold->set(i, true);
    }
  }
}

HeuristicStylusPalmDetectionFilter::HeuristicStylusPalmDetectionFilter(
    SharedPalmDetectionFilterState* shared_palm_state,
    int hold_stroke_count,
    base::TimeDelta hold,
    base::TimeDelta cancel)
    : PalmDetectionFilter(shared_palm_state),
      hold_stroke_count_(hold_stroke_count),
      time_after_stylus_to_hold_(hold),
      time_after_stylus_to_cancel_(cancel) {
  touch_started_time_.resize(kNumTouchEvdevSlots, base::TimeTicks::UnixEpoch());
  stroke_length_.resize(kNumTouchEvdevSlots, 0);
  DCHECK(hold >= cancel) << "Expected hold time to be longer than cancel time.";
}

HeuristicStylusPalmDetectionFilter::~HeuristicStylusPalmDetectionFilter() {}

const char HeuristicStylusPalmDetectionFilter::kFilterName[] =
    "HeuristicStylusPalmDetectionFilter";

std::string HeuristicStylusPalmDetectionFilter::FilterNameForTesting() const {
  return kFilterName;
}

base::TimeDelta HeuristicStylusPalmDetectionFilter::HoldTime() const {
  return time_after_stylus_to_hold_;
}

base::TimeDelta HeuristicStylusPalmDetectionFilter::CancelTime() const {
  return time_after_stylus_to_cancel_;
}

bool HeuristicStylusPalmDetectionFilter::
    CompatibleWithHeuristicStylusPalmDetectionFilter(
        const EventDeviceInfo& device_info) {
  // Only internal devices are used for heuristics.
  return device_info.device_type() == INPUT_DEVICE_INTERNAL;
}

}  // namespace ui
