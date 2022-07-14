// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter_util.h"

#include <algorithm>

namespace ui {

PalmFilterDeviceInfo CreatePalmFilterDeviceInfo(
    const EventDeviceInfo& devinfo) {
  PalmFilterDeviceInfo info;

  info.max_x = devinfo.GetAbsMaximum(ABS_MT_POSITION_X);
  info.x_res = devinfo.GetAbsResolution(ABS_MT_POSITION_X);
  info.max_y = devinfo.GetAbsMaximum(ABS_MT_POSITION_Y);
  info.y_res = devinfo.GetAbsResolution(ABS_MT_POSITION_Y);
  if (info.x_res == 0) {
    info.x_res = 1;
  }
  if (info.y_res == 0) {
    info.y_res = 1;
  }

  info.major_radius_res = devinfo.GetAbsResolution(ABS_MT_TOUCH_MAJOR);
  if (info.major_radius_res == 0) {
    // Device does not report major res: set to 1.
    info.major_radius_res = 1;
  }
  if (devinfo.HasAbsEvent(ABS_MT_TOUCH_MINOR)) {
    info.minor_radius_supported = true;
    info.minor_radius_res = devinfo.GetAbsResolution(ABS_MT_TOUCH_MINOR);
  } else {
    info.minor_radius_supported = false;
    info.minor_radius_res = info.major_radius_res;
  }
  if (info.minor_radius_res == 0) {
    // Device does not report minor res: set to 1.
    info.minor_radius_res = 1;
  }

  return info;
}

namespace {
float ScaledRadius(
    float radius,
    const NeuralStylusPalmDetectionFilterModelConfig& model_config) {
  if (model_config.radius_polynomial_resize.empty()) {
    return radius;
  }
  float return_value = 0.0f;
  for (uint32_t i = 0; i < model_config.radius_polynomial_resize.size(); ++i) {
    float power = model_config.radius_polynomial_resize.size() - 1 - i;
    return_value +=
        model_config.radius_polynomial_resize[i] * powf(radius, power);
  }
  return return_value;
}

float interpolate(float start_value, float end_value, float proportion) {
  return start_value + (end_value - start_value) * proportion;
}

/**
 * During resampling, the later events are used as a basis to populate
 * non-resampled fields like major and minor. However, if the requested time is
 * within this delay of the earlier event, the earlier event will be used as a
 * basis instead.
 */
const static auto kPreferInitialEventDelay = base::Microseconds(1);

/**
 * Interpolate between the "before" and "after" events to get a resampled value
 * at the timestamp 'time'. Not all fields are interpolated. For fields that are
 * not interpolated, the values are taken from the 'after' sample unless the
 * requested time is very close to the 'before' sample.
 */
PalmFilterSample getSampleAtTime(base::TimeTicks time,
                                 const PalmFilterSample& before,
                                 const PalmFilterSample& after) {
  // Use the newest sample as the base, except when the requested time is very
  // close to the 'before' sample.
  PalmFilterSample result = after;
  if (time - before.time < kPreferInitialEventDelay) {
    result = before;
  }
  // Only the x and y values are interpolated. We could also interpolate the
  // oval size and orientation, but it's not a simple computation, and would
  // likely not provide much value.
  const float proportion =
      static_cast<float>((time - before.time).InNanoseconds()) /
      (after.time - before.time).InNanoseconds();
  result.edge = interpolate(before.edge, after.edge, proportion);
  result.point.set_x(
      interpolate(before.point.x(), after.point.x(), proportion));
  result.point.set_y(
      interpolate(before.point.y(), after.point.y(), proportion));
  result.time = time;
  return result;
}
}  // namespace

PalmFilterSample CreatePalmFilterSample(
    const InProgressTouchEvdev& touch,
    const base::TimeTicks& time,
    const NeuralStylusPalmDetectionFilterModelConfig& model_config,
    const PalmFilterDeviceInfo& dev_info) {
  // radius_x and radius_y have been
  // scaled by resolution already.

  PalmFilterSample sample;
  sample.time = time;

  sample.major_radius = ScaledRadius(
      std::max(touch.major, touch.minor) / dev_info.major_radius_res,
      model_config);
  if (dev_info.minor_radius_supported) {
    sample.minor_radius = ScaledRadius(
        std::min(touch.major, touch.minor) / dev_info.minor_radius_res,
        model_config);
  } else {
    sample.minor_radius = ScaledRadius(touch.major, model_config);
  }

  float nearest_x_edge = std::min(touch.x, dev_info.max_x - touch.x);
  float nearest_y_edge = std::min(touch.y, dev_info.max_y - touch.y);
  float normalized_x_edge = nearest_x_edge / dev_info.x_res;
  float normalized_y_edge = nearest_y_edge / dev_info.y_res;
  // Nearest edge distance, in mm.
  sample.edge = std::min(normalized_x_edge, normalized_y_edge);
  sample.point =
      gfx::PointF(touch.x / dev_info.x_res, touch.y / dev_info.y_res);
  sample.tracking_id = touch.tracking_id;
  sample.pressure = touch.pressure;

  return sample;
}

PalmFilterStroke::PalmFilterStroke(
    const NeuralStylusPalmDetectionFilterModelConfig& model_config,
    int tracking_id)
    : tracking_id_(tracking_id),
      max_sample_count_(model_config.max_sample_count),
      resample_period_(model_config.resample_period) {}
PalmFilterStroke::PalmFilterStroke(const PalmFilterStroke& other) = default;
PalmFilterStroke::PalmFilterStroke(PalmFilterStroke&& other) = default;
PalmFilterStroke::~PalmFilterStroke() {}

void PalmFilterStroke::ProcessSample(const PalmFilterSample& sample) {
  DCHECK_EQ(tracking_id_, sample.tracking_id);
  if (resample_period_.has_value()) {
    Resample(sample);
    return;
  }

  AddSample(sample);

  while (samples_.size() > max_sample_count_) {
    AddToUnscaledCentroid(-samples_.front().point.OffsetFromOrigin());
    samples_.pop_front();
  }
}

void PalmFilterStroke::AddSample(const PalmFilterSample& sample) {
  AddToUnscaledCentroid(sample.point.OffsetFromOrigin());
  samples_.push_back(sample);
  samples_seen_++;
}

/**
 * When resampling is enabled, we don't store all samples. Only the resampled
 * values are stored into samples_. In addition, the last real event is stored
 * into last_sample_, which is used to calculate the resampled values.
 */
void PalmFilterStroke::Resample(const PalmFilterSample& sample) {
  if (samples_seen_ == 0) {
    AddSample(sample);
    last_sample_ = sample;
    return;
  }

  // We already have a valid last sample here.
  DCHECK_LE(last_sample_.time, sample.time);
  // Generate resampled values
  base::TimeTicks next_sample_time = samples_.back().time + *resample_period_;
  while (next_sample_time <= sample.time) {
    AddSample(getSampleAtTime(next_sample_time, last_sample_, sample));
    next_sample_time = samples_.back().time + (*resample_period_);
  }
  last_sample_ = sample;

  // Prune the resampled collection
  while ((samples_.back().time - samples_.front().time) >=
         (*resample_period_) * max_sample_count_) {
    AddToUnscaledCentroid(-samples_.front().point.OffsetFromOrigin());
    samples_.pop_front();
  }
}

void PalmFilterStroke::AddToUnscaledCentroid(const gfx::Vector2dF point) {
  const gfx::Vector2dF corrected_point = point - unscaled_centroid_sum_error_;
  const gfx::PointF new_unscaled_centroid =
      unscaled_centroid_ + corrected_point;
  unscaled_centroid_sum_error_ =
      (new_unscaled_centroid - unscaled_centroid_) - corrected_point;
  unscaled_centroid_ = new_unscaled_centroid;
}

gfx::PointF PalmFilterStroke::GetCentroid() const {
  if (samples_.size() == 0) {
    return gfx::PointF(0., 0.);
  }
  return gfx::ScalePoint(unscaled_centroid_, 1.f / samples_.size());
}

const std::deque<PalmFilterSample>& PalmFilterStroke::samples() const {
  return samples_;
}

int PalmFilterStroke::tracking_id() const {
  return tracking_id_;
}

uint64_t PalmFilterStroke::samples_seen() const {
  return samples_seen_;
}

float PalmFilterStroke::MaxMajorRadius() const {
  float maximum = 0.0;
  for (const auto& sample : samples_) {
    maximum = std::max(maximum, sample.major_radius);
  }
  return maximum;
}

float PalmFilterStroke::BiggestSize() const {
  float biggest = 0;
  for (const auto& sample : samples_) {
    float size;
    if (sample.minor_radius <= 0) {
      size = sample.major_radius * sample.major_radius;
    } else {
      size = sample.major_radius * sample.minor_radius;
    }
    biggest = std::max(biggest, size);
  }
  return biggest;
}

}  // namespace ui
