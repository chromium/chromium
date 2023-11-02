// Copyright 2019 The Chromium Authors
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
#include "ui/events/ozone/evdev/touch_filter/palm_model/onedevice_train_palm_detection_filter_inference_beta.h"
#include "ui/events/ozone/features.h"
#define USE_EIGEN 0

namespace ui {
namespace {
const std::string kBetaVersion = "beta";
}

namespace alpha = internal_onedevice::alpha_model;
namespace beta = internal_onedevice::beta_model;

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
  if (config_.model_version == kBetaVersion) {
    std::unique_ptr<beta::FixedAllocations> fixed_allocations(
        new beta::FixedAllocations());
    beta::Inference(&features[0], &output, fixed_allocations.get());
  } else {
    std::unique_ptr<alpha::FixedAllocations> fixed_allocations(
        new alpha::FixedAllocations());
    alpha::Inference(&features[0], &output, fixed_allocations.get());
  }
  return output;
}

const NeuralStylusPalmDetectionFilterModelConfig&
OneDeviceTrainNeuralStylusPalmDetectionFilterModel::config() const {
  return config_;
}

void OneDeviceTrainNeuralStylusPalmDetectionFilterModel::Initialize() {
  // Common configurations:
  config_.include_sequence_count_in_strokes = true;
  config_.max_dead_neighbor_time = base::Milliseconds(100.0f);
  config_.heuristic_palm_touch_limit = 20.0f;
  config_.heuristic_palm_area_limit = 400.0f;
  config_.max_blank_time = base::Milliseconds(100.0f);
  config_.nearest_neighbor_count = 0;

  if (config_.model_version == kBetaVersion) {
    config_.max_neighbor_distance_in_mm = 200.0f;
    config_.biggest_near_neighbor_count = 4;
    config_.min_sample_count = 5;
    config_.max_sample_count = 12;
    config_.neighbor_min_sample_count = 5;
    config_.output_threshold = 4.465f;
    config_.use_tracking_id_count = true;
    config_.use_active_tracking_id_count = true;
    expected_feature_size_ = 325;
  } else {
    config_.max_neighbor_distance_in_mm = 100.0f;
    config_.biggest_near_neighbor_count = 4;

    config_.min_sample_count = 3;
    config_.max_sample_count = 6;
    config_.neighbor_min_sample_count = 1;
    config_.output_threshold = 0.90271f;
    expected_feature_size_ = 173;

    if (base::FeatureList::IsEnabled(kEnableNeuralPalmAdaptiveHold)) {
      config_.nn_delay_start_if_palm = true;
      config_.early_stage_sample_counts = std::unordered_set<uint32_t>({2});
    }
  }
}

OneDeviceTrainNeuralStylusPalmDetectionFilterModel::
    OneDeviceTrainNeuralStylusPalmDetectionFilterModel(
        const std::string& model_version,
        const std::vector<float>& radius_poly) {
  config_.model_version = model_version;
  config_.radius_polynomial_resize = radius_poly;
  Initialize();
}

}  // namespace ui
