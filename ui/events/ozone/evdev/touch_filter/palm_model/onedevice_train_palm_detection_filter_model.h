// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_PALM_MODEL_ONEDEVICE_TRAIN_PALM_DETECTION_FILTER_MODEL_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_PALM_MODEL_ONEDEVICE_TRAIN_PALM_DETECTION_FILTER_MODEL_H_

#include <cstdint>
#include <vector>

#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter_model.h"

namespace ui {

// A simplified Neural stylus Palm Detection Model, trained on the data based on
// a single device class but translatable to others. Neural inference
// implementation based on inline neural net inference.
class COMPONENT_EXPORT(EVDEV) OneDeviceTrainNeuralStylusPalmDetectionFilterModel
    : public NeuralStylusPalmDetectionFilterModel {
 public:
  explicit OneDeviceTrainNeuralStylusPalmDetectionFilterModel(
      const std::string& model_version,
      const std::vector<float>& radius_poly);

  OneDeviceTrainNeuralStylusPalmDetectionFilterModel(
      const OneDeviceTrainNeuralStylusPalmDetectionFilterModel&) = delete;
  OneDeviceTrainNeuralStylusPalmDetectionFilterModel& operator=(
      const OneDeviceTrainNeuralStylusPalmDetectionFilterModel&) = delete;

  float Inference(const std::vector<float>& features) const override;

  const NeuralStylusPalmDetectionFilterModelConfig& config() const override;

 protected:
  NeuralStylusPalmDetectionFilterModelConfig config_;

 private:
  void Initialize();
  size_t expected_feature_size_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_PALM_MODEL_ONEDEVICE_TRAIN_PALM_DETECTION_FILTER_MODEL_H_
