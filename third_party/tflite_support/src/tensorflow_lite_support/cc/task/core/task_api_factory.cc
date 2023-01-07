/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"

namespace tflite {
namespace task {
namespace core {

// static
absl::Status TaskAPIFactory::SetMiniBenchmarkFileNameFromBaseOptions(
    ::tflite::proto::ComputeSettings& compute_settings,
    const BaseOptions* base_options) {
  if (!base_options->has_model_file()) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "Missing mandatory `model_file` field in `base_options`",
        tflite::support::TfLiteSupportStatus::kInvalidArgumentError);
  }
  if (base_options->model_file().has_file_name()) {
    compute_settings.mutable_settings_to_test_locally()
        ->mutable_model_file()
        ->set_filename(base_options->model_file().file_name());
  } else if (base_options->model_file().has_file_descriptor_meta()) {
    const task::core::FileDescriptorMeta& fd_meta =
        base_options->model_file().file_descriptor_meta();
    auto* mutable_model_file =
        compute_settings.mutable_settings_to_test_locally()
            ->mutable_model_file();
    mutable_model_file->set_fd(fd_meta.fd());
    mutable_model_file->set_offset(fd_meta.offset());
    mutable_model_file->set_length(fd_meta.length());
  } else {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "Mini-benchmark is currently not able to run on model passed as "
        "bytes.",
        tflite::support::TfLiteSupportStatus::kInvalidArgumentError);
  }

  return absl::OkStatus();
}

}  // namespace core
}  // namespace task
}  // namespace tflite
