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
#import <XCTest/XCTest.h>

#import "tensorflow_lite_support/ios/sources/TFLCommon.h"
#import "tensorflow_lite_support/ios/task/audio/core/sources/TFLRingBuffer.h"

#define VerifyError(error, expectedErrorDomain, expectedErrorCode, expectedLocalizedDescription) \
  XCTAssertEqualObjects(error.domain, expectedErrorDomain);                                      \
  XCTAssertEqual(error.code, expectedErrorCode);                                                 \
  XCTAssertEqualObjects(error.localizedDescription, expectedLocalizedDescription);

NS_ASSUME_NONNULL_BEGIN

@interface TFLRingBufferTests : XCTestCase
@end

@implementation TFLRingBufferTests

- (void)testLoadSucceedsWithFullLengthBuffer {
  NSInteger inDataLength = 5;
  float inData[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

  NSInteger bufferSize = 5;
  TFLRingBuffer *ringBuffer = [[TFLRingBuffer alloc] initWithBufferSize:bufferSize];

  XCTAssertTrue([ringBuffer loadFloatData:&(inData[0])
                                 dataSize:inDataLength
                                   offset:0
                                     size:inDataLength
                                    error:nil]);
  // State after load: [1.0, 2.0, 3.0, 4.0, 5.0]

  TFLFloatBuffer *outBuffer = ringBuffer.floatBuffer;
  XCTAssertNotNil(outBuffer);
  XCTAssertEqual(outBuffer.size, bufferSize);

  float expectedData[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

  for (int i = 0; i < inDataLength; i++) {
    XCTAssertEqual(outBuffer.data[i], expectedData[i]);
  }
}

- (void)testLoadSucceedsWithPartialLengthBuffer {
  NSInteger inDataSize = 3;
  float inData[] = {1.0f, 2.0f, 3.0f};

  NSInteger bufferSize = 5;
  TFLRingBuffer *ringBuffer = [[TFLRingBuffer alloc] initWithBufferSize:bufferSize];

  XCTAssertTrue([ringBuffer loadFloatData:&(inData[0])
                                 dataSize:inDataSize
                                   offset:0
                                     size:inDataSize
                                    error:nil]);

  // State after load: [0.0, 0.0, 1.0, 2.0, 3.0]

  TFLFloatBuffer *outBuffer = ringBuffer.floatBuffer;
  XCTAssertNotNil(outBuffer);
  XCTAssertEqual(outBuffer.size, bufferSize);

  // Expected state after loading most recent elements of source buffer.
  float expectedData[] = {0.0f, 0.0f, 1.0f, 2.0f, 3.0f};

  for (int i = 0; i < bufferSize; i++) {
    XCTAssertEqual(outBuffer.data[i], expectedData[i]);
  }
}

- (void)testLoadSucceedsByShiftingOutOldElements {
  NSInteger initialDataSize = 4;
  float initialArray[] = {1.0f, 2.0f, 3.0f, 4.0f};

  NSInteger bufferSize = 5;
  TFLRingBuffer *ringBuffer = [[TFLRingBuffer alloc] initWithBufferSize:bufferSize];

  XCTAssertTrue([ringBuffer loadFloatData:&(initialArray[0])
                                 dataSize:initialDataSize
                                   offset:0
                                     size:initialDataSize
                                    error:nil]);

  // State after load: [0.0, 1.0, 2.0, 3.0, 4.0]

  NSInteger inDataSize = 3;
  float inArray[] = {5, 6, 7};

  XCTAssertTrue([ringBuffer loadFloatData:&(inArray[0])
                                 dataSize:inDataSize
                                   offset:0
                                     size:inDataSize
                                    error:nil]);

  TFLFloatBuffer *outBuffer = ringBuffer.floatBuffer;
  XCTAssertNotNil(outBuffer);
  XCTAssertEqual(outBuffer.size, bufferSize);

  // Expected state after loading most recent elements of source buffer.
  float expectedData[] = {3.0f, 4.0f, 5.0f, 6.0f, 7.0f};

  for (int i = 0; i < bufferSize; i++) {
    XCTAssertEqual(outBuffer.data[i], expectedData[i]);
  }
}

- (void)testLoadSucceedsWithMostRecentElements {
  NSInteger initialDataSize = 5;
  float initialArray[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

  NSInteger bufferSize = 5;
  TFLRingBuffer *ringBuffer = [[TFLRingBuffer alloc] initWithBufferSize:bufferSize];

  XCTAssertTrue([ringBuffer loadFloatData:&(initialArray[0])
                                 dataSize:initialDataSize
                                   offset:0
                                     size:initialDataSize
                                    error:nil]);

  // State after load: [1.0, 2.0, 3.0, 4.0, 5.0]

  NSInteger sourceDataSize = 6;
  float sourceArray[] = {6, 7, 8, 9, 10, 11};
  XCTAssertTrue([ringBuffer loadFloatData:&(sourceArray[0])
                                 dataSize:sourceDataSize
                                   offset:0
                                     size:sourceDataSize
                                    error:nil]);

  TFLFloatBuffer *outBuffer = ringBuffer.floatBuffer;
  XCTAssertNotNil(outBuffer);
  XCTAssertEqual(outBuffer.size, bufferSize);

  // Expected state after loading most recent elements of source buffer.
  float expectedData[] = {7.0f, 8.0f, 9.0f, 10.0f, 11.0f};

  for (int i = 0; i < bufferSize; i++) {
    XCTAssertEqual(outBuffer.data[i], expectedData[i]);
  }
}

- (void)testLoadSucceedsWithOffseAndMostRecentElements {
  NSInteger initialDataSize = 5;
  float initialArray[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

  NSInteger bufferSize = 5;
  TFLRingBuffer *ringBuffer = [[TFLRingBuffer alloc] initWithBufferSize:bufferSize];

  XCTAssertTrue([ringBuffer loadFloatData:&(initialArray[0])
                                 dataSize:initialDataSize
                                   offset:0
                                     size:initialDataSize
                                    error:nil]);

  // State after load: [1.0, 2.0, 3.0, 4.0, 5.0]

  NSInteger totalInSize = 8;
  float inArray[] = {6, 7, 8, 9, 10, 11, 12, 13};

  NSInteger offset = 2;
  NSInteger inDataSize = 6;
  XCTAssertTrue([ringBuffer loadFloatData:&(inArray[0])
                                 dataSize:totalInSize
                                   offset:offset
                                     size:inDataSize
                                    error:nil]);

  TFLFloatBuffer *outBuffer = ringBuffer.floatBuffer;
  XCTAssertNotNil(outBuffer);
  XCTAssertEqual(outBuffer.size, bufferSize);

  // Expected state after load with most recent elements and offset.
  float expectedData[] = {9.0f, 10.0f, 11.0f, 12.0f, 13.0f};

  for (int i = 0; i < bufferSize; i++) {
    XCTAssertEqual(outBuffer.data[i], expectedData[i]);
  }
}

- (void)testLoadSucceedsWithOffset {
  NSInteger initialDataSize = 2;
  float initialArray[] = {1.0f, 2.0f};

  NSInteger bufferSize = 5;
  TFLRingBuffer *ringBuffer = [[TFLRingBuffer alloc] initWithBufferSize:bufferSize];

  XCTAssertTrue([ringBuffer loadFloatData:&(initialArray[0])
                                 dataSize:initialDataSize
                                   offset:0
                                     size:initialDataSize
                                    error:nil]);

  // State after load: [0.0, 0.0, 0.0, 1.0, 2.0]

  NSInteger totalInSize = 4;
  float inArray[] = {6.0f, 7.0f, 8.0f, 9.0f};

  NSInteger offset = 2;
  NSInteger inDataSize = 2;
  XCTAssertTrue([ringBuffer loadFloatData:&(inArray[0])
                                 dataSize:totalInSize
                                   offset:offset
                                     size:inDataSize
                                    error:nil]);

  TFLFloatBuffer *outBuffer = ringBuffer.floatBuffer;
  XCTAssertNotNil(outBuffer);
  XCTAssertEqual(outBuffer.size, bufferSize);

  // State after load with offset
  float expectedData[] = {0.0f, 1.0f, 2.0f, 8.0f, 9.0f};

  for (int i = 0; i < bufferSize; i++) {
    XCTAssertEqual(outBuffer.data[i], expectedData[i]);
  }
}

- (void)testLoadFailsWithIndexOutofBounds {
  NSInteger initialDataSize = 2;
  float initialArray[] = {1.0f, 2.0f};

  NSInteger bufferSize = 5;
  TFLRingBuffer *ringBuffer = [[TFLRingBuffer alloc] initWithBufferSize:bufferSize];

  XCTAssertTrue([ringBuffer loadFloatData:&(initialArray[0])
                                 dataSize:initialDataSize
                                   offset:0
                                     size:initialDataSize
                                    error:nil]);

  float inArray[] = {6.0f, 7.0f, 8.0f, 9.0f};

  NSInteger offset = 2;
  NSInteger inDataSize = 3;

  NSError *error = nil;
  XCTAssertFalse([ringBuffer loadFloatData:&(inArray[0])
                                  dataSize:initialDataSize
                                    offset:offset
                                      size:inDataSize
                                     error:&error]);

  XCTAssertNotNil(error);
  VerifyError(error, @"org.tensorflow.lite.tasks", TFLSupportErrorCodeInvalidArgumentError,
              @"offset + size exceeds the maximum size of the source buffer.");
}

- (void)testClearSucceeds {
  NSInteger initialDataSize = 2;
  float initialArray[] = {1.0f, 2.0f};

  NSInteger bufferSize = 5;
  TFLRingBuffer *ringBuffer = [[TFLRingBuffer alloc] initWithBufferSize:bufferSize];

  XCTAssertTrue([ringBuffer loadFloatData:&(initialArray[0])
                                 dataSize:initialDataSize
                                   offset:0
                                     size:initialDataSize
                                    error:nil]);

  [ringBuffer clear];

  float expectedData[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

  TFLFloatBuffer *outBuffer = ringBuffer.floatBuffer;
  XCTAssertNotNil(outBuffer);
  XCTAssertEqual(outBuffer.size, bufferSize);

  for (int i = 0; i < bufferSize; i++) {
    XCTAssertEqual(outBuffer.data[i], expectedData[i]);
  }
}

@end

NS_ASSUME_NONNULL_END
