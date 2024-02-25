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
#import <XCTest/XCTest.h>

#import "tensorflow_lite_support/ios/sources/TFLCommon.h"
#import "tensorflow_lite_support/ios/task/audio/sources/TFLAudioClassifier.h"
#import "tensorflow_lite_support/ios/test/task/audio/core/audio_record/utils/sources/AVAudioPCMBuffer+Utils.h"

#define VerifyError(error, expectedDomain, expectedCode, expectedLocalizedDescription)  \
  XCTAssertNotNil(error);                                                               \
  XCTAssertEqualObjects(error.domain, expectedDomain);                                  \
  XCTAssertEqual(error.code, expectedCode);                                             \
  XCTAssertNotEqual(                                                                    \
      [error.localizedDescription rangeOfString:expectedLocalizedDescription].location, \
      NSNotFound)

#define VerifyCategory(category, expectedIndex, expectedScore, expectedLabel, expectedDisplayName) \
  XCTAssertEqual(category.index, expectedIndex);                                                   \
  XCTAssertEqualWithAccuracy(category.score, expectedScore, 1e-6);                                 \
  XCTAssertEqualObjects(category.label, expectedLabel);                                            \
  XCTAssertEqualObjects(category.displayName, expectedDisplayName);

#define VerifyClassifications(classifications, expectedHeadIndex, expectedCategoryCount) \
  XCTAssertEqual(classifications.categories.count, expectedCategoryCount);               \
  XCTAssertEqual(classifications.headIndex, expectedHeadIndex)

#define VerifyClassificationResult(classificationResult, expectedClassificationsCount) \
  XCTAssertNotNil(classificationResult);                                               \
  XCTAssertEqual(classificationResult.classifications.count, expectedClassificationsCount)

static NSString *const expectedTaskErrorDomain = @"org.tensorflow.lite.tasks";

NS_ASSUME_NONNULL_BEGIN

@interface TFLAudioClassifierTests : XCTestCase
@property(nonatomic, nullable) NSString *modelPath;
@property(nonatomic) AVAudioFormat *audioEngineFormat;
@end

// This category of TFLAudioRecord is private to the test files. This is needed in order to
// expose the method to load the audio record buffer without calling: -[TFLAudioRecord
// startRecordingWithError:]. This is needed to avoid exposing this method which isn't useful to the
// consumers of the framework.
@interface TFLAudioRecord (Tests)
- (void)convertAndLoadBuffer:(AVAudioPCMBuffer *)buffer
         usingAudioConverter:(AVAudioConverter *)audioConverter;
@end

@implementation TFLAudioClassifierTests

- (void)setUp {
  // Put setup code here. This method is called before the invocation of each test method in the
  // class.
  [super setUp];
  self.modelPath =
      [[NSBundle bundleForClass:self.class] pathForResource:@"yamnet_audio_classifier_with_metadata"
                                                     ofType:@"tflite"];
  XCTAssertNotNil(self.modelPath);

  self.audioEngineFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                            sampleRate:48000
                                                              channels:1
                                                           interleaved:NO];
}

- (nullable AVAudioPCMBuffer *)bufferFromFileWithName:(NSString *)name
                                            extension:(NSString *)extension
                                          audioFormat:(TFLAudioFormat *)audioFormat {
  NSString *filePath = [[NSBundle bundleForClass:self.class] pathForResource:name ofType:extension];
  return [AVAudioPCMBuffer loadPCMBufferFromFileWithPath:filePath audioFormat:audioFormat];
}

- (nullable AVAudioPCMBuffer *)bufferFromFileWithName:(NSString *)name
                                            extension:(NSString *)extension
                                     processingFormat:(AVAudioFormat *)processingFormat {
  NSString *filePath = [[NSBundle bundleForClass:self.class] pathForResource:name ofType:extension];
  return [AVAudioPCMBuffer loadPCMBufferFromFileWithPath:filePath
                                        processingFormat:processingFormat];
}

- (void)mockLoadBufferOfAudioRecord:(TFLAudioRecord *)audioRecord {
  // Loading AVAudioPCMBuffer with an array is not currently supported for iOS versions < 15.0.
  // Instead audio samples from a wav file are loaded and converted into the same format
  // of AVAudioEngine's input node to mock the input from the AVAudio Engine.
  AVAudioPCMBuffer *audioEngineBuffer = [self bufferFromFileWithName:@"speech"
                                                           extension:@"wav"
                                                    processingFormat:self.audioEngineFormat];
  XCTAssertNotNil(audioEngineBuffer);

  // Convert the buffer in the audio engine input format to the format with which audio record is
  // intended to output the audio samples. This mocks the internal conversion of audio record when
  // -[TFLAudioRecord startRecording:withError:] is called.
  AVAudioFormat *recordingFormat = [[AVAudioFormat alloc]
      initWithCommonFormat:AVAudioPCMFormatFloat32
                sampleRate:audioRecord.audioFormat.sampleRate
                  channels:(AVAudioChannelCount)audioRecord.audioFormat.channelCount
               interleaved:YES];

  AVAudioConverter *audioConverter = [[AVAudioConverter alloc] initFromFormat:self.audioEngineFormat
                                                                     toFormat:recordingFormat];
  // Convert and load the buffer of `TFLAudioRecord`.
  [audioRecord convertAndLoadBuffer:audioEngineBuffer usingAudioConverter:audioConverter];
}

- (TFLAudioClassifier *)createAudioClassifierWithModelPath:(NSString *)modelPath {
  TFLAudioClassifierOptions *options =
      [[TFLAudioClassifierOptions alloc] initWithModelPath:self.modelPath];
  TFLAudioClassifier *audioClassifier = [TFLAudioClassifier audioClassifierWithOptions:options
                                                                                 error:nil];
  XCTAssertNotNil(audioClassifier);

  return audioClassifier;
}

- (TFLAudioClassifier *)createAudioClassifierWithOptions:(TFLAudioClassifierOptions *)options {
  TFLAudioClassifier *audioClassifier = [TFLAudioClassifier audioClassifierWithOptions:options
                                                                                 error:nil];
  XCTAssertNotNil(audioClassifier);

  return audioClassifier;
}

- (TFLAudioTensor *)createAudioTensorWithAudioClassifier:(TFLAudioClassifier *)audioClassifier {
  // Create the audio tensor using audio classifier.
  TFLAudioTensor *audioTensor = [audioClassifier createInputAudioTensor];
  XCTAssertNotNil(audioTensor);

  return audioTensor;
}

- (void)loadAudioTensor:(TFLAudioTensor *)audioTensor fromWavFileWithName:(NSString *)fileName {
  // Load pcm buffer from file.
  AVAudioPCMBuffer *buffer = [self bufferFromFileWithName:fileName
                                                extension:@"wav"
                                              audioFormat:audioTensor.audioFormat];

  // Get float buffer from pcm buffer.
  TFLFloatBuffer *floatBuffer = buffer.floatBuffer;
  XCTAssertNotNil(floatBuffer);

  // Load float buffer into the audio tensor.
  [audioTensor loadBuffer:floatBuffer offset:0 size:floatBuffer.size error:nil];
}

- (TFLClassificationResult *)classifyWithAudioClassifier:(TFLAudioClassifier *)audioClassifier
                                             audioTensor:(TFLAudioTensor *)audioTensor
                                   expectedCategoryCount:(const NSInteger)expectedCategoryCount {
  TFLClassificationResult *classificationResult =
      [audioClassifier classifyWithAudioTensor:audioTensor error:nil];

  const NSInteger expectedClassificationsCount = 1;
  VerifyClassificationResult(classificationResult, expectedClassificationsCount);

  const NSInteger expectedHeadIndex = 0;
  VerifyClassifications(classificationResult.classifications[0], expectedHeadIndex,
                        expectedCategoryCount);

  return classificationResult;
}

- (void)validateClassificationResultForInferenceWithFloatBuffer:
    (NSArray<TFLCategory *> *)categories {
  VerifyCategory(categories[0],
                 0,          // expectedIndex
                 0.957031,   // expectedScore
                 @"Speech",  // expectedLabel
                 nil         // expectedDisplaName
  );
  VerifyCategory(categories[1],
                 500,  // expectedIndex
                                         0.019531,               // expectedScore
                 @"Inside, small room",  // expectedLabel
                 nil                     // expectedDisplaName
  );
}

- (void)validateCategoriesForInferenceWithAudioRecord:(NSArray<TFLCategory *> *)categories {
  // The third category is different from the third category specified in -[TFLAudioClassifierTests
  // validateClassificationResultForInferenceWithFloatBuffer]. This is because in case of inference
  // with audio record involves more native internal conversions to mock the conversions done by the
  // audio record as opposed to inference with float buffer where the number of native conversions
  // are fewer. Since each native conversion by `AVAudioConverter` employ strategies to pick samples
  // based on the format specified, the samples passed in for inference in case of float buffer and
  // audio record will be slightly different.
  VerifyCategory(categories[0],
                 0,          // expectedIndex
                 0.957031,   // expectedScore
                 @"Speech",  // expectedLabel
                 nil         // expectedDisplaName
  );
  VerifyCategory(categories[1],
                 500,  // expectedIndex
                 0.019531,               // expectedScore
                 @"Inside, small room",  // expectedLabel
                 nil                     // expectedDisplaName
  );
}

- (void)validateCategoriesForInferenceWithLabelDenyList:(NSArray<TFLCategory *> *)categories {
  VerifyCategory(categories[0],
                 500,  // expectedIndex
                                         0.019531,               // expectedScore
                 @"Inside, small room",  // expectedLabel
                 nil                     // expectedDisplaName
  );
}

- (TFLAudioRecord *)createAudioRecordWithAudioClassifier:(TFLAudioClassifier *)audioClassifier {
  // Create audio record using audio classifier
  TFLAudioRecord *audioRecord = [audioClassifier createAudioRecordWithError:nil];
  XCTAssertNotNil(audioRecord);

  return audioRecord;
}

- (void)testInferenceWithFloatBufferSucceeds {
  TFLAudioClassifier *audioClassifier = [self createAudioClassifierWithModelPath:self.modelPath];

  TFLAudioTensor *audioTensor = [self createAudioTensorWithAudioClassifier:audioClassifier];
  [self loadAudioTensor:audioTensor fromWavFileWithName:@"speech"];

  const NSInteger expectedCategoryCount = 521;
  TFLClassificationResult *classificationResult =
      [self classifyWithAudioClassifier:audioClassifier
                            audioTensor:audioTensor
                  expectedCategoryCount:expectedCategoryCount];

  [self validateClassificationResultForInferenceWithFloatBuffer:classificationResult
                                                                    .classifications[0]
                                                                    .categories];
}

- (void)testInferenceWithAudioRecordSucceeds {
  TFLAudioClassifier *audioClassifier = [self createAudioClassifierWithModelPath:self.modelPath];
  TFLAudioTensor *audioTensor = [self createAudioTensorWithAudioClassifier:audioClassifier];

  TFLAudioRecord *audioRecord = [self createAudioRecordWithAudioClassifier:audioClassifier];
  // Mocks the loading of the internal buffer of audio record when new samples are input from the
  // mic.
  [self mockLoadBufferOfAudioRecord:audioRecord];
  // Load the audioRecord buffer into the audio tensor.
  [audioTensor loadAudioRecord:audioRecord withError:nil];

  const NSInteger expectedCategoryCount = 521;
  TFLClassificationResult *classificationResult =
      [self classifyWithAudioClassifier:audioClassifier
                            audioTensor:audioTensor
                  expectedCategoryCount:expectedCategoryCount];

  [self validateCategoriesForInferenceWithAudioRecord:classificationResult.classifications[0]
                                                          .categories];
}

- (void)testInferenceWithMaxResultsSucceeds {
  const NSInteger maxResults = 3;
  TFLAudioClassifierOptions *options =
      [[TFLAudioClassifierOptions alloc] initWithModelPath:self.modelPath];
  options.classificationOptions.maxResults = maxResults;

  TFLAudioClassifier *audioClassifier = [self createAudioClassifierWithOptions:options];
  TFLAudioTensor *audioTensor = [self createAudioTensorWithAudioClassifier:audioClassifier];
  [self loadAudioTensor:audioTensor fromWavFileWithName:@"speech"];

  TFLClassificationResult *classificationResult = [self classifyWithAudioClassifier:audioClassifier
                                                                        audioTensor:audioTensor
                                                              expectedCategoryCount:maxResults];

  [self validateClassificationResultForInferenceWithFloatBuffer:classificationResult
                                                                    .classifications[0]
                                                                    .categories];
}

- (void)testInferenceWithClassNameAllowListAndDenyListFails {
  TFLAudioClassifierOptions *options =
      [[TFLAudioClassifierOptions alloc] initWithModelPath:self.modelPath];
  options.classificationOptions.labelAllowList = @[ @"Speech" ];
  options.classificationOptions.labelDenyList = @[ @"Inside, small room" ];

  NSError *error = nil;
  TFLAudioClassifier *audioClassifier = [TFLAudioClassifier audioClassifierWithOptions:options
                                                                                 error:&error];
  XCTAssertNil(audioClassifier);
  VerifyError(error,
              expectedTaskErrorDomain,                  // expectedErrorDomain
              TFLSupportErrorCodeInvalidArgumentError,  // expectedErrorCode
              @"INVALID_ARGUMENT: `class_name_allowlist` and `class_name_denylist` are mutually "
              @"exclusive options."  // expectedErrorMessage
  );
}

- (void)testInferenceWithLabelAllowListSucceeds {
  TFLAudioClassifierOptions *options =
      [[TFLAudioClassifierOptions alloc] initWithModelPath:self.modelPath];
  options.classificationOptions.labelAllowList = @[ @"Speech", @"Inside, small room" ];

  NSError *error = nil;
  TFLAudioClassifier *audioClassifier = [TFLAudioClassifier audioClassifierWithOptions:options
                                                                                 error:&error];
  TFLAudioTensor *audioTensor = [self createAudioTensorWithAudioClassifier:audioClassifier];
  [self loadAudioTensor:audioTensor fromWavFileWithName:@"speech"];

  TFLClassificationResult *classificationResult =
      [self classifyWithAudioClassifier:audioClassifier
                            audioTensor:audioTensor
                  expectedCategoryCount:options.classificationOptions.labelAllowList.count];

  [self validateClassificationResultForInferenceWithFloatBuffer:classificationResult
                                                                    .classifications[0]
                                                                    .categories];
}

- (void)testInferenceWithLabelDenyListSucceeds {
  TFLAudioClassifierOptions *options =
      [[TFLAudioClassifierOptions alloc] initWithModelPath:self.modelPath];
  options.classificationOptions.labelDenyList = @[ @"Speech" ];

  NSError *error = nil;
  TFLAudioClassifier *audioClassifier = [TFLAudioClassifier audioClassifierWithOptions:options
                                                                                 error:&error];
  TFLAudioTensor *audioTensor = [self createAudioTensorWithAudioClassifier:audioClassifier];
  [self loadAudioTensor:audioTensor fromWavFileWithName:@"speech"];

  const NSInteger expectedCategoryCount = 520;
  TFLClassificationResult *classificationResult =
      [self classifyWithAudioClassifier:audioClassifier
                            audioTensor:audioTensor
                  expectedCategoryCount:expectedCategoryCount];

  [self validateCategoriesForInferenceWithLabelDenyList:classificationResult.classifications[0]
                                                            .categories];
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
- (void)testCreateAudioClassifierWithNilOptionsFails {
  NSError *error = nil;
  TFLAudioClassifier *audioClassifier = [TFLAudioClassifier audioClassifierWithOptions:nil
                                                                                 error:&error];

  XCTAssertNil(audioClassifier);
  VerifyError(error,
              expectedTaskErrorDomain,                              // expectedErrorDomain
              TFLSupportErrorCodeInvalidArgumentError,              // expectedErrorCode
              @"TFLAudioClassifierOptions argument cannot be nil."  // expectedErrorMessage
  );
}

- (void)testInferenceWithNilAudioTensorFails {
  TFLAudioClassifier *audioClassifier = [self createAudioClassifierWithModelPath:self.modelPath];

  NSError *error = nil;
  TFLClassificationResult *classificationResult = [audioClassifier classifyWithAudioTensor:nil
                                                                                     error:&error];

  XCTAssertNil(classificationResult);
  VerifyError(error,
              expectedTaskErrorDomain,                  // expectedErrorDomain
              TFLSupportErrorCodeInvalidArgumentError,  // expectedErrorCode
              @"audioTensor argument cannot be nil."    // expectedErrorMessage
  );
}
#pragma clang diagnostic pop

@end

NS_ASSUME_NONNULL_END
