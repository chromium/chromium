// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/palm_model/onedevice_train_palm_detection_filter_model.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "ui/events/ozone/evdev/touch_filter/palm_model/onedevice_train_palm_detection_filter_inference.h"
#define USE_EIGEN 0

namespace ui {

float OneDeviceTrainNeuralStylusPalmDetectionFilterModel::Inference(
    const std::vector<float>& features) const {
  DVLOG(1) << "In Inference.";
  std::unique_ptr<internal_onedevice::FixedAllocations> fixed_allocations(
      new internal_onedevice::FixedAllocations());
  if (features.size() != 193) {
    LOG(DFATAL) << "Bad count. Is " << features.size() << " expected " << 193;
    return nanf("");
  }
  // TODO(robsc): Update to DVLOG_IS_ON if relevant.
  if (DCHECK_IS_ON() && VLOG_IS_ON(1)) {
    for (unsigned i = 0; i < features.size(); ++i) {
      DVLOG(1) << "Feature " << i << " is " << features[i];
    }
  }
  float output = 0;
  internal_onedevice::Inference(&features[0], &output, fixed_allocations.get());
  return output;
}

const NeuralStylusPalmDetectionFilterModelConfig&
OneDeviceTrainNeuralStylusPalmDetectionFilterModel::config() const {
  return config_;
}

OneDeviceTrainNeuralStylusPalmDetectionFilterModel::
    OneDeviceTrainNeuralStylusPalmDetectionFilterModel() {
  config_.nearest_neighbor_count = 0;
  config_.biggest_near_neighbor_count = 2;
  config_.include_sequence_count_in_strokes = true;
  config_.max_neighbor_distance_in_mm = 100.0f;
  config_.min_sample_count = 6;
  config_.max_sample_count = 12;
  config_.max_dead_neighbor_time = base::TimeDelta::FromMillisecondsD(100.0f);
  config_.heuristic_palm_touch_limit = 20.0f;
  config_.heuristic_palm_area_limit = 400.0f;
}

OneDeviceTrainNeuralStylusPalmDetectionFilterModel::
    OneDeviceTrainNeuralStylusPalmDetectionFilterModel(
        const std::vector<float>& radius_poly)
    : OneDeviceTrainNeuralStylusPalmDetectionFilterModel() {
  config_.radius_polynomial_resize = radius_poly;
}

}  // namespace ui
