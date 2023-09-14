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
#import "tensorflow_lite_support/ios/sources/TFLCommonCppUtils.h"
#import "tensorflow_lite_support/ios/task/vision/utils/sources/GMLImage+CppUtils.h"
#import "tensorflow_lite_support/ios/task/vision/utils/sources/GMLImage+Utils.h"

#include "absl/strings/str_format.h"  // from @com_google_absl  // from @com_google_absl
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"

namespace {
using FrameBufferCpp = ::tflite::task::vision::FrameBuffer;
using ::tflite::support::StatusOr;
}  // namespace

@implementation GMLImage (CppUtils)

- (std::unique_ptr<tflite::task::vision::FrameBuffer>)
    cppFrameBufferWithUnderlyingBuffer:(uint8_t **)buffer
                                 error:(NSError *_Nullable *)error {
  *buffer = [self bufferWithError:error];

  if (!buffer) {
    return NULL;
  }

  CGSize bitmapSize = self.bitmapSize;
  FrameBufferCpp::Format frame_buffer_format = FrameBufferCpp::Format::kRGB;

  StatusOr<std::unique_ptr<FrameBufferCpp>> frameBuffer =
      CreateFromRawBuffer(*buffer, {(int)bitmapSize.width, (int)bitmapSize.height},
                          frame_buffer_format, FrameBufferCpp::Orientation::kTopLeft);

  if (![TFLCommonCppUtils checkCppError:frameBuffer.status() toError:error]) {
    return NULL;
  }

  return std::move(frameBuffer.value());
}

@end
