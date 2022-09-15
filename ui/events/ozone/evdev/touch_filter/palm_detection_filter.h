// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_PALM_DETECTION_FILTER_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_PALM_DETECTION_FILTER_H_

#include <bitset>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"

namespace ui {

// An abstract palm detection filter. It has two functions:
// 1. To decide which touches to "hold"
// 2. To decide which touches to suppress / "cancel".
// Interface is similar to that of TouchFilter but enshrines the "hold" as a
// first class citizen.
class COMPONENT_EXPORT(EVDEV) PalmDetectionFilter {
 public:
  // shared_palm_state is not owned!
  explicit PalmDetectionFilter(
      SharedPalmDetectionFilterState* shared_palm_state);
  virtual ~PalmDetectionFilter();

  // Execute a filter event. Expected to be executed on every update to touches.
  // Arguments are:
  // 1. touches: a vector of kNumTouchEvdevSlots touches.
  // 2. time: the latest event time in touches.
  // 3. slots_to_hold: output bitset of slots to hold. Must not be null.
  // 4. slots_to_suppress: output bitset of slots to suppress/cancel. Must not
  // be null.
  //
  // Note that if a slot is marked as both suppress and hold, we expect the
  // suppress to override the hold.
  virtual void Filter(const std::vector<InProgressTouchEvdev>& touches,
                      base::TimeTicks time,
                      std::bitset<kNumTouchEvdevSlots>* slots_to_hold,
                      std::bitset<kNumTouchEvdevSlots>* slots_to_suppress) = 0;

  // The name of this filter, for testing purposes.
  virtual std::string FilterNameForTesting() const = 0;

 protected:
  // Not owned!
  const raw_ptr<SharedPalmDetectionFilterState> shared_palm_state_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_PALM_DETECTION_FILTER_H_
