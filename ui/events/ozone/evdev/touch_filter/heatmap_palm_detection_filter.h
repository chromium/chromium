// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_HEATMAP_PALM_DETECTION_FILTER_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_HEATMAP_PALM_DETECTION_FILTER_H_

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"

namespace ui {

struct HeatmapPalmFilterSample {
  int tracking_id = 0;
  base::TimeTicks time;

  bool operator==(const HeatmapPalmFilterSample& other) const = default;
};

struct HeatmapPalmDetectionFilterModelConfig {
  // Maximum sample count.
  uint32_t max_sample_count = 0;

  // Maximum time duration for a stroke to be considered as active.
  base::TimeDelta max_dead_neighbor_time = base::Milliseconds(100.0f);
};

class COMPONENT_EXPORT(EVDEV) HeatmapPalmFilterStroke {
 public:
  explicit HeatmapPalmFilterStroke(
      const HeatmapPalmDetectionFilterModelConfig& model_config,
      int tracking_id);
  HeatmapPalmFilterStroke(const HeatmapPalmFilterStroke& other);
  HeatmapPalmFilterStroke(HeatmapPalmFilterStroke&& other);
  ~HeatmapPalmFilterStroke();

  void ProcessSample(const HeatmapPalmFilterSample& sample);

  // Return the time duration of this stroke.
  base::TimeDelta Duration() const;

  const base::circular_deque<HeatmapPalmFilterSample>& samples() const {
    return samples_;
  }
  uint64_t samples_seen() const { return samples_seen_; }

 private:
  void AddSample(const HeatmapPalmFilterSample& sample);

  base::circular_deque<HeatmapPalmFilterSample> samples_;
  const int tracking_id_;

  // How many total samples have been reported for this stroke. This is
  // different from samples_.size() because samples_ will get pruned to only
  // keep a certain number of last samples.
  uint64_t samples_seen_ = 0;

  const uint64_t max_sample_count_;
  base::TimeTicks first_sample_time_;
};

// An implementation of PalmDetectionFilter. Stop listening to the heatmap based
// ML model when there is no new tracking ID within ~100ms.
class COMPONENT_EXPORT(EVDEV) HeatmapPalmDetectionFilter
    : public PalmDetectionFilter {
 public:
  HeatmapPalmDetectionFilter(
      const EventDeviceInfo& devinfo,
      std::unique_ptr<HeatmapPalmDetectionFilterModelConfig> model_config,
      SharedPalmDetectionFilterState* shared_palm_state);

  HeatmapPalmDetectionFilter(const HeatmapPalmDetectionFilter&) = delete;
  HeatmapPalmDetectionFilter& operator=(const HeatmapPalmDetectionFilter&) =
      delete;

  ~HeatmapPalmDetectionFilter() override;

  void Filter(const std::vector<InProgressTouchEvdev>& touches,
              base::TimeTicks time,
              std::bitset<kNumTouchEvdevSlots>* slots_to_hold,
              std::bitset<kNumTouchEvdevSlots>* slots_to_suppress) override;

  bool ShouldRunModel(const int tracking_id) const;

  static bool CompatibleWithHeatmapPalmDetectionFilter(
      const EventDeviceInfo& devinfo);

  static constexpr char kFilterName[] = "HeatmapPalmDetectionFilter";
  std::string FilterNameForTesting() const override;

 private:
  void EraseOldStrokes(base::TimeTicks time);

  // The key of strokes map is tracking id.
  std::map<int, HeatmapPalmFilterStroke> strokes_;
  base::flat_set<int> tracking_ids_decided_;
  std::array<int, kNumTouchEvdevSlots> tracking_ids_{};
  std::unique_ptr<HeatmapPalmDetectionFilterModelConfig> model_config_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_HEATMAP_PALM_DETECTION_FILTER_H_
