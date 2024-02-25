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

#import "tensorflow_lite_support/ios/task/audio/core/sources/TFLRingBuffer.h"
#import "tensorflow_lite_support/ios/sources/TFLCommon.h"
#import "tensorflow_lite_support/ios/sources/TFLCommonUtils.h"

@implementation TFLRingBuffer {
  NSUInteger _nextIndex;
  TFLFloatBuffer *_buffer;
}

- (instancetype)initWithBufferSize:(NSUInteger)size {
  self = [self init];
  if (self) {
    _buffer = [[TFLFloatBuffer alloc] initWithSize:size];
  }
  return self;
}

- (BOOL)loadFloatData:(float *)data
             dataSize:(NSUInteger)dataSize
               offset:(NSUInteger)offset
                 size:(NSUInteger)size
                error:(NSError **)error {
  NSUInteger sizeToCopy = size;
  NSUInteger newOffset = offset;

  if (offset + size > dataSize) {
    [TFLCommonUtils
        createCustomError:error
                 withCode:TFLSupportErrorCodeInvalidArgumentError
              description:@"offset + size exceeds the maximum size of the source buffer."];
    return NO;
  }

  // Length is greater than buffer size, then modify size and offset to
  // keep most recent data in the sourceBuffer.
  if (size >= _buffer.size) {
    sizeToCopy = _buffer.size;
    newOffset = offset + (size - _buffer.size);
  }

  // If the new nextIndex + sizeToCopy is smaller than the size of the ring buffer directly
  // copy all elements to the end of the ring buffer.
  if (_nextIndex + sizeToCopy < _buffer.size) {
    memcpy(_buffer.data + _nextIndex, data + newOffset, sizeof(float) * sizeToCopy);
  } else {
    NSUInteger endChunkSize = _buffer.size - _nextIndex;
    memcpy(_buffer.data + _nextIndex, data + newOffset, sizeof(float) * endChunkSize);

    NSUInteger startChunkSize = sizeToCopy - endChunkSize;
    memcpy(_buffer.data, data + newOffset + endChunkSize, sizeof(float) * startChunkSize);
  }

  _nextIndex = (_nextIndex + sizeToCopy) % _buffer.size;

  return YES;
}

- (TFLFloatBuffer *)floatBuffer {
  return [self floatBufferWithOffset:0 size:self.size];
}

- (nullable TFLFloatBuffer *)floatBufferWithOffset:(NSUInteger)offset size:(NSUInteger)size {
  if (offset + size > _buffer.size) {
    return nil;
  }

  TFLFloatBuffer *bufferToReturn = [[TFLFloatBuffer alloc] initWithSize:size];

  // Return buffer in correct order.
  // Compute offset in flat ring buffer array considering warping.
  NSInteger correctOffset = (_nextIndex + offset) % _buffer.size;

  // If no; elements to be copied are within the end of the flat ring buffer.
  if ((correctOffset + size) <= _buffer.size) {
    memcpy(bufferToReturn.data, _buffer.data + correctOffset, sizeof(float) * size);
  } else {
    // If no; elements to be copied warps around to the beginning of the ring buffer.
    // Copy the chunk starting at ringBuffer[nextIndex + offset : size] to
    // beginning of the result array.
    NSInteger endChunkSize = _buffer.size - correctOffset;
    memcpy(bufferToReturn.data, _buffer.data + correctOffset, sizeof(float) * endChunkSize);

    // Next copy the chunk starting at ringBuffer[0 : size - endChunkSize] to the result array.
    NSInteger firstChunkSize = size - endChunkSize;
    memcpy(bufferToReturn.data + endChunkSize, _buffer.data, sizeof(float) * firstChunkSize);
  }

  return bufferToReturn;
}

- (void)clear {
  [_buffer clear];
}

- (NSUInteger)size {
  return _buffer.size;
}

@end
