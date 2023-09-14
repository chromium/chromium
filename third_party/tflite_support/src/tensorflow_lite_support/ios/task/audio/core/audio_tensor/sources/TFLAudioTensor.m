// Copyright 2022 The TensorFlow Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "tensorflow_lite_support/ios/task/audio/core/audio_tensor/sources/TFLAudioTensor.h"
#import "tensorflow_lite_support/ios/sources/TFLCommon.h"
#import "tensorflow_lite_support/ios/sources/TFLCommonUtils.h"
#import "tensorflow_lite_support/ios/task/audio/core/sources/TFLRingBuffer.h"

@implementation TFLAudioTensor {
  TFLRingBuffer *_ringBuffer;
}

- (instancetype)initWithAudioFormat:(TFLAudioFormat *)format sampleCount:(NSUInteger)sampleCount {
  self = [super init];
  if (self) {
    _audioFormat = format;

    const NSInteger size = sampleCount * format.channelCount;
    _ringBuffer = [[TFLRingBuffer alloc] initWithBufferSize:size];
  }
  return self;
}

- (BOOL)loadBuffer:(TFLFloatBuffer *)buffer
            offset:(NSUInteger)offset
              size:(NSUInteger)size
             error:(NSError **)error {
  return [_ringBuffer loadFloatData:buffer.data
                           dataSize:buffer.size
                             offset:offset
                               size:buffer.size
                              error:error];
}

- (BOOL)loadAudioRecord:(TFLAudioRecord *)audioRecord withError:(NSError **)error {
  if (![self.audioFormat isEqual:audioRecord.audioFormat]) {
    [TFLCommonUtils
        createCustomError:error
                 withCode:TFLSupportErrorCodeInvalidArgumentError
              description:@"Audio format of TFLAudioRecord does not match the audio format "
                          @"of Tensor Audio. Please ensure that the channelCount and "
                          @"sampleRate of both audio formats are equal."];
    return NO;
  }

  NSUInteger sizeToLoad = audioRecord.bufferSize;
  TFLFloatBuffer *buffer = [audioRecord readAtOffset:0 withSize:sizeToLoad error:error];

  if (!buffer) {
    return NO;
  }

  return [self loadBuffer:buffer offset:0 size:sizeToLoad error:error];
}

- (TFLFloatBuffer *)buffer {
  return _ringBuffer.floatBuffer;
}

- (NSUInteger)bufferSize {
  return _ringBuffer.size;
}

@end
