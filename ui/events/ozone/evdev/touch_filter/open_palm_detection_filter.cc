// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/open_palm_detection_filter.h"

namespace ui {

OpenPalmDetectionFilter::OpenPalmDetectionFilter(
    SharedPalmDetectionFilterState* shared_palm_state)
    : PalmDetectionFilter(shared_palm_state) {}

OpenPalmDetectionFilter::~OpenPalmDetectionFilter() {}

void OpenPalmDetectionFilter::Filter(
    const std::vector<InProgressTouchEvdev>& touches,
    base::TimeTicks time,
    std::bitset<kNumTouchEvdevSlots>* slots_to_hold,
    std::bitset<kNumTouchEvdevSlots>* slots_to_suppress) {
  slots_to_hold->reset();
  slots_to_suppress->reset();
  // Open Palm doesn't know if it's running on a stylus device, or a
  // touchscreen. It doesn't track active fingers/palms on the device.
}

const char OpenPalmDetectionFilter::kFilterName[] = "OpenPalmDetectionFilter";

std::string OpenPalmDetectionFilter::FilterNameForTesting() const {
  return kFilterName;
}

}  // namespace ui
