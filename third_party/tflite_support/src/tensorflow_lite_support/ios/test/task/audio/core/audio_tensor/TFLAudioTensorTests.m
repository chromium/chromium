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
#import "tensorflow_lite_support/ios/task/audio/core/audio_tensor/sources/TFLAudioTensor.h"
#import "tensorflow_lite_support/ios/test/task/audio/core/audio_record/utils/sources/AVAudioPCMBuffer+Utils.h"

#define VerifyError(error, expectedErrorDomain, expectedErrorCode, expectedLocalizedDescription) \
  XCTAssertEqualObjects(error.domain, expectedErrorDomain);                                      \
  XCTAssertEqual(error.code, expectedErrorCode);                                                 \
  XCTAssertEqualObjects(error.localizedDescription, expectedLocalizedDescription);
#define BUFFER_SIZE 100

NS_ASSUME_NONNULL_BEGIN

@interface TFLAudioTensorTests : XCTestCase
@property(nonatomic) AVAudioFormat *audioEngineFormat;
@end

// This category of TFLAudioRecord is private to the current test file.
// This is needed in order to expose the method to load the audio record buffer
// without calling: -[TFLAudioRecord startRecordingWithError:]. This will
// be used for testing -[TFLAudioTensor loadAudioRecord:withError:].
// This is needed to avoid exposing this method which isn't useful to the consumers
// of the framework.
@interface TFLAudioRecord (Tests)
- (void)convertAndLoadBuffer:(AVAudioPCMBuffer *)buffer
         usingAudioConverter:(AVAudioConverter *)audioConverter;
@end

@implementation TFLAudioTensorTests

- (void)setUp {
  [super setUp];
  self.audioEngineFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                            sampleRate:48000
                                                              channels:1
                                                           interleaved:NO];
}

- (AVAudioPCMBuffer *)audioEngineBufferFromFileWithName:(NSString *)name
                                              extension:(NSString *)extension {
  // Loading AVAudioPCMBuffer with an array is not currently supported for iOS versions < 15.0.
  // Instead audio samples from a wav file are loaded and converted into the same format
  // of AVAudioEngine's input node to mock thhe input from the AVAudio Engine.
  NSString *filePath = [[NSBundle bundleForClass:self.class] pathForResource:name ofType:extension];

  return [AVAudioPCMBuffer loadPCMBufferFromFileWithPath:filePath
                                        processingFormat:self.audioEngineFormat];
}

- (TFLAudioFormat *)createAudioFormatWithSampleRate:(NSUInteger)sampleRate {
  return [[TFLAudioFormat alloc] initWithChannelCount:1 sampleRate:sampleRate];
}

- (TFLAudioRecord *)createAudioRecordWithAudioFormat:(TFLAudioFormat *)audioFormat {
  NSUInteger bufferSize = 100;
  TFLAudioRecord *audioRecord = [[TFLAudioRecord alloc] initWithAudioFormat:audioFormat
                                                                 bufferSize:bufferSize
                                                                      error:nil];
  XCTAssertNotNil(audioRecord);
  return audioRecord;
}

- (void)validateTensorWithAudioFormat:(TFLAudioFormat *)audioFormat
                          sampleCount:(NSUInteger)sampleCount {
  TFLAudioRecord *audioRecord = [self createAudioRecordWithAudioFormat:audioFormat];

  AVAudioFormat *recordingFormat =
      [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                       sampleRate:audioFormat.sampleRate
                                         channels:(AVAudioChannelCount)audioFormat.channelCount
                                      interleaved:YES];

  AVAudioConverter *audioConverter = [[AVAudioConverter alloc] initFromFormat:self.audioEngineFormat
                                                                     toFormat:recordingFormat];

  // Loading AVAudioPCMBuffer with an array is not currently supported for iOS versions < 15.0.
  // Instead audio samples from a wav file are loaded and converted into the same format
  // of AVAudioEngine's input node to mock thhe input from the AVAudio Engine.
  AVAudioPCMBuffer *audioEngineBuffer = [self audioEngineBufferFromFileWithName:@"speech"
                                                                      extension:@"wav"];
  XCTAssertNotNil(audioEngineBuffer);

  // Convert and load the buffer of `TFLAudioRecord`.
  [audioRecord convertAndLoadBuffer:audioEngineBuffer usingAudioConverter:audioConverter];

  // Initialize audio tensor with the same audio format as the audio record.
  TFLAudioTensor *audioTensor = [[TFLAudioTensor alloc] initWithAudioFormat:audioFormat
                                                                sampleCount:sampleCount];
  [audioTensor loadAudioRecord:audioRecord withError:nil];

  // Get the current buffer of audio tensor.
  TFLFloatBuffer *audioTensorBuffer = audioTensor.buffer;
  XCTAssertNotNil(audioTensorBuffer);

  // Get the buffer to compare the audio tensor buffer to. Here we convert the buffer which mimics
  // the buffer output by audio engine to a buffer of format similar to audio tensor and audio
  // record.
  AVAudioPCMBuffer *bufferToCompare = [audioEngineBuffer bufferUsingAudioConverter:audioConverter];
  XCTAssertNotNil(bufferToCompare);

  XCTAssertEqual(audioTensorBuffer.size, sampleCount);
  for (int i = 0; i < audioTensorBuffer.size; i++) {
    NSInteger startIndex = bufferToCompare.frameLength - audioTensorBuffer.size;
    XCTAssertEqual(audioTensorBuffer.data[i], bufferToCompare.floatChannelData[0][startIndex + i]);
  }
}

- (void)testInitWithMultipleChannelsSucceeds {
  TFLAudioFormat *audioFormat = [[TFLAudioFormat alloc] initWithChannelCount:2 sampleRate:8000];

  NSUInteger sampleCount = BUFFER_SIZE;
  TFLAudioTensor *audioTensor = [[TFLAudioTensor alloc] initWithAudioFormat:audioFormat
                                                                sampleCount:sampleCount];
  XCTAssertEqual(audioTensor.bufferSize, sampleCount * audioFormat.channelCount);
}

- (void)testLoadFullBufferFromAudioRecordSucceeds {
  TFLAudioFormat *audioFormat = [self createAudioFormatWithSampleRate:8000];
  [self validateTensorWithAudioFormat:audioFormat sampleCount:BUFFER_SIZE];
}

- (void)testLoadRecentElementsFromAudioRecordSucceeds {
  TFLAudioFormat *audioFormat = [self createAudioFormatWithSampleRate:8000];
  NSInteger sampleCount = 45;
  [self validateTensorWithAudioFormat:audioFormat sampleCount:sampleCount];
}

- (void)testLoadAudioRecordFailsWithUnequalAudioFormats {
  TFLAudioFormat *audioFormat = [self createAudioFormatWithSampleRate:8000];
  TFLAudioRecord *audioRecord = [self createAudioRecordWithAudioFormat:audioFormat];

  // Loading AVAudioPCMBuffer with an array is not currentlyy supported for iOS versions < 15.0.
  // Instead audio samples from a wav file are loaded and converted into the same format
  // of AVAudioEngine's input node to mock thhe input from the AVAudio Engine.
  AVAudioPCMBuffer *audioEngineBuffer = [self audioEngineBufferFromFileWithName:@"speech"
                                                                      extension:@"wav"];
  XCTAssertNotNil(audioEngineBuffer);

  AVAudioFormat *recordingFormat =
      [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                       sampleRate:audioFormat.sampleRate
                                         channels:(AVAudioChannelCount)audioFormat.channelCount
                                      interleaved:YES];

  AVAudioFormat *audioEngineFormat =
      [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                       sampleRate:48000
                                         channels:1
                                      interleaved:NO];

  AVAudioConverter *audioConverter = [[AVAudioConverter alloc] initFromFormat:audioEngineFormat
                                                                     toFormat:recordingFormat];

  // Convert and load audio buffer using audio engine.
  [audioRecord convertAndLoadBuffer:audioEngineBuffer usingAudioConverter:audioConverter];

  TFLAudioFormat *audioTensorFormat = [[TFLAudioFormat alloc] initWithChannelCount:2
                                                                        sampleRate:16000];

  TFLAudioTensor *audioTensor = [[TFLAudioTensor alloc] initWithAudioFormat:audioTensorFormat
                                                                sampleCount:BUFFER_SIZE];

  NSError *error = nil;
  XCTAssertFalse([audioTensor loadAudioRecord:audioRecord withError:&error]);
  XCTAssertNotNil(error);

  VerifyError(
      error, @"org.tensorflow.lite.tasks", TFLSupportErrorCodeInvalidArgumentError,
      @"Audio format of TFLAudioRecord does not match the audio format of Tensor Audio. Please "
      @"ensure that the channelCount and sampleRate of both audio formats are equal.");
}

- (void)testLoadBufferSucceeds {
  TFLAudioFormat *audioFormat = [self createAudioFormatWithSampleRate:8000];
  NSInteger sampleCount = 5;

  TFLAudioTensor *audioTensor = [[TFLAudioTensor alloc] initWithAudioFormat:audioFormat
                                                                sampleCount:sampleCount];

  float inData[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
  TFLFloatBuffer *floatBuffer = [[TFLFloatBuffer alloc] initWithData:&(inData[0]) size:sampleCount];

  NSError *error = nil;
  XCTAssertTrue([audioTensor loadBuffer:floatBuffer offset:0 size:sampleCount error:&error]);
  XCTAssertNil(error);

  // State after load: [1.0, 2.0, 3.0, 4.0, 5.0]

  TFLFloatBuffer *outBuffer = audioTensor.buffer;
  XCTAssertNotNil(outBuffer);
  XCTAssertEqual(outBuffer.size, sampleCount);

  float expectedData[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

  for (int i = 0; i < sampleCount; i++) {
    XCTAssertEqual(outBuffer.data[i], expectedData[i]);
  }
}

- (void)testLoadBufferFailsWithIndexOutOfBounds {
  TFLAudioFormat *audioFormat = [self createAudioFormatWithSampleRate:8000];
  NSInteger sampleCount = 5;

  TFLAudioTensor *audioTensor = [[TFLAudioTensor alloc] initWithAudioFormat:audioFormat
                                                                sampleCount:sampleCount];

  NSInteger totalInArraySize = 4;
  float inArray[] = {6.0f, 7.0f, 8.0f, 9.0f};

  TFLFloatBuffer *floatBuffer =
      [[TFLFloatBuffer alloc] initWithData:&(inArray[0]) size:totalInArraySize];

  NSInteger offset = 2;
  NSInteger loadDataSize = 3;

  NSError *error = nil;
  XCTAssertFalse([audioTensor loadBuffer:floatBuffer offset:offset size:loadDataSize error:&error]);

  XCTAssertNotNil(error);
  VerifyError(error, @"org.tensorflow.lite.tasks", TFLSupportErrorCodeInvalidArgumentError,
              @"offset + size exceeds the maximum size of the source buffer.");
}

@end

NS_ASSUME_NONNULL_END
