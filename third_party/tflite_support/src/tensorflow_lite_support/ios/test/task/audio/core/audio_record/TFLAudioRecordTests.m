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

#import <AVFoundation/AVFoundation.h>
#import <XCTest/XCTest.h>

#import "tensorflow_lite_support/ios/sources/TFLCommon.h"
#import "tensorflow_lite_support/ios/test/task/audio/core/audio_record/utils/sources/AVAudioPCMBuffer+Utils.h"

#import "tensorflow_lite_support/ios/task/audio/core/audio_record/sources/TFLAudioRecord.h"

#define VerifyError(error, expectedErrorDomain, expectedErrorCode, expectedLocalizedDescription) \
  XCTAssertEqualObjects(error.domain, expectedErrorDomain);                                      \
  XCTAssertEqual(error.code, expectedErrorCode);                                                 \
  XCTAssertEqualObjects(error.localizedDescription, expectedLocalizedDescription);

NS_ASSUME_NONNULL_BEGIN

@interface TFLAudioRecordTests : XCTestCase
@property(nonatomic) AVAudioFormat *audioEngineFormat;
@end

// This category of TFLAudioRecord is private to the current test file.
// This is needed in order to expose the method to load the audio record buffer
// without calling: -[TFLAudioRecord startRecordingWithError:].
// This is needed to avoid exposing this method which isn't useful to the consumers
// of the framework.
@interface TFLAudioRecord (Tests)
- (void)convertAndLoadBuffer:(AVAudioPCMBuffer *)buffer
         usingAudioConverter:(AVAudioConverter *)audioConverter;
@end

@implementation TFLAudioRecordTests

- (void)setUp {
  [super setUp];
  self.audioEngineFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                            sampleRate:48000
                                                              channels:1
                                                           interleaved:NO];
}

- (AVAudioPCMBuffer *)audioEngineFromFileWithName:(NSString *)name extension:(NSString *)extension {
  // Loading AVAudioPCMBuffer with an array is not currently suupported for iOS versions < 15.0.
  // Instead audio samples from a wav file are loaded and converted into the same format
  // of AVAudioEngine's input node to mock thhe input from the AVAudio Engine.
  NSString *filePath = [[NSBundle bundleForClass:self.class] pathForResource:name ofType:extension];

  return [AVAudioPCMBuffer loadPCMBufferFromFileWithPath:filePath
                                        processingFormat:self.audioEngineFormat];
}

- (void)testInitAudioRecordFailsWithInvalidChannelCount {
  TFLAudioFormat *audioFormat = [[TFLAudioFormat alloc] initWithChannelCount:3 sampleRate:8000];

  NSError *error = nil;
  TFLAudioRecord *audioRecord = [[TFLAudioRecord alloc] initWithAudioFormat:audioFormat
                                                                 bufferSize:100
                                                                      error:&error];
  XCTAssertNil(audioRecord);

  XCTAssertNotNil(error);
  VerifyError(error, @"org.tensorflow.lite.audio.record",
              TFLAudioRecordErrorCodeInvalidArgumentError,
              @"The channel count provided does not match the supported "
              @"channel count. Only up to 2 audio channels are currently supported.");
}

- (void)testInitBufferFailsWithInvalidBufferSize {
  TFLAudioFormat *audioFormat = [[TFLAudioFormat alloc] initWithChannelCount:2 sampleRate:8000];

  NSError *error = nil;
  TFLAudioRecord *audioRecord = [[TFLAudioRecord alloc] initWithAudioFormat:audioFormat
                                                                 bufferSize:101
                                                                      error:&error];
  XCTAssertNil(audioRecord);

  VerifyError(error, @"org.tensorflow.lite.audio.record",
              TFLAudioRecordErrorCodeInvalidArgumentError,
              @"The buffer size provided is not a multiple of channel count.");
}

- (void)testConvertAndLoadBufferSucceeds {
  TFLAudioFormat *audioFormat = [[TFLAudioFormat alloc] initWithChannelCount:1 sampleRate:8000];
  TFLAudioRecord *audioRecord = [[TFLAudioRecord alloc] initWithAudioFormat:audioFormat
                                                                 bufferSize:100
                                                                      error:nil];
  XCTAssertNotNil(audioRecord);

  // Loading AVAudioPCMBuffer with an array is not currentlyy supported for iOS versions < 15.0.
  // Instead audio samples from a wav file are loaded and converted into the same format
  // of AVAudioEngine's input node to mock thhe input from the AVAudio Engine.
  AVAudioPCMBuffer *audioEngineBuffer = [self audioEngineFromFileWithName:@"speech"
                                                                extension:@"wav"];
  XCTAssertNotNil(audioEngineBuffer);

  AVAudioFormat *recordingFormat =
      [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                       sampleRate:audioFormat.sampleRate
                                         channels:(AVAudioChannelCount)audioFormat.channelCount
                                      interleaved:YES];

  AVAudioConverter *audioConverter = [[AVAudioConverter alloc] initFromFormat:self.audioEngineFormat
                                                                     toFormat:recordingFormat];

  // Convert and load audio buffer using audio engine.
  [audioRecord convertAndLoadBuffer:audioEngineBuffer usingAudioConverter:audioConverter];

  // To compare the result of readAtOffset:, we produce a bufferToCompare by converting
  // audioEngineBuffer using by the same logic that coonverts the buffer to the required format in
  // the `TFLAudioRecord`. Since the conversion is performed by the native `AVAudioConverter`, we
  // don't have to test if the conversion itself produces valid samples. We simply compare the
  // resulting buffer to the buffer that is read from `TFLAudioRecord`.
  AVAudioPCMBuffer *bufferToCompare = [audioEngineBuffer bufferUsingAudioConverter:audioConverter];
  XCTAssertNotNil(bufferToCompare);

  NSError *error = nil;
  TFLFloatBuffer *floatBuffer = [audioRecord readAtOffset:0
                                                 withSize:audioRecord.bufferSize
                                                    error:&error];

  XCTAssertNotNil(floatBuffer);
  XCTAssertEqual(floatBuffer.size, audioRecord.bufferSize);
  for (int i = 0; i < floatBuffer.size; i++) {
    NSInteger startIndex = bufferToCompare.frameLength - floatBuffer.size;
    XCTAssertEqual(floatBuffer.data[i], bufferToCompare.floatChannelData[0][startIndex + i]);
  }
}

- (void)testConvertAndLoadBufferSucceedsWithTwoChannels {
  TFLAudioFormat *audioFormat = [[TFLAudioFormat alloc] initWithChannelCount:2 sampleRate:8000];
  TFLAudioRecord *audioRecord = [[TFLAudioRecord alloc] initWithAudioFormat:audioFormat
                                                                 bufferSize:100
                                                                      error:nil];
  XCTAssertNotNil(audioRecord);

  AVAudioPCMBuffer *audioEngineBuffer = [self audioEngineFromFileWithName:@"speech"
                                                                extension:@"wav"];
  XCTAssertNotNil(audioEngineBuffer);

  AVAudioFormat *recordingFormat =
      [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                       sampleRate:audioFormat.sampleRate
                                         channels:(AVAudioChannelCount)audioFormat.channelCount
                                      interleaved:YES];

  AVAudioConverter *audioConverter = [[AVAudioConverter alloc] initFromFormat:self.audioEngineFormat
                                                                     toFormat:recordingFormat];

  [audioRecord convertAndLoadBuffer:audioEngineBuffer usingAudioConverter:audioConverter];

  AVAudioPCMBuffer *bufferToCompare = [audioEngineBuffer bufferUsingAudioConverter:audioConverter];
  XCTAssertNotNil(bufferToCompare);

  NSError *error = nil;
  TFLFloatBuffer *floatBuffer = [audioRecord readAtOffset:0
                                                 withSize:audioRecord.bufferSize
                                                    error:&error];

  XCTAssertNotNil(floatBuffer);
  XCTAssertEqual(floatBuffer.size, audioRecord.bufferSize);

  // audioConverter will produce bufferToCompare with floatChannelData[0] in an interleaved format.
  // Hence bufferToCompare can be compared to the result of `readAtOffset` directly without
  // verifying is there is sample duplication.
  for (int i = 0; i < floatBuffer.size; i++) {
    NSInteger startIndex = bufferToCompare.frameLength - floatBuffer.size;
    XCTAssertEqual(floatBuffer.data[i], bufferToCompare.floatChannelData[0][startIndex + i]);
  }
}

- (void)testReadFailsWithoutLoad {
  TFLAudioFormat *audioFormat = [[TFLAudioFormat alloc] initWithChannelCount:1 sampleRate:8000];

  TFLAudioRecord *audioRecord = [[TFLAudioRecord alloc] initWithAudioFormat:audioFormat
                                                                 bufferSize:100
                                                                      error:nil];
  XCTAssertNotNil(audioRecord);

  NSError *error = nil;
  [audioRecord readAtOffset:0 withSize:audioRecord.bufferSize error:&error];

  VerifyError(error, @"org.tensorflow.lite.audio.record",
              TFLAudioRecordErrorCodeWaitingForNewMicInputError,
              @"TFLAudioRecord hasn't started receiving samples from the audio "
              @"input source. Please wait for the input.");
}

- (void)testReadSucceedsWithOffset {
  TFLAudioFormat *audioFormat = [[TFLAudioFormat alloc] initWithChannelCount:1 sampleRate:8000];
  TFLAudioRecord *audioRecord = [[TFLAudioRecord alloc] initWithAudioFormat:audioFormat
                                                                 bufferSize:100
                                                                      error:nil];
  XCTAssertNotNil(audioRecord);

  AVAudioPCMBuffer *audioEngineBuffer = [self audioEngineFromFileWithName:@"speech"
                                                                extension:@"wav"];
  XCTAssertNotNil(audioEngineBuffer);

  AVAudioFormat *recordingFormat =
      [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                       sampleRate:audioFormat.sampleRate
                                         channels:(AVAudioChannelCount)audioFormat.channelCount
                                      interleaved:YES];

  AVAudioConverter *audioConverter = [[AVAudioConverter alloc] initFromFormat:self.audioEngineFormat
                                                                     toFormat:recordingFormat];

  [audioRecord convertAndLoadBuffer:audioEngineBuffer usingAudioConverter:audioConverter];

  AVAudioPCMBuffer *bufferToCompare = [audioEngineBuffer bufferUsingAudioConverter:audioConverter];
  XCTAssertNotNil(bufferToCompare);

  const NSInteger offset = 10;
  const NSInteger size = audioRecord.bufferSize - 15;

  NSError *error = nil;
  TFLFloatBuffer *floatBuffer = [audioRecord readAtOffset:offset withSize:size error:&error];
  XCTAssertNotNil(floatBuffer);
  XCTAssertNil(error);

  XCTAssertEqual(floatBuffer.size, size);

  for (int i = 0; i < floatBuffer.size; i++) {
    NSInteger startIndex = bufferToCompare.frameLength - audioRecord.bufferSize + offset;
    XCTAssertEqual(floatBuffer.data[i], bufferToCompare.floatChannelData[0][startIndex + i]);
  }
}

- (void)testReadFailsWithIndexOutOfBounds {
  TFLAudioFormat *audioFormat = [[TFLAudioFormat alloc] initWithChannelCount:1 sampleRate:8000];
  TFLAudioRecord *audioRecord = [[TFLAudioRecord alloc] initWithAudioFormat:audioFormat
                                                                 bufferSize:100
                                                                      error:nil];
  XCTAssertNotNil(audioRecord);

  AVAudioPCMBuffer *audioEngineBuffer = [self audioEngineFromFileWithName:@"speech"
                                                                extension:@"wav"];
  XCTAssertNotNil(audioEngineBuffer);

  AVAudioFormat *recordingFormat =
      [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                       sampleRate:audioFormat.sampleRate
                                         channels:(AVAudioChannelCount)audioFormat.channelCount
                                      interleaved:YES];

  AVAudioConverter *audioConverter = [[AVAudioConverter alloc] initFromFormat:self.audioEngineFormat
                                                                     toFormat:recordingFormat];

  [audioRecord convertAndLoadBuffer:audioEngineBuffer usingAudioConverter:audioConverter];

  AVAudioPCMBuffer *bufferToCompare = [audioEngineBuffer bufferUsingAudioConverter:audioConverter];
  XCTAssertNotNil(bufferToCompare);

  NSError *error = nil;

  const NSInteger offset = 10;

  TFLFloatBuffer *floatBuffer = [audioRecord readAtOffset:offset
                                                 withSize:audioRecord.bufferSize
                                                    error:&error];
  XCTAssertNil(floatBuffer);

  VerifyError(error, @"org.tensorflow.lite.audio.record",
              TFLAudioRecordErrorCodeInvalidArgumentError,
              @"Index out of bounds: offset + size should be <= to the size of "
              @"TFLAudioRecord's internal buffer.");
}

@end

NS_ASSUME_NONNULL_END
