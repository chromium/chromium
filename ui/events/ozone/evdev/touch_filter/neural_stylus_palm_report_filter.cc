// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_report_filter.h"

#include "base/metrics/histogram_macros.h"

namespace ui {

NeuralStylusReportFilter::NeuralStylusReportFilter(
    SharedPalmDetectionFilterState* shared_palm_state)
    : PalmDetectionFilter(shared_palm_state) {}

NeuralStylusReportFilter::~NeuralStylusReportFilter() {}

bool NeuralStylusReportFilter::CompatibleWithNeuralStylusReportFilter(
    const EventDeviceInfo& devinfo) {
  return devinfo.HasStylus();
}

void NeuralStylusReportFilter::Filter(
    const std::vector<InProgressTouchEvdev>& touches,
    base::TimeTicks time,
    std::bitset<kNumTouchEvdevSlots>* slots_to_hold,
    std::bitset<kNumTouchEvdevSlots>* slots_to_suppress) {
  bool should_update = false;
  for (const auto& touch : touches) {
    // Is a stylus detected? We should be updating.
    if (touch.altered && touch.stylus_button) {
      should_update = true;
      continue;
    }
  }
  // Only update once when a stylus is detected.
  if (!previous_update_ && should_update) {
    // Once a stylus is detected, we report.
    base::TimeDelta palm_age =
        time - shared_palm_state_->latest_palm_touch_time;
    base::TimeDelta finger_age =
        time - shared_palm_state_->latest_finger_touch_time;
    UMA_HISTOGRAM_TIMES(kNeuralPalmAge, palm_age);
    UMA_HISTOGRAM_TIMES(kNeuralFingerAge, finger_age);
    UMA_HISTOGRAM_COUNTS_100(kNeuralPalmTouchCount,
                             shared_palm_state_->active_palm_touches);
  }
  previous_update_ = should_update;
  slots_to_hold->reset();
  slots_to_suppress->reset();
}

const char NeuralStylusReportFilter::kFilterName[] = "NeuralStylusReportFilter";
const char NeuralStylusReportFilter::kNeuralPalmAge[] =
    "Ozone.NeuralStylusReport.PalmAgeBeforeStylus";
const char NeuralStylusReportFilter::kNeuralFingerAge[] =
    "Ozone.NeuralStylusReport.FingerAgeBeforeStylus";
const char NeuralStylusReportFilter::kNeuralPalmTouchCount[] =
    "Ozone.NeuralStylusReport.ActivePalmTouchCount";
std::string NeuralStylusReportFilter::FilterNameForTesting() const {
  return kFilterName;
}

}  // namespace ui
