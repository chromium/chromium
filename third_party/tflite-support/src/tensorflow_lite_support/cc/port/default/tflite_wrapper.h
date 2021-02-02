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
#ifndef TENSORFLOW_LITE_SUPPORT_CC_PORT_DEFAULT_TFLITE_WRAPPER_H_
#define TENSORFLOW_LITE_SUPPORT_CC_PORT_DEFAULT_TFLITE_WRAPPER_H_

#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "tensorflow/lite/experimental/acceleration/configuration/configuration.pb.h"
#include "tensorflow/lite/interpreter.h"

namespace tflite {
namespace support {

// Wrapper for a TfLiteInterpreter that may be accelerated[1]. This is NOT yet
// implemented: this class only provides a first, minimal interface in the
// meanwhile.
//
// [1] See tensorflow/lite/experimental/acceleration for more details.
class TfLiteInterpreterWrapper {
 public:
  TfLiteInterpreterWrapper() = default;

  virtual ~TfLiteInterpreterWrapper() = default;

  // Calls `interpreter_initializer` and then `AllocateTensors`. Future
  // implementation of this method will attempt to apply the provided
  // `compute_settings` with a graceful fallback in case a failure occurs.
  // Note: before this gets implemented, do NOT call this method with non-empty
  // `compute_settings` otherwise an unimplemented error occurs.
  absl::Status InitializeWithFallback(
      std::function<absl::Status(std::unique_ptr<tflite::Interpreter>*)>
          interpreter_initializer,
      const tflite::proto::ComputeSettings& compute_settings);

  // Calls `set_inputs` and then Invoke() on the interpreter. Future
  // implementation of this method will perform a graceful fallback in case a
  // failure occur due to the `compute_settings` provided at initialization
  // time.
  absl::Status InvokeWithFallback(
      const std::function<absl::Status(tflite::Interpreter* interpreter)>&
          set_inputs);

  // Calls Invoke() on the interpreter. Caller must have set up inputs
  // before-hand.
  absl::Status InvokeWithoutFallback();

  // Cancels the current running TFLite invocation on CPU. This method is not
  // yet implemented though it is safe to use it as it acts as a NOP.
  void Cancel();

  // Accesses the underlying interpreter for other methods.
  tflite::Interpreter& operator*() { return *interpreter_; }
  tflite::Interpreter* operator->() { return interpreter_.get(); }
  tflite::Interpreter& operator*() const { return *interpreter_; }
  tflite::Interpreter* operator->() const { return interpreter_.get(); }
  tflite::Interpreter* get() const { return interpreter_.get(); }

  TfLiteInterpreterWrapper(const TfLiteInterpreterWrapper&) = delete;
  TfLiteInterpreterWrapper& operator=(const TfLiteInterpreterWrapper&) = delete;

 private:
  std::unique_ptr<tflite::Interpreter> interpreter_;
};

}  // namespace support
}  // namespace tflite

#endif  // TENSORFLOW_LITE_SUPPORT_CC_PORT_DEFAULT_TFLITE_WRAPPER_H_
