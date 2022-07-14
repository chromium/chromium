// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_NEURAL_STYLUS_PALM_DETECTION_FILTER_UTIL_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_NEURAL_STYLUS_PALM_DETECTION_FILTER_UTIL_H_

#include <cstdint>
#include <deque>
#include <vector>

#include "base/time/time.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"
#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter_model.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

struct COMPONENT_EXPORT(EVDEV) PalmFilterDeviceInfo {
  float max_x = 0.f;
  float max_y = 0.f;
  float x_res = 1.f;
  float y_res = 1.f;
  float major_radius_res = 1.f;
  float minor_radius_res = 1.f;
  bool minor_radius_supported = false;
};

COMPONENT_EXPORT(EVDEV)
PalmFilterDeviceInfo CreatePalmFilterDeviceInfo(const EventDeviceInfo& devinfo);

// Data for a single touch event.
struct COMPONENT_EXPORT(EVDEV) PalmFilterSample {
  float major_radius = 0;
  float minor_radius = 0;
  float pressure = 0;
  float edge = 0;
  int tracking_id = 0;
  gfx::PointF point;
  base::TimeTicks time;

  bool operator==(const PalmFilterSample& other) const {
    return major_radius == other.major_radius &&
           minor_radius == other.minor_radius && pressure == other.pressure &&
           edge == other.edge && tracking_id == other.tracking_id &&
           point == other.point && time == other.time;
  }
};

COMPONENT_EXPORT(EVDEV)
PalmFilterSample CreatePalmFilterSample(
    const InProgressTouchEvdev& touch,
    const base::TimeTicks& time,
    const NeuralStylusPalmDetectionFilterModelConfig& model_config,
    const PalmFilterDeviceInfo& dev_info);

class COMPONENT_EXPORT(EVDEV) PalmFilterStroke {
 public:
  explicit PalmFilterStroke(
      const NeuralStylusPalmDetectionFilterModelConfig& model_config,
      int tracking_id);
  PalmFilterStroke(const PalmFilterStroke& other);
  PalmFilterStroke(PalmFilterStroke&& other);
  ~PalmFilterStroke();

  void ProcessSample(const PalmFilterSample& sample);
  gfx::PointF GetCentroid() const;
  float BiggestSize() const;
  // If no elements in stroke, returns 0.0;
  float MaxMajorRadius() const;
  const std::deque<PalmFilterSample>& samples() const;
  uint64_t samples_seen() const;
  int tracking_id() const;

 private:
  void AddToUnscaledCentroid(const gfx::Vector2dF point);
  void AddSample(const PalmFilterSample& sample);
  /**
   * Process the sample. Potentially store the resampled sample into samples_.
   */
  void Resample(const PalmFilterSample& sample);

  std::deque<PalmFilterSample> samples_;
  const int tracking_id_;
  /**
   * How many total samples have been reported for this stroke. This is
   * different from samples_.size() because samples_ will get pruned to only
   * keep a certain number of last samples.
   * When resampling is enabled, this value will be equal to the number of
   * resampled values that this stroke has received. It may not be equal to the
   * number of times 'AddSample' has been called.
   */
  uint64_t samples_seen_ = 0;
  /**
   * The last sample seen by the model. Used when resampling is enabled in order
   * to compute the resampled value.
   */
  PalmFilterSample last_sample_;

  const uint64_t max_sample_count_;
  const absl::optional<base::TimeDelta> resample_period_;

  gfx::PointF unscaled_centroid_ = gfx::PointF(0., 0.);
  // Used in part of the kahan summation.
  gfx::Vector2dF unscaled_centroid_sum_error_ =
      gfx::PointF(0., 0.).OffsetFromOrigin();
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_NEURAL_STYLUS_PALM_DETECTION_FILTER_UTIL_H_
