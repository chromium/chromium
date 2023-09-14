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

#import "tensorflow_lite_support/ios/task/audio/core/sources/TFLFloatBuffer.h"

@implementation TFLFloatBuffer

- (instancetype)initWithData:(float *)data size:(NSUInteger)size {
  self = [self init];
  if (self) {
    _size = size;
    _data = malloc(sizeof(float) * size);
    if (!_data) {
      exit(-1);
    }
    if (data) {
      memcpy(_data, data, sizeof(float) * size);
    }
  }
  return self;
}

- (instancetype)initWithSize:(NSUInteger)size {
  self = [self init];
  if (self) {
    _size = size;
    _data = calloc(size, sizeof(float));
    if (!_data) {
      exit(-1);
    }
  }
  return self;
}

- (id)copyWithZone:(NSZone *)zone {
  return [[TFLFloatBuffer alloc] initWithData:_data size:_size];
}

- (void)clear {
  memset(_data, 0, sizeof(float) * _size);
}

- (void)dealloc {
  free(_data);
}

@end
