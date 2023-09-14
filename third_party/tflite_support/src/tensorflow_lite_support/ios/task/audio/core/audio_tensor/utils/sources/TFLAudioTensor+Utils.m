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
#import "tensorflow_lite_support/ios/task/audio/core/audio_tensor/utils/sources/TFLAudioTensor+Utils.h"

@implementation TFLAudioTensor (Utils)

// This method expects the caller to pass in the ring buffer's floatBuffer as opposed to getting the
// float buffer using [self.ringBuffer floatBuffer] within the implementation in order to avoid
// performing an extra memcpy. TFLFloatBuffer free's its float *data when it deallocates. Accessing
// [self.ringBuffer floatBuffer] inside this method will cause the returned float* to be deallocated
// when this method call completes and hence will warrant a copy of the float * to the cAudioBuffer.
// Instead the classify() of TFLAudioClassifier calls [audioTensor floatBuffer] and passes it into
// this method which will prevent the float* from being deallocated until classify() method
// completes.
- (TfLiteAudioBuffer)cAudioBufferFromFloatBuffer:(TFLFloatBuffer *)floatBuffer {
  TfLiteAudioFormat cFormat = {.channels = (int)self.audioFormat.channelCount,
                               .sample_rate = (int)self.audioFormat.sampleRate};

  TfLiteAudioBuffer cAudioBuffer = {
      .format = cFormat, .data = floatBuffer.data, .size = floatBuffer.size};

  return cAudioBuffer;
}

@end
