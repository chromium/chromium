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

#include "tensorflow_lite_support/c/task/vision/utils/frame_buffer_cpp_c_utils.h"

#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"

namespace tflite {
namespace task {
namespace vision {

namespace {
using FrameBufferCpp = ::tflite::task::vision::FrameBuffer;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
}  // namespace

StatusOr<std::unique_ptr<FrameBufferCpp>> CreateCppFrameBuffer(
    const TfLiteFrameBuffer* frame_buffer) {
  if (frame_buffer == nullptr)
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Expected non null frame buffer."),
        TfLiteSupportStatus::kInvalidArgumentError);

  FrameBufferCpp::Format frame_buffer_format =
      FrameBufferCpp::Format(frame_buffer->format);

  return CreateFromRawBuffer(
      frame_buffer->buffer,
      {frame_buffer->dimension.width, frame_buffer->dimension.height},
      frame_buffer_format);
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
