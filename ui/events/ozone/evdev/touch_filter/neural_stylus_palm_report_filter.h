// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_NEURAL_STYLUS_PALM_REPORT_FILTER_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_NEURAL_STYLUS_PALM_REPORT_FILTER_H_

#include <bitset>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"

namespace ui {

// A palm detection filter that doesn't change any reports, but adds UMA reports
// based on the shared palm state. Otherwise behaves identically to
// OpenPalmDetectionFilter.
class COMPONENT_EXPORT(EVDEV) NeuralStylusReportFilter
    : public PalmDetectionFilter {
 public:
  explicit NeuralStylusReportFilter(
      SharedPalmDetectionFilterState* shared_palm_state);

  NeuralStylusReportFilter(const NeuralStylusReportFilter&) = delete;
  NeuralStylusReportFilter& operator=(const NeuralStylusReportFilter&) = delete;

  ~NeuralStylusReportFilter() override;

  static bool CompatibleWithNeuralStylusReportFilter(
      const EventDeviceInfo& devinfo);

  void Filter(const std::vector<InProgressTouchEvdev>& touches,
              base::TimeTicks time,
              std::bitset<kNumTouchEvdevSlots>* slots_to_hold,
              std::bitset<kNumTouchEvdevSlots>* slots_to_suppress) override;

  static const char kFilterName[];
  static const char kNeuralFingerAge[];
  static const char kNeuralPalmAge[];
  static const char kNeuralPalmTouchCount[];
  std::string FilterNameForTesting() const override;

 private:
  bool previous_update_ = false;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_NEURAL_STYLUS_PALM_REPORT_FILTER_H_
