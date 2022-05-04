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

#include "base/feature_list.h"
#include "base/logging.h"
#include "ui/events/ozone/evdev/touch_filter/palm_model/onedevice_train_palm_detection_filter_inference.h"
#include "ui/events/ozone/evdev/touch_filter/palm_model/onedevice_train_palm_detection_filter_inference_v2.h"
#include "ui/events/ozone/features.h"
#define USE_EIGEN 0

namespace ui {

float OneDeviceTrainNeuralStylusPalmDetectionFilterModel::Inference(
    const std::vector<float>& features) const {
  DVLOG(1) << "In Inference.";
  if (features.size() != expected_feature_size_) {
    LOG(DFATAL) << "Bad count. Is " << features.size() << " expected "
                << expected_feature_size_;
    return nanf("");
  }
  // TODO(robsc): Update to DVLOG_IS_ON if relevant.
  if (DCHECK_IS_ON() && VLOG_IS_ON(1)) {
    for (unsigned i = 0; i < features.size(); ++i) {
      DVLOG(1) << "Feature " << i << " is " << features[i];
    }
  }
  float output = 0;
  if (base::FeatureList::IsEnabled(kEnableNeuralPalmRejectionModelV2)) {
    std::unique_ptr<v2::internal_onedevice::FixedAllocations> fixed_allocations(
        new v2::internal_onedevice::FixedAllocations());
    v2::internal_onedevice::Inference(&features[0], &output,
                                      fixed_allocations.get());
  } else {
    std::unique_ptr<internal_onedevice::FixedAllocations> fixed_allocations(
        new internal_onedevice::FixedAllocations());
    internal_onedevice::Inference(&features[0], &output,
                                  fixed_allocations.get());
  }
  return output;
}

const NeuralStylusPalmDetectionFilterModelConfig&
OneDeviceTrainNeuralStylusPalmDetectionFilterModel::config() const {
  return config_;
}

OneDeviceTrainNeuralStylusPalmDetectionFilterModel::
    OneDeviceTrainNeuralStylusPalmDetectionFilterModel() {
  // Common configurations:
  config_.include_sequence_count_in_strokes = true;
  config_.max_neighbor_distance_in_mm = 100.0f;
  config_.max_dead_neighbor_time = base::Milliseconds(100.0f);
  config_.heuristic_palm_touch_limit = 20.0f;
  config_.heuristic_palm_area_limit = 400.0f;
  config_.max_blank_time = base::Milliseconds(100.0f);
  config_.nearest_neighbor_count = 0;
  config_.biggest_near_neighbor_count = 4;

  if (base::FeatureList::IsEnabled(kEnableNeuralPalmRejectionModelV2)) {
    config_.min_sample_count = 3;
    config_.max_sample_count = 6;
    config_.neighbor_min_sample_count = 1;
    config_.output_threshold = 0.90271f;
    expected_feature_size_ = 173;

    if (base::FeatureList::IsEnabled(kEnableNeuralPalmAdaptiveHold)) {
      config_.nn_delay_start_if_palm = true;
      config_.early_stage_sample_counts = std::unordered_set<uint32_t>({2});
    }
  } else {
    config_.min_sample_count = 5;
    config_.max_sample_count = 12;
    config_.neighbor_min_sample_count = 5;
    config_.output_threshold = 2.519f;
    expected_feature_size_ = 323;
  }
}

OneDeviceTrainNeuralStylusPalmDetectionFilterModel::
    OneDeviceTrainNeuralStylusPalmDetectionFilterModel(
        const std::string& model_version,
        const std::vector<float>& radius_poly)
    : OneDeviceTrainNeuralStylusPalmDetectionFilterModel() {
  config_.model_version = model_version;
  config_.radius_polynomial_resize = radius_poly;
}

}  // namespace ui
