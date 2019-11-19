// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_NEURAL_STYLUS_PALM_DETECTION_FILTER_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_NEURAL_STYLUS_PALM_DETECTION_FILTER_H_

#include <bitset>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"
#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter_model.h"
#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter_util.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

// An implementation of PalmDetectionFilter that relies on a DNN implementation
// to decide on palm detection. Requires a configured model as an argument.
// Heuristics are added for handling short strokes
class EVENTS_OZONE_EVDEV_EXPORT NeuralStylusPalmDetectionFilter
    : public PalmDetectionFilter {
 public:
  // Takes ownership of the model.
  NeuralStylusPalmDetectionFilter(
      const EventDeviceInfo& devinfo,
      std::unique_ptr<NeuralStylusPalmDetectionFilterModel> palm_model,
      SharedPalmDetectionFilterState* shared_palm_state);
  ~NeuralStylusPalmDetectionFilter() override;
  void Filter(const std::vector<InProgressTouchEvdev>& touches,
              base::TimeTicks time,
              std::bitset<kNumTouchEvdevSlots>* slots_to_hold,
              std::bitset<kNumTouchEvdevSlots>* slots_to_suppress) override;

  static bool CompatibleWithNeuralStylusPalmDetectionFilter(
      const EventDeviceInfo& devinfo);

  static const int kFeaturesPerSample;
  static const int kExtraFeaturesForNeighbor;

  static const char kFilterName[];
  std::string FilterNameForTesting() const override;

 private:
  void FindNearestNeighborsWithin(
      int neighbor_count,
      float max_distance,
      const PalmFilterStroke& stroke,
      std::vector<std::pair<float, int>>* nearest_strokes) const;
  void FindBiggestNeighborsWithin(
      int neighbor_count,
      float max_distance,
      const PalmFilterStroke& stroke,
      std::vector<std::pair<float, int>>* biggest_strokes) const;

  bool DetectSpuriousStroke(const std::vector<float>& features,
                            int tracking_id,
                            float threshold) const;
  // Extracts the feature vector for the specified stroke.
  std::vector<float> ExtractFeatures(int tracking_id) const;
  void AppendFeatures(const PalmFilterStroke& stroke,
                      std::vector<float>* features) const;
  void AppendFeaturesAsNeighbor(const PalmFilterStroke& stroke,
                                float distance,
                                std::vector<float>* features) const;

  bool ShouldDecideStroke(const PalmFilterStroke& stroke) const;
  bool IsHeuristicPalmStroke(const PalmFilterStroke& stroke) const;
  void EraseOldStrokes(base::TimeTicks time);

  std::bitset<kNumTouchEvdevSlots> is_palm_;
  std::bitset<kNumTouchEvdevSlots> is_delay_;
  std::map<int, PalmFilterStroke> strokes_;
  int tracking_ids_[kNumTouchEvdevSlots];
  const PalmFilterDeviceInfo palm_filter_dev_info_;
  std::unique_ptr<NeuralStylusPalmDetectionFilterModel> model_;

  static const std::vector<int> kRequiredAbsMtCodes;

  DISALLOW_COPY_AND_ASSIGN(NeuralStylusPalmDetectionFilter);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_NEURAL_STYLUS_PALM_DETECTION_FILTER_H_
