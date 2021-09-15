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

#include "tensorflow_lite_support/cc/port/default/tflite_wrapper.h"

#include "absl/status/status.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"

namespace tflite {
namespace support {

absl::Status TfLiteInterpreterWrapper::InitializeWithFallback(
    std::function<absl::Status(std::unique_ptr<tflite::Interpreter>*)>
        interpreter_initializer,
    const tflite::proto::ComputeSettings& compute_settings) {
  if (compute_settings.has_preference() ||
      compute_settings.has_tflite_settings()) {
    return absl::UnimplementedError(
        "Acceleration via ComputeSettings is not supported yet.");
  }
  RETURN_IF_ERROR(interpreter_initializer(&interpreter_));
  return interpreter_->AllocateTensors() != kTfLiteOk
             ? absl::InternalError(
                   "TFLite interpreter: AllocateTensors() failed.")
             : absl::OkStatus();
}

absl::Status TfLiteInterpreterWrapper::InvokeWithFallback(
    const std::function<absl::Status(tflite::Interpreter* interpreter)>&
        set_inputs) {
  RETURN_IF_ERROR(set_inputs(interpreter_.get()));
  return interpreter_->Invoke() != kTfLiteOk
             ? absl::InternalError("TFLite interpreter: Invoke() failed.")
             : absl::OkStatus();
}

absl::Status TfLiteInterpreterWrapper::InvokeWithoutFallback() {
  return interpreter_->Invoke() != kTfLiteOk
             ? absl::InternalError("TFLite interpreter: Invoke() failed.")
             : absl::OkStatus();
}

void TfLiteInterpreterWrapper::Cancel() {
  // NOP
}

}  // namespace support
}  // namespace tflite
