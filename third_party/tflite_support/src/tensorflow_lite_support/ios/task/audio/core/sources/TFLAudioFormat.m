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

#import "tensorflow_lite_support/ios/task/audio/core/sources/TFLAudioFormat.h"

#define DEFAULT_CHANNEL_COUNT 1

@implementation TFLAudioFormat

- (instancetype)initWithChannelCount:(NSUInteger)channelCount sampleRate:(NSUInteger)sampleRate {
  self = [super init];
  if (self) {
    _channelCount = channelCount;
    _sampleRate = sampleRate;
  }
  return self;
}

- (instancetype)initWithSampleRate:(NSUInteger)sampleRate {
  return [self initWithChannelCount:DEFAULT_CHANNEL_COUNT sampleRate:sampleRate];
}

- (BOOL)isEqual:(id)object {
  return [object isKindOfClass:[self class]] &&
         self.channelCount == [(TFLAudioFormat *)object channelCount] &&
         self.sampleRate == [(TFLAudioFormat *)object sampleRate];
}

@end
