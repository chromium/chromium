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

#include "tensorflow_lite_support/python/task/core/pybinds/task_utils.h"

namespace tflite {
namespace task {
namespace core {

namespace {
using PythonBaseOptions = ::tflite::python::task::core::BaseOptions;
using CppBaseOptions = ::tflite::task::core::BaseOptions;
}  // namespace

std::unique_ptr<CppBaseOptions> convert_to_cpp_base_options(
    PythonBaseOptions options) {
  std::unique_ptr<CppBaseOptions> cpp_options =
      std::make_unique<CppBaseOptions>();
  if (options.has_file_content()) {
    cpp_options->mutable_model_file()->set_file_content(options.file_content());
  }
  if (options.has_file_name()) {
    cpp_options->mutable_model_file()->set_file_name(options.file_name());
  }

  cpp_options->mutable_compute_settings()
      ->mutable_tflite_settings()
      ->mutable_cpu_settings()
      ->set_num_threads(options.num_threads());

  if (options.use_coral()) {
    cpp_options->mutable_compute_settings()
        ->mutable_tflite_settings()
        ->set_delegate(tflite::proto::Delegate::EDGETPU_CORAL);
  }
  return cpp_options;
}

}  // namespace core
}  // namespace task
}  // namespace tflite
