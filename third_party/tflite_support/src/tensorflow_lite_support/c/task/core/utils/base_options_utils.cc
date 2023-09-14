/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow_lite_support/c/task/core/utils/base_options_utils.h"

namespace tflite {
namespace task {
namespace core {

TfLiteBaseOptions CreateDefaultBaseOptions() {
  TfLiteBaseOptions base_options = {{0}};
  base_options.compute_settings.cpu_settings.num_threads = -1;
  base_options.compute_settings.coreml_delegate_settings.enable_delegate =
      false;
  return base_options;
}

  ::tflite::proto::TFLiteSettings TfLiteSettingsProtoFromCOptions(
    const TfLiteComputeSettings* c_options) {
  ::tflite::proto::TFLiteSettings tflite_settings;

  if (c_options == nullptr) {
    return tflite_settings;
  }

  // c_options->base_options.compute_settings.num_threads is expected to be
  // set to value > 0 or -1. Otherwise invoking
  // ImageClassifierCpp::CreateFromOptions() results in a not ok status.
  tflite_settings.mutable_cpu_settings()->set_num_threads(
      c_options->cpu_settings.num_threads);

  if (c_options->coreml_delegate_settings.enable_delegate) {
    tflite_settings.set_delegate(::tflite::proto::Delegate::CORE_ML);
    switch (c_options->coreml_delegate_settings.enabled_devices) {
      case kDevicesAll:
        tflite_settings.mutable_coreml_settings()->set_enabled_devices(
            ::tflite::proto::CoreMLSettings::DEVICES_ALL);
        break;
      case kDevicesWithNeuralEngine:
        tflite_settings.mutable_coreml_settings()->set_enabled_devices(
            ::tflite::proto::CoreMLSettings::DEVICES_WITH_NEURAL_ENGINE);
        break;
    }
  }

  return tflite_settings;
}

}  // namespace core
}  // namespace task
}  // namespace tflite
