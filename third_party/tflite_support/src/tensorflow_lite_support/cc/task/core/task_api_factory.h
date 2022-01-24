/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_LITE_SUPPORT_CC_TASK_CORE_TASK_API_FACTORY_H_
#define TENSORFLOW_LITE_SUPPORT_CC_TASK_CORE_TASK_API_FACTORY_H_

#include <memory>

#include "absl/status/status.h"
#include "tensorflow/lite/core/api/op_resolver.h"
#include "tensorflow/lite/kernels/op_macros.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/base_task_api.h"
#include "tensorflow_lite_support/cc/task/core/proto/external_file_proto_inc.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"

namespace tflite {
namespace task {
namespace core {
template <typename T>
using EnableIfBaseUntypedTaskApiSubclass = typename std::enable_if<
    std::is_base_of<BaseUntypedTaskApi, T>::value>::type*;

// Template creator for all subclasses of BaseTaskApi
class TaskAPIFactory {
 public:
  TaskAPIFactory() = delete;

  template <typename T, EnableIfBaseUntypedTaskApiSubclass<T> = nullptr>
  static tflite::support::StatusOr<std::unique_ptr<T>> CreateFromBuffer(
      const char* buffer_data,
      size_t buffer_size,
      std::unique_ptr<tflite::OpResolver> resolver =
          absl::make_unique<tflite::ops::builtin::BuiltinOpResolver>(),
      int num_threads = 1) {
    auto engine = absl::make_unique<TfLiteEngine>(std::move(resolver));
    RETURN_IF_ERROR(engine->BuildModelFromFlatBuffer(buffer_data, buffer_size));
    return CreateFromTfLiteEngine<T>(std::move(engine), num_threads);
  }

  template <typename T, EnableIfBaseUntypedTaskApiSubclass<T> = nullptr>
  static tflite::support::StatusOr<std::unique_ptr<T>> CreateFromFile(
      const string& file_name,
      std::unique_ptr<tflite::OpResolver> resolver =
          absl::make_unique<tflite::ops::builtin::BuiltinOpResolver>(),
      int num_threads = 1) {
    auto engine = absl::make_unique<TfLiteEngine>(std::move(resolver));
    RETURN_IF_ERROR(engine->BuildModelFromFile(file_name));
    return CreateFromTfLiteEngine<T>(std::move(engine), num_threads);
  }

  template <typename T, EnableIfBaseUntypedTaskApiSubclass<T> = nullptr>
  static tflite::support::StatusOr<std::unique_ptr<T>> CreateFromFileDescriptor(
      int file_descriptor,
      std::unique_ptr<tflite::OpResolver> resolver =
          absl::make_unique<tflite::ops::builtin::BuiltinOpResolver>(),
      int num_threads = 1) {
    auto engine = absl::make_unique<TfLiteEngine>(std::move(resolver));
    RETURN_IF_ERROR(engine->BuildModelFromFileDescriptor(file_descriptor));
    return CreateFromTfLiteEngine<T>(std::move(engine), num_threads);
  }

  template <typename T, EnableIfBaseUntypedTaskApiSubclass<T> = nullptr>
  static tflite::support::StatusOr<std::unique_ptr<T>>
  CreateFromExternalFileProto(
      const ExternalFile* external_file,
      std::unique_ptr<tflite::OpResolver> resolver =
          absl::make_unique<tflite::ops::builtin::BuiltinOpResolver>(),
      int num_threads = 1) {
    auto engine = absl::make_unique<TfLiteEngine>(std::move(resolver));
    RETURN_IF_ERROR(engine->BuildModelFromExternalFileProto(external_file));
    return CreateFromTfLiteEngine<T>(std::move(engine), num_threads);
  }

 private:
  template <typename T, EnableIfBaseUntypedTaskApiSubclass<T> = nullptr>
  static tflite::support::StatusOr<std::unique_ptr<T>> CreateFromTfLiteEngine(
      std::unique_ptr<TfLiteEngine> engine,
      int num_threads) {
    RETURN_IF_ERROR(engine->InitInterpreter(num_threads));
    return absl::make_unique<T>(std::move(engine));
  }
};

}  // namespace core
}  // namespace task
}  // namespace tflite

#endif  // TENSORFLOW_LITE_SUPPORT_CC_TASK_CORE_TASK_API_FACTORY_H_
