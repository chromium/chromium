// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter_model.h"

#include "base/logging.h"

namespace ui {

NeuralStylusPalmDetectionFilterModelConfig::
    NeuralStylusPalmDetectionFilterModelConfig() = default;

NeuralStylusPalmDetectionFilterModelConfig::
    NeuralStylusPalmDetectionFilterModelConfig(
        const NeuralStylusPalmDetectionFilterModelConfig& other) = default;

NeuralStylusPalmDetectionFilterModelConfig::
    ~NeuralStylusPalmDetectionFilterModelConfig() = default;

base::TimeDelta
NeuralStylusPalmDetectionFilterModelConfig::GetEquivalentDuration(
    uint32_t sample_count) const {
  if (!resample_period) {
    LOG(DFATAL) << __func__
                << " should only be called if resampling is enabled";
    return base::Microseconds(0);
  }
  if (sample_count <= 1) {
    return base::Microseconds(0);
  }
  return (sample_count - 1) * (*resample_period);
}

}  // namespace ui
